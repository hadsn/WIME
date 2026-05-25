
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <ctype.h>
#include <getopt.h>
#include "wimexim.h"
#include "so/xres.h"
#include "so/wimeapi.h"
#include "lib/link.h"
#include "lib/wimeconn.h"
#include "lib/log.h"
#include "lib/ut.h"
#include "lib/cmdlineopt.h"
#include "lib/printf.h"

ToggleKey* ToggleKeys; //変換トグルキーとシフト状態
char* DefaultCompFont;	//変換ウィンドウのフォント

#define SERVERNAME "wime"

void on_selection_request(Window win, const XSelectionRequestEvent* ev);
void on_client_message(Window win, XClientMessageEvent* ev);
Window make_server(int ac, char* av[]);
void context_list_cr(void* p);
Window add_proxy(Window c);
void destroy_client(const XDestroyWindowEvent* ev);
void reset_req_func_tab(bool enable_wime);
static void restart_server(void);
int cl_opt(int ac, char* av[]);

static Window ServerWin;

//windowとXimHeaderからWxContextを探す
WxContext* none_imic(Window, const XimHeader*, int*, int*);
WxContext* have_imic(Window, const XimHeader*, int*, int*);
WxContext* have_im(Window, const XimHeader*, int*, int*);

enum {
    WIMEXIM_PROP,	// _XIM_WIMEXIM_PROP
    XIM_PROTOCOL,	// _XIM_PROTOCOL
    SERVER,		// @server=wime
    XIM_SERVERS,	// XIM_SERVERS

    //この３つは先に作っておかないとSelectionRequestが飛んでこないみたい
    XIM_XCONNECT,	// _XIM_XCONNECT
    LOCALES,		// LOCALES
    TRANSPORT,		// TRANSPORT

    RESTART_WIME,	//サーバーが再起動したときにclient messageを送る。
    ATOM_MAX
};
static Atom Atm[ATOM_MAX];

Array ContextList; //WxContext[im-id]
Display* Disp;

int (*original_error_handler)(Display* disp, XErrorEvent* e);
int match_client(void* elem, void* arg) //Clientウィンドウを探す
{
    return ((WxContext*)elem)->Client == *(Window*)arg ? 1 : 0;
}
int x_error(Display* disp, XErrorEvent* e)
{
    if (e->error_code == BadWindow) {
        /*なぜ先にClientが閉じられるのか？原因が分からないので、とにかくこのエラーを補足し、
          proc_client_message()でフラグを確認する。*/
        int index = ArForEach(&ContextList, match_client, &(e->resourceid));
        if (index > 0) {
            ERRORLOG(CH_GLOBAL, "bad window 0x%lx,mark close.\n", e->resourceid);
            //??? IMF_INVALIDをつけると再利用されるが、再利用後に以前のメッセージが来たりしないか？
            ((WxContext*)ArElem(&ContextList, index))->Flags |= IMF_BADWINDOW | IMF_INVALID;
        }
        else {
            ERRORLOG(CH_GLOBAL, "bad window 0x%lx,not found.\n", e->resourceid);
        }
        return 0;
    }
    return (*original_error_handler)(disp, e);
}

