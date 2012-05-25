#include "wimexim.h"
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
    send_ww(p->Client,code,p->Pkt->imid,p->Pkt->icid);
    return sync;
}

static void draw(CallbackParam* p)
{
    XimPreeditDraw1 *d1;
    XimPreeditDraw2 *d2;
    WimeCompStrInfo si;
    int d1size,pktsize,ctlen;
    char *ej,*ct;

    ej = WimeGetCompStr(p->Ic->WimeCxn,&si);
    LOG("%d %d %d %d %d %s\n",si.CursorPos,si.DeltaStart,si.TargetClause,si.TargetClLen,si.Length,ej);

    if(ej!=NULL){
	ct = EucjpToCtext(ej);
	ctlen = strlen(ct);
    }else{
	ct = NULL;
	ctlen = 0;
    }
    d1size = sizeof(*d1)+ctlen+Pad(2+ctlen);
    pktsize = d1size+sizeof(*d2)+si.Length*sizeof(d2->feedback[0]);

    d1 = malloc(pktsize);
    d2 = (XimPreeditDraw2*)((char*)d1 + d1size);
    d1->imid = p->Pkt->imid;
    d1->icid = p->Pkt->icid;
    d1->caret = si.CursorPos;
    d1->chg_first = 0;

    //前編集バッファを空にする
    if(p->Ic->PreeditLen > 0){
	d1->chg_length = p->Ic->PreeditLen;
	d1->status = PREEDIT_DRAW_NO_STR|PREEDIT_DRAW_NO_FB;
	send_n(p->Client,XIM_PREEDIT_DRAW,d1,pktsize);
    }

    if(ej != NULL){
	//バッファ全体を置き換える
	d1->chg_length = 0; //si.Length;	
	d1->status = 0;
	memcpy(d1->str,ct,d1->str_len=ctlen);
    
	d2->feedback_len = sizeof(d2->feedback[0])*si.Length;
	for(int x=0; x<si.Length; ++x)
	    d2->feedback[x]=XIMUnderline;
	if(si.TargetClause != -1){
	    for(int x=0; x<si.TargetClLen; ++x)
		d2->feedback[si.TargetClause+x] = XIMReverse;
	}
	send_n(p->Client,XIM_PREEDIT_DRAW,d1,pktsize);

	p->Ic->PreeditLen = si.Length;

	free(ej);
    }
    free(ct);
    free(d1);
}

static int done_preedit(CallbackParam* p)
{
    if(p->Ic->PreeditLen > 0){
	/* ooでは前編集文字列を消去しなければこれとcommitで二重に入力されてしまう。
	   leafpadでは問題ないんだが。*/
	int bufsize = sizeof(XimPreeditDraw1)+Pad(2)+sizeof(XimPreeditDraw2);
	char buf[bufsize];
	XimPreeditDraw1 *d1 = memset(buf,0,bufsize);
	d1->imid = p->Pkt->imid;
	d1->icid = p->Pkt->icid;
	d1->chg_length = p->Ic->PreeditLen;
	d1->status = PREEDIT_DRAW_NO_STR|PREEDIT_DRAW_NO_FB;
	send_n(p->Client,XIM_PREEDIT_DRAW,d1,bufsize);
    }
    p->Ic->PreeditLen = 0;
    send_ww(p->Client,XIM_PREEDIT_DONE,p->Pkt->imid,p->Pkt->icid);
    send_ww(p->Client,XIM_PREEDIT_START,p->Pkt->imid,p->Pkt->icid);
    return XIM_PREEDIT_START_REPLY;
}

static bool reject_key(CallbackParam* p UNUSED)
{
    /*
      前編集中のときにXIM_FORWARD_EVENTを送り返すとbad protocolになってしまう。
      gtkのときだけか？ これを避けるために、前編集文字列がないときだけ送り返す。
    */
    WimeCompStrInfo si;
    char *cmp = WimeGetCompStr(p->Ic->WimeCxn,&si);
    bool st = (cmp==NULL);
    free(cmp);
    return st;
}

static void init(CallbackParam* p)
{
    WimeShowToolbar(p->Ic->WimeCxn,true,false);

    /*!!!
      カーソル位置を取得できないため影窓を左上にしておく。
      大きさはそのままにしておく。ひょっとしたら元の大きさが小さすぎて候補ウィンドウが
      表示されないかもしれない。
    */
    WimeMoveShadowWin(p->Ic->WimeCxn,0,0,-1,-1);
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
