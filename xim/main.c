#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <locale.h>
#include <getopt.h>
#include "wimexim.h"
#include "lib/link.h"
#include "so/wimeapi.h"
#include "so/winkey.h"
#include "so/xres.h"
#include "exe/version.h"

ToggleKey *ToggleKeys; //変換トグルキーとシフト状態
char *DefaultCompFont;	//変換ウィンドウのフォント

#define SERVERNAME "wime"

void on_selection_request(Window win,const XSelectionRequestEvent* ev);
void on_client_message(Window win,XClientMessageEvent* ev);
Window make_server(int ac,char* av[]);
void context_list_cr(void* p);
Window add_proxy(Window c);
void destroy_client(const XDestroyWindowEvent* ev);
void reset_req_func_tab(bool enable_wime);
int cmdline_opt(int ac,char* av[]);

//windowとXimHeaderからWxContextを探す
WxContext* none_imic(Window,const XimHeader*,int*,int*);
WxContext* have_imic(Window,const XimHeader*,int*,int*);
WxContext* have_im(Window,const XimHeader*,int*,int*);

enum{
    WIMEXIM_PROP,	// _XIM_WIMEXIM_PROP
    XIM_PROTOCOL,	// _XIM_PROTOCOL
    SERVER,		// @server=wime
    XIM_SERVERS,	// XIM_SERVERS

    //この３つは先に作っておかないとSelectionRequestが飛んでこないみたい
    XIM_XCONNECT,	// _XIM_XCONNECT
    LOCALES,		// LOCALES
    TRANSPORT,		// TRANSPORT

    ATOM_MAX
};
Atom Atm[ATOM_MAX];

Array ContextList;
Display* Disp;

int main(int ac,char* av[])
{
    Window win;
    XEvent ev;
    const char* atom_str[]={
	"_XIM_WIMEXIM_PROP",
	"_XIM_PROTOCOL",
	"@server=" SERVERNAME,
	"XIM_SERVERS",
	"_XIM_XCONNECT",
	"LOCALES",
	"TRANSPORT"
    };

    Verbose = 1;

    if(setlocale(LC_ALL,"") == NULL){
	ERR("cannot set locale\n");
	return 1;
    }
    if(!XSupportsLocale()){
	ERR("not support locale\n");
	return 1;
    }
    if(!WimeInitialize(cmdline_opt(ac,av),LOGMARK)){
	ERR("cannot connect wime\n");
    }

    Disp = XOpenDisplay(NULL);
    InitDatabase(Disp,"xim");
    ToggleKeys = GetConvKeyFromResource(Disp);
    DefaultCompFont = GetCompFont(Disp);
    ArNew(&ContextList,sizeof(WxContext),context_list_cr);
    for(int i=0; i<ATOM_MAX; ++i)
	Atm[i] = XInternAtom(Disp,atom_str[i],False);
    win = make_server(ac,av);

    while(1){
	XNextEvent(Disp,&ev);
	switch(ev.type){
	case SelectionRequest:
	    on_selection_request(win,(XSelectionRequestEvent*)&ev);
	    break;
	case ClientMessage:
	    on_client_message(win,(XClientMessageEvent*)&ev);
	    break;
	case DestroyNotify:
	    if(((XDestroyWindowEvent*)&ev)->window == win)
		goto fin;
	    destroy_client((XDestroyWindowEvent*)&ev);
	    break;
	case ConfigureNotify: //root window入力で前編集窓を動かした時
	    MoveInputWindow((XConfigureEvent*)&ev);
	    break;
	case MappingNotify:
	    LOG("MappingNotify\n");
	    XRefreshKeyboardMapping((XMappingEvent*)&ev);
	    break;
#if 0
	default:
	    MSG("EVENT:%d\n",ev.type);
#endif
	}
    }
fin:
    WimeFinalize();
    XCloseDisplay(Disp);
    return 0;
}

//クライアントウィンドウが閉じられた時
void destroy_client(const XDestroyWindowEvent* ev)
{
    int imid,icid;
    WxContext *cx;

    if((cx = none_imic(ev->window,NULL,&imid,&icid)) != NULL){
	LOG("destroy notify proxy %p client %p\n",cx->Proxy,cx->Client);
	if(setjmp(WimeJmp) == 0)
	    DisconnectClient(cx,false);
    }
}

bool proc_client_message(Window win,const XClientMessageEvent* ev,XimHeader* h);
XimHeader* get_message(Window win,const XClientMessageEvent* ev);
void preconnect(const XClientMessageEvent* ev);

