// -*- coding:euc-jp -*-
#ifndef WIME_XIM_WIMEXIM
#define WIME_XIM_WIMEXIM

#include <X11/Xlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "lib/array.h"
#include "x.h"
#include "so/wimeapi.h"

#define PACKET_MAX_SIZE MEMBERSIZE(XEvent,xclient.data.b)
#define FLG(x) (1<<(x))
#define TEST2(v,x,y) (((v) & (FLG(x)|FLG(y)))==(FLG(x)|(FLG(y))))

//im属性の番号(ビット位置としても使う）
enum{
    IM_INPUT_STYLE,
};

//ic属性の番号(ビット位置としても使う）
enum{
    IC_INPUT_STYLE,
    IC_CLIENT_WINDOW,
    IC_FOCUS_WINDOW,
    IC_FILTER_EVENTS,
    IC_STRING_CONV,
    IC_RESET_STATE,
    IC_HOTKEY_STATE,
    IC_PREEDIT_ATTR,
    IC_STATUS_ATTR,

    IC_AREA,
    IC_AREA_NEEDED,
    IC_COLOR_MAP,
    IC_FG,
    IC_BG,
    IC_BG_PIXMAP,
    IC_FONTSET,
    IC_LINE_SPACE,
    IC_CURSOR,

    IC_SPOT_LOC,
    IC_STATE,

    IC_SEP
};

//PreeditAttrとStatusAttrの共通項目
typedef struct{
    XRectangle	Area;
    XRectangle	AreaNeeded;
    Colormap	ColorMap;
    unsigned	Foreground;
    unsigned	Background;
    Pixmap	BgPixmap;
    char*	FontSet;
    unsigned	LineSpace;
    Cursor	CursorId;
} CommonAttr;

typedef struct{
    CommonAttr	Cmn;
    XPoint	SpotLocation;
    unsigned	State;
} PreeditAttr;

typedef struct{
    CommonAttr	Cmn;
} StatusAttr;

typedef struct{
    unsigned		Defined;
    unsigned		InputStyle;
    Window		ClientWindow;
    Window		FocusWindow;
    unsigned		FilterEvents;
    XIMStringConversionText StrConv;
    XIMResetState	ResetState;
    XIMHotKeyTriggers	Hotkey;
    XIMHotKeyState	HotkeyState;
    PreeditAttr		Preedit;
    StatusAttr		StatusArea;
} IcAttributes;

typedef struct IcData_s IcData;

//入力方法
typedef struct{
    IcData* Ic;
    Window Client;
    const XimImIc* Pkt;

    const char* u8;
    const WimeCompStrInfo* si;
} CallbackParam;

typedef struct{
    void (*Init)(CallbackParam* p);
    int (*OpenIme)(CallbackParam* p,bool);
    int (*Done)(CallbackParam*);
    void (*Draw)(CallbackParam*);
    bool (*RejectKey)(int); //trueならキーをクライアントに返す
    void (*Cleanup)(CallbackParam*); //DestroyIcで呼びだされる
    void (*SetSpotLoc)(const CallbackParam*,const XPoint*); //over-the-spotのみ

    Window (*TargetWindow)(const IcData*);
    void (*MoveWime)(const IcData*,int x,int y);
} ConvCallbackFuncs;

struct IcData_s {
    IcAttributes Attrs;
    unsigned Flags;
    int WimeCxn; //wimeのコンテキスト番号
    int CompFontHeight; //変換ウィンドウフォントの高さ。未取得=-1,エラー=0
    ConvCallbackFuncs *ConvFunc; //on-the-spot,over-the-spotなど
    int PreeditLen; //現在の前編集文字列の長さ
    int ExtPosX,ExtPosY; //xim_ext_move
};    

typedef struct{
    Window Proxy;
    Window Client;
    int Sync;
    int Flags;
    Array Ic; //IcDataの配列  Ic[icid-1]
    char* Encoding; //デフォルト(ctext)のときNULL
} WxContext;

