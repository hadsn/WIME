// -*- coding:euc-jp -*-
#include "wimexim.h"
#include "lib/log.h"
#include "lib/ut.h"
#include "so/wimeapi.h"
#include <string.h>
#include <stdlib.h>

/*
  XIM_PREEDIT_STARTを送るのは文字が入力されたときにしようと思ったが、
  XIM_PREEDIT_STARTを送ったらXIM_PREEDIT_START_REPLYを受け取る必要がある。
  しかし現在の構造ではイベントを受け取るためにメインループに戻らなければならない。
  そのためこの関数でXIM_PREEDIT_STARTを送ることにする。
  文字列確定後続けて入力する場合、確定後にXIM_PREEDIT_DONE,XIM_PREEDIT_STARTを続けて送ることにする。
  常に変換開始状態になっているわけで、今ひとつすっきりしない。できれば入力を始めたらXIM_PREEDIT_START,入力が終わったらXIM_PREEDIT_DONEにしたいが、入力してからイベントを送ったらレスポンスが悪くなるか？
  そもそもカーソル位置などの情報を取得できないので、候補ウィンドウを適切な場所に表示することができない。on-the-spotに対する処理は深く考えないことにする。
*/

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    int32_t	caret;
    int32_t	chg_first;
    int32_t	chg_length;
    int32_t	status;
    int16_t	str_len;
    char	str[0];
    //		pad(2+len)
}__attribute__((packed)) XimPreeditDraw1;

typedef struct{
    int16_t	feedback_len;
    int16_t	dum;
    int32_t	feedback[0];
}__attribute__((packed)) XimPreeditDraw2;

#define PREEDIT_DRAW_NO_STR	1
#define PREEDIT_DRAW_NO_FB	2

extern Display* Disp;

static int open_ime(CallbackParam* p,bool st)
{
    int code,sync;
    if(st){
	code = XIM_PREEDIT_START;
	sync = XIM_PREEDIT_START_REPLY;
    }else{
	code = XIM_PREEDIT_DONE;
	sync = 0;
    }
    WimeEnableIme(p->Ic->WimeCxn,st);
    SendW(p->Client,code,p->Pkt->imid,p->Pkt->icid);
    return sync;
}

//winの一番外側のウィンドウを返す。
Window app_window(Window win)
{
    Window root, parent=win;
    Status st=1;
    do{
	Window* ch;
	unsigned nch;
	win = parent;
	st = XQueryTree(Disp,win,&root,&parent,&ch,&nch);
	//DEBUGLOG(CH_XIM,"0:%d %x %x %x\n",st,win,parent,root);
	XFree(ch);
    }while(st && parent!=root);
    return st ? win : 0;
}

//winの一番外側のウィンドウの位置と大きさを返す。NULLであれば返さない。
bool app_geo(Window win,int* x,int* y,int* w,int* h)
{
    bool st = false;
    win = app_window(win);
    if(!win){
	DEBUGLOG(CH_XIM,"invalid window\n");
    }else{
	XWindowAttributes at;
	Window ch;
	int dum;
	if(x == NULL)
	    x = &dum;
	if(y == NULL)
	    y = &dum;
	if(XGetWindowAttributes(Disp,win,&at) &&
	   XTranslateCoordinates(Disp,win,XDefaultRootWindow(Disp),0,0,x,y,&ch)){
	    if(w)
		*w = at.width;
	    if(h)
		*h = at.height;
	    st = true;
	}else{
	    DEBUGLOG(CH_XIM,"cannot get client window position\n");
	}
    }
    return st;
}