typedef struct{
    Window win;
    XClientMessageEvent ev;
    XimHeader *pkt; //プロパティ経由のデータのアドレスを入れる。
} QueueData;
BiLink *EventQ;

void on_client_message(Window win,XClientMessageEvent* ev)
{
    if(ev->message_type == Atm[XIM_XCONNECT]){
	preconnect(ev);
    }else if(ev->message_type == Atm[XIM_PROTOCOL]){
	QueueData *q;
	XimHeader *qh;

	XimHeader* h = get_message(ev->window,ev);
	bool st = proc_client_message(win,ev,h);

	//キューに残っているパケットがあれば処理してみる
	BiLink* c=EventQ;
	while(c!=NULL){
	    LOG("check queue\n");
	    q = c->obj;
	    qh = q->pkt!=NULL ? q->pkt : get_message(q->win,&q->ev);
	    if(proc_client_message(q->win,&q->ev,qh)){
		//処理できたので、キューから削除してキュー先頭から再検査
		if((char*)qh != q->ev.data.b)
		    XFree(qh);
		free(LkRemove(&EventQ,c));
		c = EventQ;
		continue;
	    }
	    c = c->next;
	}

	if(st){
	    if((char*)h != ev->data.b) //XGetWindowPropertyできたデータ
		XFree(h);
	}else{
	    q = malloc(sizeof(QueueData));
	    q->win = win;
	    q->ev = *ev;
	    q->pkt = (char*)h!=ev->data.b ? h : NULL;
	    LkPushEnd(&EventQ,q);
	}
    }else{
	char* n = XGetAtomName(Disp,ev->message_type);
	LOG("unknown message type %s\n",n);
	XFree(n);
    }
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
    XSendEvent(Disp,ne.xclient.window,False,NoEventMask,&ne);
    LOG("client-id=0x%lx version=%ld/%ld proxy-window=0x%lx\n",ev->data.l[0],ev->data.l[1],ev->data.l[2],ne.xclient.data.l[0]);
    /*
      これ以降_XIM_PROTOCOLでデータが来るわけだが、windowメンバは自
      分（サーバ）になっている。ということは誰に返答すればいいか分
      からない。なので、接続ごとに中継windowをつくることにする。そ
      うするとwindowメンバには中継windowが入っているので、対応表を
      見ればクライアントwindowが分かる。
    */
}

Window make_server(int ac,char* av[])
{
    Atom *data,type;
    int format;
    unsigned long ndata,r;
    Window win,root;
    const char name[]=SERVERNAME;
    XTextProperty np={(unsigned char*)name,XA_STRING,8,sizeof(name)-1};

    root = XDefaultRootWindow(Disp);
    win = XCreateSimpleWindow(Disp,root,0,0,1,1,0,0,XWhitePixel(Disp,XDefaultScreen(Disp)));
    LOG("create display %p window %p\n",Disp,win);
    XSetWMProperties(Disp,win,&np,&np,av,ac,NULL,NULL,NULL);
    XSelectInput(Disp,win,StructureNotifyMask);//destroyイベントを受ける

    XSetSelectionOwner(Disp,Atm[SERVER],win,CurrentTime);

    //XIM_SERVERSに追加する
    if(XGetWindowProperty(Disp,root,Atm[XIM_SERVERS],0,40*1024/4,False,XA_ATOM,&type,&format,&ndata,&r,(unsigned char**)&data) == Success){
	unsigned long n;
	for(n=0; n<ndata && data[n]!=Atm[SERVER]; ++n)
	    ;
	if(n < ndata){ //すでに登録されている
	    data[n] = data[--ndata]; //一番後ろのデータで上書き
	    XChangeProperty(Disp,root,Atm[XIM_SERVERS],XA_ATOM,32,PropModeReplace,(unsigned char*)data,ndata);
	}
	XFree(data);
    }
    XChangeProperty(Disp,root,Atm[XIM_SERVERS],XA_ATOM,32,PropModeAppend,(unsigned char*)&Atm[SERVER],1);

    return win;
}