int main(int ac, char* av[])
{
    XEvent ev;
    const char* atom_str[] = {
        "_XIM_WIMEXIM_PROP",
        "_XIM_PROTOCOL",
        "@server=" SERVERNAME,
        "XIM_SERVERS",
        "_XIM_XCONNECT",
        "LOCALES",
        "TRANSPORT",
        "restart_wime"
    };

    CustomPrintf();
    if (setlocale(LC_ALL, "") == NULL) {
        ERR("cannot set locale\n");
        return 1;
    }
    if (!XSupportsLocale()) {
        ERR("not support locale\n");
        return 1;
    }

    Disp = XOpenDisplay(NULL);
    original_error_handler = XSetErrorHandler(x_error);

    int socket_num = cl_opt(ac, av);
    if (socket_num < 0)
        return 1;
    if (WimeInitialize(socket_num, 'x') < 0) {
        ERR("cannot connect wime\n");
    }
    WimeRestartSignal(restart_server);

    //オプションのソケット番号があればサーバー名にも追加する。
    if (socket_num > 0) {
        char buf[strlen(atom_str[SERVER]) + 10]; //追加ソケットは最大0xffffなので5文字あればいい。
        sprintf(buf, "%s%d", atom_str[SERVER], socket_num);
        atom_str[SERVER] = strdup(buf);
    }

    InitDatabase(Disp, "xim");
    ToggleKeys = GetConvKeyFromResource(Disp);
    DefaultCompFont = GetCompFont(Disp);
    ArNew(&ContextList, sizeof(WxContext), context_list_cr);
    for (int i = 0; i < ATOM_MAX; ++i)
        Atm[i] = XInternAtom(Disp, atom_str[i], False);
    ServerWin = make_server(ac, av);

    while (1) {
        XNextEvent(Disp, &ev);
        switch (ev.type) {
        case SelectionRequest:
            on_selection_request(ServerWin, (XSelectionRequestEvent*)&ev);
            break;
        case ClientMessage:
            on_client_message(ServerWin, (XClientMessageEvent*)&ev);
            break;
        case DestroyNotify:
            if (((XDestroyWindowEvent*)&ev)->window == ServerWin)
                goto fin;
            destroy_client((XDestroyWindowEvent*)&ev);
            break;
        case ConfigureNotify: //root window入力で前編集窓を動かした時
            MoveInputWindow((XConfigureEvent*)&ev);
            break;
        case MappingNotify:
            XRefreshKeyboardMapping((XMappingEvent*)&ev);
            break;
#if 0
        default:
            MESG("EVENT:%d\n", ev.type);
#endif
        }
    }
fin:
    WimeFinalize();
    XCloseDisplay(Disp);
    return 0;
}

bool do_sync(const char* arg, void* tmp)
{
    XSynchronize(Disp, True);
    return true;
}
int cl_opt(int ac, char* av[])
{
    OptArg oa[] = {
        {NULL,'s',no_argument,	do_sync,NULL,"\tsynchronouse request",NULL}
    };
    return CmdlineOpt(ac, av, oa, ITEMS(oa), NULL);
}

//クライアントウィンドウが閉じられた時
void destroy_client(const XDestroyWindowEvent* ev)
{
    int imid, icid;
    WxContext* cx;

    if ((cx = none_imic(ev->window, NULL, &imid, &icid)) != NULL) {
        DEBUGLOG(CH_XIM, "destroy notify proxy 0x%lx client 0x%lx\n", cx->Proxy, cx->Client);
        DisconnectClient(cx, false);
    }
}

bool proc_client_message(Window win, const XClientMessageEvent* ev, XimHeader* h);
XimHeader* get_message(Window win, const XClientMessageEvent* ev);
void preconnect(const XClientMessageEvent* ev);
static int chk_im(WxContext* wc, void* arg UNUSED);

typedef struct {
    Window win;
    XClientMessageEvent ev;
    XimHeader* pkt; //プロパティ経由のデータのアドレスを入れる。
} QueueData;
static BiLink* EventQ;

void on_client_message(Window win, XClientMessageEvent* ev)
{
    if (ev->message_type == Atm[XIM_XCONNECT]) {
        preconnect(ev);
        return;
    }
    if (ev->message_type == Atm[XIM_PROTOCOL]) {
        QueueData* q;

        XimHeader* h = get_message(ev->window, ev);
        bool st = proc_client_message(win, ev, h);

        //キューに残っているパケットがあれば処理してみる
        BiLink* c = EventQ;
        while (c != NULL) {
            DEBUGLOG(CH_XIM, "check queue\n");
            q = c->obj;
            XimHeader* qh = q->pkt != NULL ? q->pkt : get_message(q->win, &q->ev);
            if (proc_client_message(q->win, &q->ev, qh)) {
                //処理できたので、キューから削除してキュー先頭から再検査
                if ((char*)qh != q->ev.data.b)
                    XFree(qh);
                free(LkRemove(&EventQ, c));
                c = EventQ;
                continue;
            }
            c = c->next;
        }

        if (st) {
            if ((char*)h != ev->data.b) //XGetWindowPropertyできたデータ
                XFree(h);
        }
        else {
            q = malloc(sizeof(QueueData));
            q->win = win;
            q->ev = *ev;
            q->pkt = (char*)h != ev->data.b ? h : NULL;
            LkPushEnd(&EventQ, q);
        }
        return;
    }
    if (ev->message_type == Atm[RESTART_WIME]) {
        //サーバーが再起動した
        ERRORLOG(CH_XIM, "restart server message\n");
        ArForEach(&ContextList, (ArForEachFunc)chk_im, NULL);
        return;
    }

    char* n = XGetAtomName(Disp, ev->message_type);
    ERRORLOG(CH_GLOBAL, "unknown message type %s\n", n);
    XFree(n);
}

