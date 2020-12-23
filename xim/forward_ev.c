// -*- coding:euc-jp -*-
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include "so/xres.h"
#include "wimexim.h"
#include "lib/log.h"
#include "lib/ut.h"

#define FE_SYNC			1
#define FE_REQ_FILTERING	2
#define FE_REQ_LOOKUPSTR	4

extern ToggleKey* ToggleKeys;
extern Display* Disp;

void dump_pkt(const XimForwardEvent* pkt,const IcData* icp)
{
    MESG("im-id=%hd ic-id=%hd flag=0x%x s/n=%hd time=0x%x wime-cxn=%d\n",
	 pkt->imid,pkt->icid,pkt->flag,pkt->sn,(unsigned)pkt->ev.u.keyButtonPointer.time,
	 icp->WimeCxn);
    MESG("\ttype=0x%hhx detail=0x%hhx sqn=0x%hx state=0x%hx same-screen=%d\n",
	 pkt->ev.u.u.type,pkt->ev.u.u.detail,pkt->ev.u.u.sequenceNumber,
	 pkt->ev.u.keyButtonPointer.state,pkt->ev.u.keyButtonPointer.sameScreen);
    MESG("\twindow:root=0x%x event=0x%x child=0x%x\n",
	 (unsigned)pkt->ev.u.keyButtonPointer.root,(unsigned)pkt->ev.u.keyButtonPointer.event,
	 (unsigned)pkt->ev.u.keyButtonPointer.child);
    MESG("\tpointer:root=(%hd,%hd) event=(%hd,%hd)\n",
	 pkt->ev.u.keyButtonPointer.rootX,pkt->ev.u.keyButtonPointer.rootY,
	 pkt->ev.u.keyButtonPointer.eventX,pkt->ev.u.keyButtonPointer.eventY);
}

//r269
KeySym get_keysym(unsigned keycode,unsigned state)
{
    //KeySym ks = XkbKeycodeToKeysym(Disp,pkt->ev.u.u.detail,0,level);
    XKeyEvent xev = { .type=KeyPress, .display=Disp, .state=state, .keycode=keycode};
    KeySym ks = 0;
    XLookupString(&xev,NULL,0,&ks,NULL);
    return ks;
}

//WimeFilterKeyのコールバックに渡すデータ
typedef struct{
    WxContext* cx;
    XimImIc* pkt;
    IcData* icp;
    int sync; //ForwardEvent()が返す値
} ForwardData;

void wime_preedit(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    ForwardData* fd = arg;
    CallbackParam cbp = {fd->icp,fd->cx->Client,fd->pkt,u8,si};
    fd->icp->ConvFunc->Draw(&cbp);
}

void wime_convert(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    wime_preedit(u8,si,arg);
}

void wime_commit(const char* u8,void* arg)
{
    ForwardData* fd = arg;
    CallbackParam cbp = {fd->icp,fd->cx->Client,fd->pkt,u8};
    char* ej = U8ToEj(NULL,u8);
    DEBUGLOG(CH_XIM,"result:%s\n",ej);
    fd->sync = fd->icp->ConvFunc->Done(&cbp);
    CommitChar(fd->cx->Client,fd->pkt->imid,fd->pkt->icid,ej);
    free(ej);
}

void wime_conv_start(void* arg)
{
    ForwardData* fd = arg;
    CallbackParam cbp = {fd->icp,fd->cx->Client,fd->pkt};
    fd->icp->ConvFunc->Init(&cbp);
}

__attribute__((constructor))
static void init()
{
    WimePreedit = wime_preedit;
    WimeConvert = wime_preedit;
    WimeCommit = wime_commit;
    WimeConvStart = wime_conv_start;
}

//パケットをクライアントに送り返す
void pass_to_client(const WxContext* cx,const XimImIc* pkt)
{
    int size = sizeof(XimHeader)+pkt->h.len*4;
    char buf[size];
    XimForwardEvent* fe = memcpy(buf,pkt,size);
    fe->flag = 0;
    DEBUGLOG(CH_XIM,"send message #%d size %d\n",(unsigned)(pkt->h.major),size);
    SendN(cx->Client,pkt->h.major,fe,size);
}