typedef struct{
    XimAttrType Type;
    const char* Name;
    int Number;
    int Offset;
    int (*Getter)(char* base,char** a,uint16_t* idlist,int idlen);
    int (*Setter)(void* adr,Attribute* a,const CallbackParam*);
} Attrs_t;

//IcDataのフラグ
#define ICF_IME_ENABLE		1	//ステータスウィンドウを表示している(IcData)
#define ICF_SPOT_LOC		8	//変換ウィンドウを移動させる(IcData)
#define ICF_INVALID		4	//未使用状態
#define ICF_CB_INIT		2	//ConvCallbackFuncs->Initを呼んだ
#define ICF_HAVE_FOCUS		0x10	//フォーカスを持っている
#define ICF_MAKE_FOCUSWIN	0x20	//rootwin.cでfocuswindowをつくった。

//WxContextのフラグ
#define IMF_EXT_SET_EV_MASK	2	//ExtSetEventMaskを使う
#define IMF_INVALID		4	//未使用状態
#define IMF_BADWINDOW		8	//BadWindowがおきた
#define IMF_CLOSE		0x10	//クローズした

void SendN(Window client,unsigned major,void* h,int size);
void Send0(Window win,unsigned mj);
void SendW(Window win,unsigned mj,uint16_t p1,uint16_t p2);

void CommitChar(Window client,uint16_t imid,uint16_t icid,const char* ch);
void DisconnectClient(WxContext* cx,bool send_reply);
char* EucjpToCtext(const char* ej);
void ConvDoNothing(); //ConvCallbackFuncs用
void MoveInputWindow(const XConfigureEvent* ev);
Window MoveWineWindow(const IcData* icp); //影窓を移動
void SetCompFont(IcData* ic);
void DestroyIcIf(WxContext* cx,XimImIc* pkt,bool send_reply,bool enable_wime);

int Open(WxContext* pl,XimOpen* pkt);
int Close(WxContext*,XimClose* pkt);
int Error(WxContext*,XimError* pkt);
int QueryExtension(WxContext*,XimQueryExtension* pkt);
int EncodingNego(WxContext*,XimEncodingNego* pkt);
int Connect(WxContext*,XimConnect* pkt);
int Disconnect(WxContext*);
int GetImValues(WxContext*,XimGetImValues* pkt);
int CreateIc(WxContext* pl,XimCreateIc* pkt);
int SyncReply(WxContext* pl,XimImIc* pkt);
int ForwardEvent(WxContext* cx,XimForwardEvent* pkt);
int TriggerNotify(WxContext* cx,XimTriggerNotify* pkt);
int DestroyIc(WxContext* cx,XimImIc* pkt);
int SetIcValues(WxContext* cx,XimSetIcValues* pkt);
int GetIcValues(WxContext* cx,XimGetIcValues* pkt);
int SetIcFocus(WxContext* cx,XimImIc* pkt);
int UnsetIcFocus(WxContext* cx,XimImIc* pkt);
int PreeditStartReply(WxContext* cx,XimPreeditStartReply* pkt);
int ResetIc(WxContext* cx,XimImIc* pkt);
void SetWimeData(IcData* ic);
int ExtMove(WxContext* cx,XimExtMove* pkt);
int ExtForwardKeyEvent(WxContext* cx,XimExtForwardKeyEvent* pkt);
int ExtForwardKeyEvent_nwm(WxContext* cx,XimExtForwardKeyEvent* pkt);
int ExtSetEventMask(WxContext* cx,XimExtSetEventMask* pkt);
int ForwardEvent_nwm(WxContext* cx,XimForwardEvent* pkt);
int CreateIc_nwm(WxContext* cx,XimCreateIc* pkt);
int DestroyIc_nwm(WxContext* cx,XimImIc* pkt);
int SetIcFocus_nwm(WxContext* cx,XimImIc* pkt);
int UnsetIcFocus_nwm(WxContext* cx,XimImIc* pkt);
int Disconnect_nwm(WxContext* cx);

int ForwardKey(WxContext* cx,XimImIc* pkt,unsigned keycode,unsigned state);

#endif

//(C) 2009 thomas