/*
  接続前の _XIM_XCONNECTの処理
*/
void preconnect(const XClientMessageEvent* ev)
{
    XEvent ne;
    ne.xclient.type = ClientMessage;
    ne.xclient.window = ev->data.l[0];
    ne.xclient.message_type = Atm[XIM_XCONNECT];
    ne.xclient.format = 32;
    ne.xclient.data.l[0] = add_proxy(ev->data.l[0]);
    ne.xclient.data.l[1] = 0; //only-CM and
    ne.xclient.data.l[2] = 0; // property-with-CM
    ne.xclient.data.l[3] = PACKET_MAX_SIZE; //CM一つ以上の時はプロパティを使う
    XSendEvent(Disp, ne.xclient.window, False, NoEventMask, &ne);
    DEBUGLOG(CH_XIM, "client-id=0x%lx version=%ld/%ld proxy-window=0x%lx\n", ev->data.l[0], ev->data.l[1], ev->data.l[2], ne.xclient.data.l[0]);
    /*
      これ以降_XIM_PROTOCOLでデータが来るわけだが、windowメンバは自
      分（サーバ）になっている。ということは誰に返答すればいいか分
      からない。なので、接続ごとに中継windowをつくることにする。そ
      うするとwindowメンバには中継windowが入っているので、対応表を
      見ればクライアントwindowが分かる。
    */
}

Window make_server(int ac, char* av[])
{
    Atom* data, type;
    int format;
    unsigned long ndata, r;
    const char name[] = SERVERNAME;
    XTextProperty np = { (unsigned char*)name,XA_STRING,8,sizeof(name) - 1 };

    Window root = XDefaultRootWindow(Disp);
    Window win = XCreateSimpleWindow(Disp, root, 0, 0, 1, 1, 0, 0, XWhitePixel(Disp, XDefaultScreen(Disp)));
    DEBUGLOG(CH_XIM, "create display %p window 0x%lx\n", Disp, win);
    XSetWMProperties(Disp, win, &np, &np, av, ac, NULL, NULL, NULL);
    XSelectInput(Disp, win, StructureNotifyMask);//destroyイベントを受ける

    XSetSelectionOwner(Disp, Atm[SERVER], win, CurrentTime);

    //XIM_SERVERSに追加する
    if (XGetWindowProperty(Disp, root, Atm[XIM_SERVERS], 0, 40 * 1024 / 4, False, XA_ATOM, &type, &format, &ndata, &r, (unsigned char**)&data) == Success) {
        unsigned long n;
        for (n = 0; n < ndata && data[n] != Atm[SERVER]; ++n)
            ;
        if (n < ndata) { //すでに登録されている
            data[n] = data[--ndata]; //一番後ろのデータで上書き
            XChangeProperty(Disp, root, Atm[XIM_SERVERS], XA_ATOM, 32, PropModeReplace, (unsigned char*)data, ndata);
        }
        XFree(data);
    }
    XChangeProperty(Disp, root, Atm[XIM_SERVERS], XA_ATOM, 32, PropModeAppend, (unsigned char*)&Atm[SERVER], 1);

    return win;
}

//LOCALESとTRANSPORT
void on_selection_request(Window win, const XSelectionRequestEvent* ev)
{
    DEBUGLOG(CH_XIM, "%s %s %s\n", XGetAtomName(Disp, ev->selection), XGetAtomName(Disp, ev->target), XGetAtomName(Disp, ev->property));

    const char* val = NULL;
    if (ev->target == Atm[LOCALES]) {
        //EUC-JPは認識されなかった。eucjp以外のためにja_JPなども入れておく。
        //→わざわざja_JP.eucJPを指定しなくてもjaだけでいいのか？
        val = "@locale=ja_JP.eucJP,ja_JP,ja";
    }
    else if (ev->target == Atm[TRANSPORT]) {
        val = "@transport=X/";
    }

    if (val == NULL) {
        ERRORLOG(CH_GLOBAL, "unknown target\n");
        return;
    }
    char* valcp = strdup(val);
    XChangeProperty(Disp, ev->requestor, ev->property, ev->target, 8, PropModeReplace, (unsigned char*)valcp, strlen(valcp));
    free(valcp);

    XEvent ne;
    ne.xselection.type = SelectionNotify;
    ne.xselection.selection = ev->selection;
    ne.xselection.target = ev->target;
    ne.xselection.property = ev->property;
    ne.xselection.requestor = ev->requestor;
    ne.xselection.send_event = True;
    ne.xselection.time = CurrentTime;
    ne.xselection.display = Disp;
    XSendEvent(Disp, ev->requestor, False, 0, &ne);
}

