// -*- coding:euc-jp -*-
#include "wimexim.h"
#include "lib/log.h"
#include "lib/ut.h"
#include "so/wimeapi.h"

/*
  imeの変換ウィンドウを使う
*/

/* ???
  ktermのXNClientWindowのConfigureNotifyがとれないのはなぜ？
  サンプルプログラムだときちんとイベントが送られるのだが。
  ウィンドウを動かしたときのイベントが補足できれば、そのときに影窓と変換ウィンドウを
  移動させることができる。が、今のところ原因が分からないので、XNSpotLocationが
  来たときに影窓と変換ウィンドウを移動させることにする。
*/

static void spot_loc(const CallbackParam* p,const XPoint* pos);

static int open_ime(CallbackParam* p,bool st)
{
    WimeEnableIme(p->Ic->WimeCxn,st);
    return 0;
}

static void init(CallbackParam* p)
{
    /*
      初回の影窓の移動。クライアントウィンドウを動かしたときはXNSpotLocationが
      来たときに行う。
    */
    SetCompFont(p->Ic);
    MoveWineWindow(p->Ic);
    spot_loc(p,&p->Ic->Attrs.Preedit.SpotLocation);
}

static int done_preedit(CallbackParam* p UNUSED)
{
    return 0;
}

static bool reject_key(CallbackParam* p UNUSED)
{
    return true;
}

/*
  変換ウィンドウをspot-locationで指定された位置に移動させる。
*/
static void spot_loc(const CallbackParam* p,const XPoint* pos)
{
    if(p->Ic->CompFontHeight > 0){ //Initが呼ばれてから
	MoveWineWindow(p->Ic);

	//[r18]キャレットの位置設定
	WimeSetCandWin(p->Ic->WimeCxn,WIME_POS_POINT,pos->x,pos->y);

	//yはベースラインなので、変換ウィンドウフォントの高さを引いておく。
	int y = pos->y - p->Ic->CompFontHeight +2; //[r49]Xフォントが使えなくなったことによる位置調整
	if(y < 0)
	    y = 0;
	WimeSetCompWin(p->Ic->WimeCxn,WIME_POS_POINT,pos->x,y);
	DEBUGLOG(CH_XIM,"	composition window pos (%d,%d)\n",pos->x,y);
    }
}

ConvCallbackFuncs ConvFuncOverTheSpot = {
    .OpenIme =		open_ime,
    .Done =		done_preedit,
    .Draw =		ConvDoNothing,
    .RejectKey =	reject_key,
    .Cleanup =		ConvDoNothing,
    .SetSpotLoc =	spot_loc,
    .Init =		init,
    .TargetWindow =	(typeof(ConvFuncOverTheSpot.TargetWindow))ConvDoNothing,
    .MoveWime =		ConvDoNothing,
};

//(C) 2009 thomas
