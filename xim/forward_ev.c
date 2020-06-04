// -*- coding:euc-jp -*-
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include "wimexim.h"
#include "so/xres.h"
#include "so/wimeapi.h"
#include "lib/log.h"
#include "lib/ut.h"

#define FE_SYNC			1
#define FE_REQ_FILTERING	2
#define FE_REQ_LOOKUPSTR	4

extern ToggleKey* ToggleKeys;
extern Display* Disp;

void pass_to_client(const WxContext* cx,XimForwardEvent r);

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

KeySym get_keysym(const xEvent* ev)
{
    //KeySym ks = XkbKeycodeToKeysym(Disp,pkt->ev.u.u.detail,0,level);
    XKeyEvent xev = { .type=KeyPress, .display=Disp,
	.state=ev->u.keyButtonPointer.state, .keycode=ev->u.u.detail};
    KeySym ks = 0;
    XLookupString(&xev,NULL,0,&ks,NULL);
    return ks;
}

//full-sync methodということでいいのか？
int ForwardEvent(WxContext* cx,XimForwardEvent* pkt)
{
    int sync=0;
    IcData* icp = ArElem(&cx->Ic,pkt->icid-1);
    CallbackParam cp = {icp,cx->Client,(XimImIc*)pkt};

    DEBUGDO(CH_XIM,dump_pkt(pkt,icp));

    /*これまでは送られてきたキーイベントは全部wimeに転送していた。
      変換キーの修飾キーにaltを使っている場合、ooでimeをオンにしようとすると、altを押したときにメニューバーが選択されてしまう。使用上問題はないが、いちいちフォーカスを直さなければならない。うっとうしいので、修飾キー単体のイベントは無視する。
      [3.3.2]シフトキーを除く(一時英数モード解除のため)
    */
    unsigned state = pkt->ev.u.keyButtonPointer.state;
    int level = (state & ShiftMask) ? 1 : 0;
    KeySym ks = get_keysym(&(pkt->ev)); //r269
    if((ks==XK_Shift_L||ks==XK_Shift_R) || !IsModifierKey(ks)){
	if(IsToggleKey(ToggleKeys,ks,state)){
	    //変換キーを押した
	    if(!(icp->Flags & ICF_CB_INIT)){
		icp->ConvFunc->Init(&cp);
		icp->Flags |= ICF_CB_INIT;
	    }
	    sync = icp->ConvFunc->OpenIme(&cp,(icp->Flags ^= ICF_IME_ENABLE) & ICF_IME_ENABLE);
	    DEBUGLOG(CH_XIM,"kanji %s\n",(icp->Flags & ICF_IME_ENABLE)?"ON":"OFF");
	}else{
	    if(pkt->ev.u.keyButtonPointer.state == AUX_INPUT_MOD){
		//[atok]パレットからの入力
		char* u8 = WimeGetResultStr(icp->WimeCxn);
		char* ej = U8ToEj(NULL,u8);
		CommitChar(cx->Client,pkt->imid,pkt->icid,ej);
		DEBUGLOG(CH_XIM,"aux input,result str(euc-jp)=[%*D]\n",strlen(ej),ej);
		free(ej);
		free(u8);
	    }else if(icp->Flags & ICF_IME_ENABLE){
		//漢字変換
		DEBUGLOG(CH_XIM,"scancode 0x%hhx --> keysym 0x%x\n",pkt->ev.u.u.detail,ks);
		char* u8;
		KeySym ks1 = XkbKeycodeToKeysym(Disp,pkt->ev.u.u.detail,1,level);
		if(WimeSendKey(icp->WimeCxn,ks,ks1,state,&u8) > 0){
		    char* ej = U8ToEj(NULL,u8);
		    free(u8);
		    if(ej != NULL){ //確定された
			DEBUGLOG(CH_XIM,"result:%s\n",ej);
			sync = icp->ConvFunc->Done(&cp);
			CommitChar(cx->Client,pkt->imid,pkt->icid,ej);
			free(ej);
		    }else{ //入力途中
			icp->ConvFunc->Draw(&cp);
		    }
		}else{
		    //imeに処理されなかったのでクライアントに返す。
		    //(未入力状態でbsを押したときなど)
		    DEBUGLOG(CH_XIM,"\tdo not proc ime\n");
		    if(icp->ConvFunc->RejectKey(&cp))
			pass_to_client(cx,*pkt);
		}
	    }else{
		//漢字offなので、送られたキーをクライアントにそのまま返す
		DEBUGLOG(CH_XIM,"\tthrough\n");
		pass_to_client(cx,*pkt);
	    }
	}
    }
    SendW(cx->Client,XIM_SYNC_REPLY,pkt->imid,pkt->icid);
    return sync;
}

//パケットをクライアントに送り返す
void pass_to_client(const WxContext* cx,XimForwardEvent r)
{
    r.flag = 0;
    SendN(cx->Client,XIM_FORWARD_EVENT,&r,sizeof(r));
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
    pass_to_client(cx,*pkt);
    SendW(cx->Client,XIM_SYNC_REPLY,pkt->imid,pkt->icid);
    return 0;
}

//(C) 2009 thomas