typedef int (*ProtoFunc_t)(WxContext*, XimHeader*);
typedef WxContext* (*GetCxFunc_t)(Window, const XimHeader*, int*, int*);
typedef struct {
    ProtoFunc_t rf;
    GetCxFunc_t cf;
    ProtoFunc_t cnd[2]; //[0]wimeに接続できたとき [1]できていないとき
} ReqFunc_t;
#define DEFREQ(r,c) {(ProtoFunc_t)r,c,{NULL,NULL}}
#define NWMREQ(r,c) {(ProtoFunc_t)r,c,{(ProtoFunc_t)r,(ProtoFunc_t)r##_nwm}}
#define UNDEFREQ    {NULL,NULL,{NULL,NULL}}
ReqFunc_t NormalReqFunc[] = {
    [XIM_CONNECT]
    DEFREQ(Connect,none_imic),
    UNDEFREQ,
    NWMREQ(Disconnect,none_imic),
    UNDEFREQ,

    [XIM_AUTH_REQUIRED]
    UNDEFREQ,	//XIM_AUTH_REQUIRED		=10,
    UNDEFREQ,	//XIM_AUTH_REPLY,
    UNDEFREQ,	//XIM_AUTH_NEXT,
    UNDEFREQ,	//XIM_AUTH_SETUP,
    UNDEFREQ,	//XIM_AUTH_NG,

    [XIM_ERROR]
    DEFREQ(Error,none_imic),			//20

    [XIM_OPEN]
    DEFREQ(Open,none_imic),		//=30,
    UNDEFREQ,				//XIM_OPEN_REPLY,
    DEFREQ(Close,have_im),
    UNDEFREQ,				//XIM_CLOSE_REPLY,
    UNDEFREQ,				//XIM_REGISTER_TRIGGERKEYS,
    DEFREQ(TriggerNotify,have_imic),
    UNDEFREQ,				//XIM_TRIGGER_NOTIFY_REPLY,
    UNDEFREQ,				//XIM_SET_EVENT_MASK,
    DEFREQ(EncodingNego,have_im),
    UNDEFREQ,				//XIM_ENCODING_NEGOTIATION_REPLY,
    DEFREQ(QueryExtension,have_im),
    UNDEFREQ,				//XIM_QUERY_EXTENSION_REPLY,
    UNDEFREQ,				//XIM_SET_IM_VALUES,
    UNDEFREQ,				//XIM_SET_IM_VALUES_REPLY,
    DEFREQ(GetImValues,have_im),
    UNDEFREQ,				//XIM_GET_IM_VALUES_REPLY,

    [XIM_CREATE_IC]
    NWMREQ(CreateIc,have_im),		//=50,
    UNDEFREQ,				//XIM_CREATE_IC_REPLY,
    NWMREQ(DestroyIc,have_imic),
    UNDEFREQ,				//XIM_DESTROY_IC_REPLY,
    DEFREQ(SetIcValues,have_imic),
    UNDEFREQ,				//XIM_SET_IC_VALUES_REPLY,
    DEFREQ(GetIcValues,have_imic),
    UNDEFREQ,				//XIM_GET_IC_VALUES_REPLY,
    NWMREQ(SetIcFocus,have_imic),
    NWMREQ(UnsetIcFocus,have_imic),
    NWMREQ(ForwardEvent,have_imic),	//=60
    UNDEFREQ,				//XIM_SYNC,
    DEFREQ(SyncReply,have_imic),
    UNDEFREQ,				//XIM_COMMIT,
    DEFREQ(ResetIc,have_imic),		//XIM_RESET_IC,
    UNDEFREQ,				//XIM_RESET_IC_REPLY,

    [XIM_GEOMETRY]
    UNDEFREQ,			//XIM_GEOMETRY		=70,
    UNDEFREQ,			//XIM_STR_CONVERTION,
    UNDEFREQ,			//XIM_STR_CONVERTION_REPLY,
    UNDEFREQ,			//XIM_PREEDIT_START,
    DEFREQ(PreeditStartReply,have_imic),
    UNDEFREQ,			//XIM_PREEDIT_DRAW,
    UNDEFREQ,			//XIM_PREEDIT_CARET,
    UNDEFREQ,			//XIM_PREEDIT_CARET_REPLY,
    UNDEFREQ,			//XIM_PREEDIT_DONE,
    UNDEFREQ,			//XIM_STATUS_START,
    UNDEFREQ,			//XIM_STATUS_DRAW,
    UNDEFREQ,			//XIM_STATUS_DONE,
    UNDEFREQ			//XIM_PREEDITSTATE,
};