//LOCALESとTRANSPORT
void on_selection_request(Window win,const XSelectionRequestEvent* ev)
{
    VERBOSE(MSG("%s %s %s\n",XGetAtomName(Disp,ev->selection),XGetAtomName(Disp,ev->target),XGetAtomName(Disp,ev->property)));

    const char *val=NULL;
    char *valcp;
    if(ev->target == Atm[LOCALES]){
	//EUC-JPは認識されなかった。eucjp以外のためにja_JPなども入れておく。
	//→わざわざja_JP.eucJPを指定しなくてもjaだけでいいのか？
	val = "@locale=ja_JP.eucJP,ja_JP,ja";
    }else if(ev->target == Atm[TRANSPORT]){
	val = "@transport=X/";
    }

    if(val == NULL){
	LOG("unknown target\n");
	return;
    }
    valcp = strdup(val);
    XChangeProperty(Disp,ev->requestor,ev->property,ev->target,8,PropModeReplace,(unsigned char*)valcp,strlen(valcp));
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
    XSendEvent(Disp,ev->requestor,False,0,&ne);
}

typedef int (*ProtoFunc_t)(WxContext*,XimHeader*);
typedef WxContext* (*GetCxFunc_t)(Window,const XimHeader*,int*,int*);
typedef struct{
    ProtoFunc_t rf;
    GetCxFunc_t cf;
    ProtoFunc_t cnd[2]; //[0]wimeに接続できたとき [1]できていないとき
} ReqFunc_t;
#define DEFREQ(r,c) {(ProtoFunc_t)r,c,{NULL,NULL}}
#define NWMREQ(r,c) {(ProtoFunc_t)r,c,{(ProtoFunc_t)r,(ProtoFunc_t)r##_nwm}}
#define UNDEFREQ    {NULL,NULL,{NULL,NULL}}
ReqFunc_t NormalReqFunc[]={
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

ReqFunc_t ExtReqFunc[]={
    UNDEFREQ	//XIM_EXT_SET_EVENT_MASK	=XIM_EXT_BEGIN,
};
    
ReqFunc_t* ReqFuncs[]={ NormalReqFunc, ExtReqFunc };
unsigned ReqFuncMax[]={XIM_PROTO_END,XIM_EXT_END-XIM_EXT_BEGIN};

void error_notify(Window win,XimErrorCode err_code,int imid,int icid,const char* msg);

int tab_index(int mj,int* ext)
{
    if(mj >= XIM_EXT_BEGIN){ //拡張リクエスト
	mj -= XIM_EXT_BEGIN;
	*ext = 1;
    }else
	*ext = 0;
    return mj;
}

/*
  win=サーバー
  true:通常終了
  false:キューに入れる
*/
bool proc_client_message(Window win,const XClientMessageEvent* ev,XimHeader* h)
{
    unsigned f_id;
    int ext,imid,icid;
    WxContext *cx;

    f_id = tab_index(h->major,&ext);
    if(f_id>=ReqFuncMax[ext] || ReqFuncs[ext][f_id].rf==NULL){
	//BadProtocol:未定義リクエスト
	MSG("*** BAD PROTOCOL %hhd window %p ***\n",h->major,win);
	int dum1,dum2;
	cx = none_imic(ev->window,h,&dum1,&dum2);
	if(cx == NULL)
	    MSG("\tnot found context for window 0x%x\n",ev->window);
	else
	    error_notify(cx->Client,BAD_PROTOCOL,imid,icid,"WimeXim Error");
	return true;
    }
    if((cx = ReqFuncs[ext][f_id].cf(ev->window,h,&imid,&icid)) == NULL){
	//対応するプロキシウィンドウがない、imやicがマッチしないなど
	MSG("*** BAD CLIENT WINDOW %p window %p major %hhd\n",ev->window,win,h->major);
	error_notify(win,BAD_CLIENT_WINDOW,imid,icid,"WimeXim Error");
	return true;
    }
    if(cx->Sync!=0 && cx->Sync!=h->major && h->major!=XIM_ERROR){
	//同期リクエストの返答を期待していたが違うのが来た
	LOG("queue this request %d window %p major %hhd\n",f_id,win,h->major);
	return false;
    }
    if(setjmp(WimeJmp) != 0){
	ERR("Disconnect wime, window 0x%x major-code %hhd\n",(unsigned)win,h->major);
	reset_req_func_tab(false);
    }
    LOG("proxy %p client %p major %hhd\n",cx->Proxy,cx->Client,h->major);
    cx->Sync = ReqFuncs[ext][f_id].rf(cx,h);
    return true;
}

void error_notify(Window win,XimErrorCode err_code,int imid,int icid,const char* msg)
{
    int msglen = strlen(msg);
    int bufsize = sizeof(XimError)+msglen+Pad(msglen);
    char buf[bufsize];
    XimError *e = memset(buf,0,bufsize);
    if((e->imid = imid) != 0)
	e->flag |= 1;
    if((e->icid = icid) != 0)
	e->flag |= 2;
    e->code = err_code;
    e->length = msglen;
    memcpy(e->detail,msg,msglen);
    send_n(win,XIM_ERROR,e,bufsize);
}

XimHeader* get_message(Window proxy,const XClientMessageEvent* ev)
{
    Atom type;
    int format;
    unsigned long items,left;
    XimHeader *h;

    switch(ev->format){
    case 8: //dataにある
	h = (typeof(h))ev->data.b;
	break;
    case 32: //プロパティ経由
	if(XGetWindowProperty(Disp,proxy,ev->data.l[1],0,ev->data.l[0]*4,True,AnyPropertyType,&type,&format,&items,&left,(unsigned char**)&h)!=Success){
	    MSG("FAIL XGetWindowProperty()\n");
	    h = NULL;
	}
	break;
    default:
	LOG("message=(invalid format %d)\n",ev->format);
	h = NULL;
    }
    return h;
}

//WxContextのコンストラクタ
void context_list_cr(void* p)
{
    WxContext *wc = (WxContext*)p;
    ArNew(&wc->Ic,sizeof(IcData),NULL);
    wc->Encoding = NULL;
}

Window add_proxy(Window c)
{
    WxContext *cx;
    Window p;

    int find_unused(const void* v UNUSED,const void* elem){
	return (((WxContext*)elem)->Flags & IMF_INVALID)!=0;
    }

#if 1
    p = XCreateSimpleWindow(Disp,c,0,0,1,1,0,0,0);
    XSelectInput(Disp,p,StructureNotifyMask); //cが閉じられたらDestroyNotifyを受け取る
#else
    p = XCreateSimpleWindow(Disp,XDefaultRootWindow(Disp),0,0,1,1,0,0,0);
#endif
    cx = ArFindElemIf(&ContextList,0,find_unused,NULL);
    cx->Proxy = p;
    cx->Client = c;
    cx->Sync = cx->Flags = 0;
    cx->Ic.use = 0;
    cx->Encoding = NULL;
    LOG("client %p, proxy %p\n",c,p);
    return p;
}

/*
  imがあるリクエストにマッチするコンテキストを返す
  ヘッダの次の１word目がim
*/
WxContext* have_im(Window w UNUSED,const XimHeader* h,int* imid,int* icid)
{
    *imid = *(uint16_t*)(h+1);
    *icid = 0;
    WxContext *cx = ArElem(&ContextList,*imid-1);
    return *imid-1<ContextList.use && (cx->Flags & IMF_INVALID)==0 ? cx : NULL;
}

/*
  imとicがあるリクエストにマッチするコンテキストを返す
  ヘッダの次の１word目がim,２word目がic
*/
WxContext* have_imic(Window w,const XimHeader* h,int* imid,int* icid)
{
    WxContext *cx = have_im(w,h,imid,icid);
    *icid = *((uint16_t*)(h+1)+1);
    return cx;
}

/*
  中継ウィンドウwにマッチするコンテキストを返す
  imidが返される。
*/
WxContext* none_imic(Window w,const XimHeader* h UNUSED,int* imid,int* icid)
{
    int find_proxy(const void* ww,const void* elem){
	return ((WxContext*)elem)->Proxy==(Window)ww && (((WxContext*)elem)->Flags & IMF_INVALID)==0;
    }

    *imid = ArFindIf(&ContextList,0,find_proxy,(void*)w)+1;
    *icid = 0;
    return *imid>0 ? ArElem(&ContextList,*imid-1) : NULL;
}

int Error(WxContext* cx UNUSED,XimError* pkt)
{
    const char* flag_str(unsigned flag){
	const char *msg[]={
	    "invalid im-id,ic-id",
	    "invalid im_id",
	    "invalid ic_id"
	};
	return flag<3 ? msg[flag] : "unknown flag";
    }
    const char* code_str(unsigned code){
	const char *msg[]={ //1...16
	    "BadAlloc",		"BadStyle",		"BadClientWindow",
	    "BadFocusWindow",	"BadArea",		"BadSpotLocation",
	    "BadColormap",	"BadAtom",		"BadPixel",
	    "BadPixmap",	"BadName",		"BadCursor",
	    "BadProtocol",	"BadForeground",	"BadBackground",
	    "LocaleNotSupported"
	};
	const char *m;
	switch(code){
	case 1 ... 16:	m=msg[code-1]; break;
	case 999:	m="BadSomething"; break;
	default:	m="unknown code";
	}
	return m;
    }
	   
    MSG("ERROR:im-id=%hd ic-id=%hd\n",pkt->imid,pkt->icid);
    MSG("	flag=%hx (%s)\n",pkt->flag,flag_str(pkt->flag));
    MSG("	code=%hd (%s)\n",pkt->code,code_str(pkt->code));
    if(pkt->length > 0){
	char str[pkt->length+1];
	memcpy(str,pkt->detail,pkt->length);
	str[pkt->length] = 0;
	MSG("	detail type=%hd(0x%hx)\n",pkt->detail_type,pkt->detail_type);
	MSG("	error detail='%s'\n",str);
    }else
	MSG("	error detail=(none)\n");

    return 0;
}

void send_0(Window win,unsigned mj)
{
    XimHeader pkt;
    send_n(win,mj,&pkt,sizeof(pkt));
}

void send_ww(Window win,unsigned mj,uint16_t p1,uint16_t p2)
{
    XimData_ww pkt;

    pkt.p1 = p1;
    pkt.p2 = p2;
    send_n(win,mj,&pkt,sizeof(pkt));
}

/*
  size=ヘッダを含めたバイトサイズ
*/
void send_n(Window client,unsigned major,void* h,int size)
{
    XEvent ev;

    ((XimHeader*)h)->major = (major & 0xff);
    ((XimHeader*)h)->minor = (major >> 8);
    ((XimHeader*)h)->len = (size-sizeof(XimHeader))/4;

    if((unsigned)size <= PACKET_MAX_SIZE){
	ev.xclient.format = 8;
	memset(ev.xclient.data.b,0,PACKET_MAX_SIZE);
	memcpy(ev.xclient.data.b,h,size);
    }else{
	XChangeProperty(Disp,client,Atm[WIMEXIM_PROP],XA_STRING,8,PropModeAppend,h,size);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = size;
	ev.xclient.data.l[1] = Atm[WIMEXIM_PROP];
    }
    ev.type = ClientMessage;
    ev.xclient.display = Disp;
    ev.xclient.window = client;
    ev.xclient.message_type = Atm[XIM_PROTOCOL];
    XSendEvent(Disp,client,False,NoEventMask,&ev);
    XFlush(Disp);
}

/*
  wimeがないときのリクエスト処理関数。wimeを使うリクエストは全てこの関数になる。
*/
int disable_wime_req(WxContext* cx,XimHeader* pkt)
{
    ProtoFunc_t rf;
    int r,ext;
    ReqFunc_t *f;

    r = tab_index(pkt->major,&ext);
    f = ReqFuncs[ext] + r;
    if(WimeIsConnected()){
	reset_req_func_tab(true);
	rf = f->rf;
    }else{
	if((rf = f->cnd[1]) == NULL)
	    rf = f->rf;
    }
    return (*rf)(cx,pkt);
}

//ReqFunc_tのrfをcnd[0]かdisable_wime_reqに変える
void reset_req_func_tab(bool enable_wime)
{
    unsigned mn,mj;
    ReqFunc_t *tab;

    for(mn=0; mn<ITEMS(ReqFuncs); ++mn){
	tab = ReqFuncs[mn];
	for(mj=0; mj<ReqFuncMax[mn]; ++mj,++tab){
	    if(tab->cnd[0] != NULL)
		tab->rf = enable_wime ? tab->cnd[0] : disable_wime_req;
	}
    }
}

void usage(int exit_code)
{
    printf("wimexim [options]\n"
	   "  -p num	socket number\n"
	   "  -v,-v-	verbose (on,off)\n"
	   "  --version	print version\n"
	   "  -h,--help	this message\n"
	);
    exit(exit_code);
}

//ソケットのオプション番号を返す
int cmdline_opt(int ac,char* av[])
{
    struct option longopt[]={
	{"help",	no_argument,NULL,'h'},
	{"version",	no_argument,NULL,'vsn'},
	{NULL,0,NULL,0}
    };
    int c,socket_num=0;
    
    while((c = getopt_long(ac,av,"hp:v::",longopt,NULL)) != -1){
	switch(c){
	case 'p':
	    socket_num = atoi(optarg);
	    break;
	case 'v':
	    if(optarg==NULL)
		Verbose = 1;
	    else if(strcmp(optarg,"-")==0)
		Verbose = 0;
	    else
		usage(1);
	    break;
	case 'vsn':
	    printf("%s\n",WIME_VERSION);
	    exit(0);
	case 'h':
	    usage(0);
	default:
	    usage(1);
	}
    }
    return socket_num;
}
