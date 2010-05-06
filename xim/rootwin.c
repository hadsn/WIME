#include "wimexim.h"
#include "so/wimeapi.h"
#include <X11/Xutil.h>

/*
  ic属性のFocusWindowに入力用ウィンドウをつくる。
  rootでFocusWindowは使われていないことが前提だが、たぶん大丈夫か？
*/

extern char *DefaultCompFont;
extern Display* Disp;

static void init(CallbackParam* p)
{
    SetCompFont(p->Ic);
    WimeSetCompWin(p->Ic->WimeCxn,WIME_POS_POINT,0,0);

    int x=0,y=0,h=p->Ic->CompFontHeight,w=h*20;
    p->Ic->Attrs.FocusWindow = XCreateSimpleWindow(Disp,XDefaultRootWindow(Disp),x,y,w,h,0,0,WhitePixel(Disp,XDefaultScreen(Disp)));
    p->Ic->Attrs.Defined |= FLG(IC_FOCUS_WINDOW);

    //前編集窓が動かされたら影窓も動かす
    XSelectInput(Disp,p->Ic->Attrs.FocusWindow,StructureNotifyMask);

    //入力自体はclientで行なうので,前編集ウィンドウにフォーカスが来ないようにする 
    XWMHints *hints = XAllocWMHints(); //0クリアされる(hints->input=False)
    hints->flags = InputHint;
    XSetWMHints(Disp,p->Ic->Attrs.FocusWindow,hints);
    XFree(hints);
}

//StructureNotifyMaskをセットしたウィンドウを返す
static Window target_window(const IcData* ic)
{
    return ic->Attrs.FocusWindow;
}
//影窓を移動させる
static void move_wime(const IcData* ic,int x UNUSED,int y UNUSED)
{
    MoveWineWindow(ic);
    WimeSetCompWin(ic->WimeCxn,WIME_POS_POINT,0,0);
}

static int open_ime(CallbackParam* p,bool st)
{
    WimeEnableIme(p->Ic->WimeCxn,st);

    if(st){
	XMapWindow(Disp,p->Ic->Attrs.FocusWindow);
    }else
	XUnmapWindow(Disp,p->Ic->Attrs.FocusWindow);
    return 0;
}

static void cleanup(CallbackParam* p)
{
    if(p->Ic->Attrs.FocusWindow != 0){
	XDestroyWindow(Disp,p->Ic->Attrs.FocusWindow);
    }
}

static int done_preedit(CallbackParam* p UNUSED)
{
    return 0;
}

static bool reject_key(CallbackParam* p UNUSED)
{
    return true;
}

ConvCallbackFuncs ConvFuncRootInput = {
    .OpenIme =		open_ime,
    .Done =		done_preedit,
    .Draw =		ConvDoNothing,
    .RejectKey =	reject_key,
    .Cleanup =		cleanup,
    .SetSpotLoc =	ConvDoNothing,
    .Init =		init,
    .TargetWindow =	target_window,
    .MoveWime =		move_wime,
};