ReqFunc_t ExtReqFunc[] = {
    DEFREQ(ExtSetEventMask,have_imic),		//=XIM_EXT_BEGIN,
    NWMREQ(ExtForwardKeyEvent,have_imic),
    DEFREQ(ExtMove,have_imic),
};

ReqFunc_t* ReqFuncs[] = { NormalReqFunc, ExtReqFunc };
unsigned ReqFuncMax[] = { XIM_PROTO_END,XIM_EXT_END - XIM_EXT_BEGIN };

void error_notify(Window win, XimErrorCode err_code, int imid, int icid, const char* msg);

int tab_index(int mj, int* ext)
{
    if (mj >= XIM_EXT_BEGIN) { //拡張リクエスト
        mj -= XIM_EXT_BEGIN;
        *ext = 1;
    }
    else
        *ext = 0;
    return mj;
}

/*
  win=サーバー
  true:通常終了
  false:キューに入れる
*/
bool proc_client_message(Window win, const XClientMessageEvent* ev, XimHeader* h)
{
    int ext, imid, icid;
    WxContext* cx;

    unsigned f_id = tab_index(h->major, &ext);
    if (f_id >= ReqFuncMax[ext] || ReqFuncs[ext][f_id].rf == NULL) {
        //BadProtocol:未定義リクエスト
        FATALLOG(CH_GLOBAL, "*** BAD PROTOCOL %hhd window 0x%lx ***\n", h->major, win);
        cx = none_imic(ev->window, h, &imid, &icid);
        if (cx == NULL)
            ERRORLOG(CH_XIM, "\tnot found context for window 0x%lx\n", ev->window);
        else
            error_notify(cx->Client, BAD_PROTOCOL, imid, icid, "WimeXim Error");
        return true;
    }
    if ((cx = ReqFuncs[ext][f_id].cf(ev->window, h, &imid, &icid)) == NULL) {
        //対応するプロキシウィンドウがない、imやicがマッチしないなど
        FATALLOG(CH_GLOBAL, "*** BAD CLIENT WINDOW 0x%lx window 0x%lx major %hhd\n", ev->window, win, h->major);
        error_notify(win, BAD_CLIENT_WINDOW, imid, icid, "WimeXim Error");
        return true;
    }
    if (cx->Sync != 0 && cx->Sync != h->major && h->major != XIM_ERROR) {
        //同期リクエストの返答を期待していたが違うのが来た
        ERRORLOG(CH_XIM, "queue this request %d window 0x%lx major %hhd\n", f_id, win, h->major);
        return false;
    }
    if ((cx->Flags & IMF_BADWINDOW) != 0) {
        //BadWindowが起きたウィンドウへのメッセージ。
        INFOLOG(CH_GLOBAL, "req for bad marked window 0x%lx.\n", cx->Client);
        return false;
    }
    DEBUGLOG(CH_XIM, "proxy 0x%lx client 0x%lx major %hhu Flag 0x%x ext %d f_id %u\n", cx->Proxy, cx->Client, h->major, cx->Flags, ext, f_id);
    cx->Sync = ReqFuncs[ext][f_id].rf(cx, h);
    return true;
}