static void draw(CallbackParam* cbp)
{
    int ctlen=0;
    char* ct=NULL;

    DEBUGLOG(CH_XIM,"%d %d %d %d %d %U\n",cbp->si->CursorPos,cbp->si->DeltaStart,cbp->si->TargetClause,cbp->si->TargetClLen,cbp->si->Length,cbp->u8);

    if(cbp->u8!=NULL){
	char* ej = U8ToEj(NULL,cbp->u8);
	ct = EucjpToCtext(ej);
	ctlen = strlen(ct);
	free(ej);
    }

    int d1size = sizeof(XimPreeditDraw1)+ctlen+Pad(2+ctlen);
    int pktsize = d1size+sizeof(XimPreeditDraw2)+cbp->si->Length*MEMBERSIZE(XimPreeditDraw2,feedback[0]);
    XimPreeditDraw1* d1 = malloc(pktsize);
    XimPreeditDraw2* d2 = (XimPreeditDraw2*)((char*)d1 + d1size);
    d1->imid = cbp->Pkt->imid;
    d1->icid = cbp->Pkt->icid;
    d1->caret = cbp->si->CursorPos;
    d1->chg_first = 0;

    //前編集バッファを空にする
    if(cbp->Ic->PreeditLen > 0){
	d1->chg_length = cbp->Ic->PreeditLen;
	d1->status = PREEDIT_DRAW_NO_STR|PREEDIT_DRAW_NO_FB;
	SendN(cbp->Client,XIM_PREEDIT_DRAW,d1,pktsize);
    }

    if(ct != NULL){
	//バッファ全体を置き換える
	d1->chg_length = 0; //si.Length;	
	d1->status = 0;
	memcpy(d1->str,ct,d1->str_len=ctlen);
    
	d2->feedback_len = sizeof(d2->feedback[0])*cbp->si->Length;
	for(int x=0; x<cbp->si->Length; ++x)
	    d2->feedback[x]=XIMUnderline;
	if(cbp->si->TargetClause != -1){
	    for(int x=0; x<cbp->si->TargetClLen; ++x)
		d2->feedback[cbp->si->TargetClause+x] = XIMReverse;
	}
	SendN(cbp->Client,XIM_PREEDIT_DRAW,d1,pktsize);

	cbp->Ic->PreeditLen = cbp->si->Length;
    }
    free(ct);
    free(d1);

    //アプリケーションの下に候補ウィンドウを置く
    int x=0,y=0,h=0;
    app_geo(cbp->Ic->Attrs.ClientWindow,&x,&y,NULL,&h);
    WimeSetCandWin(cbp->Ic->WimeCxn,WIME_POS_POINT,x,y+h);
}

static int done_preedit(CallbackParam* p)
{
    if(p->Ic->PreeditLen > 0){
	/* ooでは前編集文字列を消去しなければこれとcommitで二重に入力されてしまう。
	   leafpadでは問題ないんだが。*/
	int bufsize = sizeof(XimPreeditDraw1)+Pad(2)+sizeof(XimPreeditDraw2);
	char buf[bufsize];
	XimPreeditDraw1* d1 = memset(buf,0,bufsize);
	d1->imid = p->Pkt->imid;
	d1->icid = p->Pkt->icid;
	d1->chg_length = p->Ic->PreeditLen;
	d1->status = PREEDIT_DRAW_NO_STR|PREEDIT_DRAW_NO_FB;
	SendN(p->Client,XIM_PREEDIT_DRAW,d1,bufsize);
    }
    p->Ic->PreeditLen = 0;
    SendW(p->Client,XIM_PREEDIT_DONE,p->Pkt->imid,p->Pkt->icid);
    SendW(p->Client,XIM_PREEDIT_START,p->Pkt->imid,p->Pkt->icid);
    return XIM_PREEDIT_START_REPLY;
}

static bool reject_key(int wimecxn)
{
    /*
      前編集中のときにXIM_FORWARD_EVENTを送り返すとbad protocolになってしまう。
      gtkのときだけか？ これを避けるために、前編集文字列がないときだけ送り返す。
    */
    char* cmp = WimeGetCompStr(wimecxn,NULL);
    bool st = (cmp==NULL);
    free(cmp);
    return st;
}

static void init(CallbackParam* cbp)
{
    WimeShowToolbar(cbp->Ic->WimeCxn,true,false);

    /*!!!
      カーソル位置を取得できないため影窓を左上にしておく。
      大きさはそのままにしておく。ひょっとしたら元の大きさが小さすぎて候補ウィンドウが
      表示されないかもしれない。
    */
    WimeMoveShadowWin(cbp->Ic->WimeCxn,0,0,-1,-1);
}

ConvCallbackFuncs ConvFuncOnTheSpot = {
    .OpenIme =		open_ime,
    .Done =		done_preedit,
    .Draw =		draw,
    .RejectKey =	reject_key,
    .Cleanup =		ConvDoNothing,
    .SetSpotLoc =	ConvDoNothing,
    .Init =		init,
    .TargetWindow =	(typeof(ConvFuncOnTheSpot.TargetWindow))ConvDoNothing,
    .MoveWime =		ConvDoNothing,
};

//(C) 2009 thomas