//ExtForwardKeyEventからも呼び出すためにForwardEvent()から分離させた。
int ForwardKey(WxContext* cx,XimImIc* pkt,unsigned keycode,unsigned state)
{
    DEBUGLOG(CH_XIM,"im %d ic %d keycode 0x%x state 0x%x\n",pkt->imid,pkt->icid,keycode,state);
    IcData* icp = ArElem(&cx->Ic,pkt->icid-1);
    ForwardData fd = {cx,pkt,icp,0};
    KeySym ks = get_keysym(keycode,state); //r269
    if(!WimeFilterKey(icp->WimeCxn,ToggleKeys,Disp,keycode,ks,state,&fd)){
	if(icp->ConvFunc->RejectKey(icp->WimeCxn))
	    pass_to_client(cx,pkt);
    }
    SendW(cx->Client,XIM_SYNC_REPLY,pkt->imid,pkt->icid);
    return fd.sync;
}

int ForwardEvent(WxContext* cx,XimForwardEvent* pkt)
{
    DEBUGLOG(CH_XIM,"flag 0x%hx\n",pkt->flag);
    return ForwardKey(cx,(XimImIc*)pkt,pkt->ev.u.u.detail,pkt->ev.u.keyButtonPointer.state);
}

//ConvCallbackFuncsで使う関数。何もしない。
void ConvDoNothing()
{
}

/*
  wineのウィンドウをic属性で指定されたウィンドウと同じ位置、大きさにする
  使用したウィンドウを返す。
*/
Window MoveWineWindow(const IcData* icp)
{
    int x,y;
    Window dum,cl;
    XWindowAttributes at;

    //FocusWindowが指定されないときはClientWindowを使う。
    //??? ClientWindowも指定されないときはあるのか？
    if(icp->Attrs.Defined & FLG(IC_FOCUS_WINDOW))
	cl = icp->Attrs.FocusWindow;
    else{
	cl = icp->Attrs.ClientWindow;
	DEBUGLOG(CH_XIM,"\tnone focus window,use client window 0x%lx\n",cl);
    }

    XGetWindowAttributes(Disp,cl,&at);
    XTranslateCoordinates(Disp,cl,XDefaultRootWindow(Disp),0,0,&x,&y,&dum);
    WimeMoveShadowWin(icp->WimeCxn,x,y,at.width,at.height);
    DEBUGLOG(CH_XIM,"\tshadow window 0x%x (%d,%d) %dx%d\n",(unsigned)cl,x,y,at.width,at.height);
    return cl;
}

/*
  変換ウィンドウのフォントをセットする
*/
void SetCompFont(IcData* ic)
{
    extern char* DefaultCompFont;
    ic->CompFontHeight = WimeSetCompFont(ic->WimeCxn,ic->Attrs.Preedit.Cmn.FontSet?:DefaultCompFont,ic->Attrs.Preedit.Cmn.Background);
}

/*
  root window入力で前編集窓を動かした,off the spot入力でアプリケーションを動かしたときに
  ConfigureNotifyが送られてきた
*/
void MoveInputWindow(const XConfigureEvent* ev)
{
    IcData* ic=NULL;
    extern Array ContextList;

    //ev->windowからIcDataを探す
    for(int x=0; x<ArUsing(&ContextList); ++x){
	WxContext* wc = ArElem(&ContextList,x);
	if(!(wc->Flags & (IMF_INVALID|IMF_CLOSE))){
	    for(int y=0; y<ArUsing(&wc->Ic); ++y){
		IcData* chk_ic = ArElem(&wc->Ic,y);
		if(!(chk_ic->Flags & ICF_INVALID) &&
		   chk_ic->ConvFunc &&
		   chk_ic->ConvFunc->TargetWindow(chk_ic)==ev->window){
		    ic = chk_ic;
		    break;
		}
	    }
	}
    }

    if(ic != NULL){
	ic->ConvFunc->MoveWime(ic,ev->x,ev->y);
    }
}


int ForwardEvent_nwm(WxContext* cx,XimForwardEvent* pkt)
{
    pass_to_client(cx,(XimImIc*)pkt);
    SendW(cx->Client,XIM_SYNC_REPLY,pkt->imid,pkt->icid);
    return 0;
}

//(C) 2009 thomas