void error_notify(Window win, XimErrorCode err_code, int imid, int icid, const char* msg)
{
    int msglen = strlen(msg);
    int bufsize = sizeof(XimError) + msglen + Pad(msglen);
    char buf[bufsize];

    XimError* e = memset(buf, 0, bufsize);
    if ((e->imid = imid) != 0)
        e->flag |= 1;
    if ((e->icid = icid) != 0)
        e->flag |= 2;
    e->code = err_code;
    e->length = msglen;
    memcpy(e->detail, msg, msglen);
    SendN(win, XIM_ERROR, e, bufsize);
}

XimHeader* get_message(Window proxy, const XClientMessageEvent* ev)
{
    Atom type;
    int format;
    unsigned long items, left;
    XimHeader* h;

    switch (ev->format) {
    case 8: //dataにある
        h = (typeof(h))ev->data.b;
        break;
    case 32: //プロパティ経由
        if (XGetWindowProperty(Disp, proxy, ev->data.l[1], 0, ev->data.l[0] * 4, True, AnyPropertyType, &type, &format, &items, &left, (unsigned char**)&h) != Success) {
            ERRORLOG(CH_GLOBAL, "FAIL XGetWindowProperty()\n");
            h = NULL;
        }
        break;
    default:
        INFOLOG(CH_GLOBAL, "message=(invalid format %d)\n", ev->format);
        h = NULL;
    }
    return h;
}

//WxContextのコンストラクタ
void context_list_cr(void* p)
{
    WxContext* wc = (WxContext*)p;
    ArNew(&wc->Ic, sizeof(IcData), NULL);
    wc->Encoding = NULL;
}

static int find_unused(const void* elem, const void* v UNUSED)
{
    return (((WxContext*)elem)->Flags & IMF_INVALID) != 0;
}
Window add_proxy(Window c)
{
#if 1
    Window p = XCreateSimpleWindow(Disp, c, 0, 0, 1, 1, 0, 0, 0);
    XSelectInput(Disp, p, StructureNotifyMask); //cが閉じられたらDestroyNotifyを受け取る
#else
    Window p = XCreateSimpleWindow(Disp, XDefaultRootWindow(Disp), 0, 0, 1, 1, 0, 0, 0);
#endif
    WxContext* cx = ArFindElemIf(&ContextList, 0, find_unused, NULL);
    cx->Proxy = p;
    cx->Client = c;
    cx->Sync = cx->Flags = 0;
    cx->Encoding = NULL;
    ArClear(&cx->Ic);
    DEBUGLOG(CH_XIM, "client 0x%lx, proxy 0x%lx\n", c, p);
    return p;
}

/*
  imがあるリクエストにマッチするコンテキストを返す
  ヘッダの次の１word目がim
*/
WxContext* have_im(Window w UNUSED, const XimHeader* h, int* imid, int* icid)
{
    *imid = *(uint16_t*)(h + 1);
    *icid = 0;
    WxContext* cx = ArElem(&ContextList, *imid - 1);
    return (*imid - 1 < ArUsing(&ContextList) && (cx->Flags & IMF_INVALID) == 0) ? cx : NULL;
}

/*
  imとicがあるリクエストにマッチするコンテキストを返す
  ヘッダの次の１word目がim,２word目がic
*/
WxContext* have_imic(Window w, const XimHeader* h, int* imid, int* icid)
{
    WxContext* cx = have_im(w, h, imid, icid);
    *icid = *((uint16_t*)(h + 1) + 1);
    return cx;
}

static int find_proxy(const void* elem, const void* ww)
{
    return ((WxContext*)elem)->Proxy == (Window)ww && (((WxContext*)elem)->Flags & IMF_INVALID) == 0;
}
/*
  中継ウィンドウwにマッチするコンテキストを返す
  imidが返される。
*/
WxContext* none_imic(Window w, const XimHeader* h UNUSED, int* imid, int* icid)
{
    *imid = ArFindIf(&ContextList, 0, find_proxy, (void*)w) + 1;
    *icid = 0;
    return *imid > 0 ? ArElem(&ContextList, *imid - 1) : NULL;
}

