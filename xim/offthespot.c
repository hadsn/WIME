// -*- coding:euc-jp -*-
#include "wimexim.h"
#include "lib/log.h"
#include "lib/ut.h"
#include "so/wimeapi.h"

extern Display* Disp;

void preedit_area_pos(Window cl,const IcData* icp);

//ConfigureNotifyがきたとき
//StructureNotifyMaskをセットしたウィンドウを返す
static Window target_window(const IcData* ic)
{
    return ic->Attrs.ClientWindow;
}
//影窓を移動させる
static void move_wime(const IcData* ic,int x UNUSED,int y UNUSED)
{
    preedit_area_pos(MoveWineWindow(ic),ic);
}
    
static int open_ime(CallbackParam* p,bool st)
{
    WimeEnableIme(p->Ic->WimeCxn,st);
    return 0;
}

static void init(CallbackParam* p)
{
    XSelectInput(Disp,p->Ic->Attrs.ClientWindow,StructureNotifyMask);
    SetCompFont(p->Ic);
    move_wime(p->Ic,0,0);
}

static int done_preedit(CallbackParam* p UNUSED)
{
    return 0;
}

//imeに処理されなかったキーは無視する
static bool reject_key(CallbackParam* p UNUSED)
{
    return false;
}

//変換ウィンドウをXNAreaで指定された場所に移動させる
void preedit_area_pos(Window cl,const IcData* icp)
{
    XRectangle rect;

    /* Preedit-AttributeのAreaがなければclの大きさをXから取得する。
       off-the-spotでXNAreaがないことはあるのか？*/
    if(TEST2(icp->Attrs.Defined,IC_PREEDIT_ATTR,IC_AREA)){
	LOG(CH_XIM,LOG_DEBUG,MESG("	area size = preedit-area\n"));
	rect = icp->Attrs.Preedit.Cmn.Area;
    }else{
	LOG(CH_XIM,LOG_DEBUG,MESG("	area size = XGetWindowAttributes()\n"));
	XWindowAttributes at;
	XGetWindowAttributes(Disp,cl,&at);
	rect.x = rect.y = 0;
	rect.width = at.width;
	rect.height = at.height;
    }
    /*???
      なぜかWIME_POS_RECTでは変換ウィンドウが表示されない。しかたないので
      WIME_POS_POINTで位置のみ指定し、影窓でクリッピングとする。
      width,heightを使っていないので上のif文のelse節は現状では意味がないが、
      理由が分かったときのために残しておく。
    */
    WimeSetCompWin(icp->WimeCxn,WIME_POS_POINT,rect.x,rect.y);
    LOG(CH_XIM,LOG_DEBUG,MESG("\tpreedit area (%d,%d) %dx%d\n",rect.x,rect.y,rect.width,rect.height));
}

ConvCallbackFuncs ConvFuncOffTheSpot = {
    .OpenIme =		open_ime,
    .Done =		done_preedit,
    .Draw =		ConvDoNothing,
    .RejectKey =	reject_key,
    .Cleanup =		ConvDoNothing,
    .SetSpotLoc =	ConvDoNothing,
    .Init =		init,
    .TargetWindow =	target_window,
    .MoveWime =		move_wime,
};

//(C) 2009 thomas