static const char* flag_str(unsigned flag)
{
    const char* msg[] = {
        "invalid im-id,ic-id",
        "valid im_id",
        "valid im_id,ic_id"
    };
    return flag < 3 ? msg[flag] : "unknown flag";
}
static const char* code_str(unsigned code)
{
    const char* msg[] = { //1...16
        "BadAlloc",		"BadStyle",		"BadClientWindow",
        "BadFocusWindow",	"BadArea",		"BadSpotLocation",
        "BadColormap",		"BadAtom",		"BadPixel",
        "BadPixmap",		"BadName",		"BadCursor",
        "BadProtocol",		"BadForeground",	"BadBackground",
        "LocaleNotSupported"
    };
    const char* m;
    switch (code) {
    case 1 ... 16:	m = msg[code - 1]; break;
    case 999:	m = "BadSomething"; break;
    default:	m = "unknown code";
    }
    return m;
}
int Error(WxContext* cx UNUSED, XimError* pkt)
{
    MESG("ERROR:im-id=%hd ic-id=%hd\n", pkt->imid, pkt->icid);
    MESG("	flag=0x%hx (%s)\n", pkt->flag, flag_str(pkt->flag));
    MESG("	code=%hd (%s)\n", pkt->code, code_str(pkt->code));
    if (pkt->length > 0) {
        char str[pkt->length + 1];
        memcpy(str, pkt->detail, pkt->length);
        str[pkt->length] = 0;
        MESG("	detail type=%hd(0x%hx)\n", pkt->detail_type, pkt->detail_type);
        MESG("	error detail='%s'\n", str);
    }
    else
        MESG("	error detail=(none)\n");

    return 0;
}

void Send0(Window win, unsigned mj)
{
    XimHeader pkt;
    SendN(win, mj, &pkt, sizeof(pkt));
}

void SendW(Window win, unsigned mj, uint16_t p1, uint16_t p2)
{
    XimData_ww pkt;

    pkt.p1 = p1;
    pkt.p2 = p2;
    SendN(win, mj, &pkt, sizeof(pkt));
}

/*
  size=ヘッダを含めたバイトサイズ
*/
void SendN(Window client, unsigned major, void* h, int size)
{
    XEvent ev;

    ((XimHeader*)h)->major = (major & 0xff);
    ((XimHeader*)h)->minor = (major >> 8);
    ((XimHeader*)h)->len = (size - sizeof(XimHeader)) / 4;

    if ((unsigned)size <= PACKET_MAX_SIZE) {
        ev.xclient.format = 8;
        memset(ev.xclient.data.b, 0, PACKET_MAX_SIZE);
        memcpy(ev.xclient.data.b, h, size);
    }
    else {
        XChangeProperty(Disp, client, Atm[WIMEXIM_PROP], XA_STRING, 8, PropModeAppend, h, size);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = size;
        ev.xclient.data.l[1] = Atm[WIMEXIM_PROP];
    }
    ev.type = ClientMessage;
    ev.xclient.display = Disp;
    ev.xclient.window = client;
    ev.xclient.message_type = Atm[XIM_PROTOCOL];
    XSendEvent(Disp, client, False, NoEventMask, &ev);
    XFlush(Disp);
}

//サーバーが再起動したときのシグナルの受信
static int chk_ic(IcData* ic, const Window* client)
{
    if ((ic->Flags & ICF_INVALID) == 0) {
        INFOLOG(CH_XIM, "cxn %d is invalid,replace.\n", ic->WimeCxn);
        SetWimeData(ic);
        CallbackParam cb = { ic,*client,NULL }; //!!!Pktは使わないと思うが。
        if ((ic->Flags & ICF_CB_INIT) != 0)
            ic->ConvFunc->Init(&cb);
        if ((ic->Flags & ICF_HAVE_FOCUS) != 0)
            WimeSetFocus(ic->WimeCxn, true);
    }
    return 0;
}
static int chk_im(WxContext* wc, void* arg UNUSED)
{
    if ((wc->Flags & (IMF_INVALID | IMF_CLOSE)) == 0) {
        ArForEach(&wc->Ic, (ArForEachFunc)chk_ic, &wc->Client);
    }
    return 0;
}
static void restart_server(void)
{
    //Xのイベントループの中にいるので、ループから抜けたところで再起動の処理をする。
    XEvent ev;
    ev.xclient.type = ClientMessage;
    ev.xclient.window = ServerWin;
    ev.xclient.message_type = Atm[RESTART_WIME];
    ev.xclient.format = 32;
    XSendEvent(Disp, ServerWin, False, NoEventMask, &ev);
}

//(C) 2009 thomas
