// -*- coding:euc-jp -*-
#define _GNU_SOURCE
#include <windows.h>
#include <stdio.h>
#include <imm.h>
#include <printf.h>
#include <getopt.h>
#include "canna.h"
#include "io/wimeio.h"
#include "so/wimeapi.h"
#include "so/pkt.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "lib/log.h"
#include "lib/cmdlineopt.h"
#include "lib/version.h"
#include "lib/printf.h"
#include "apisup.h"

//message id
enum {
    WM_SET_COMP_STR = WM_APP,
    WM_CANNA_PACKET,
};

struct GlobalData_t WimeData;
WMCANNAPROTO* WmCannaTab[3];
unsigned CanFunMax[3];
char ClassName[]="ImeBridge";
FILE* LogFile; //指定がなければstdoutにする
char* LogFileName;

HWND NewWin();
DWORD WINAPI recv_xim(void* h);
LRESULT CALLBACK wnd_proc(HWND wh,UINT msg,WPARAM wp,LPARAM lp);
static int initialize(int ac,char* av[]);
static void ime_info(void);

int main(int ac,char* av[])
{
    int socket_num = initialize(ac,av);
    if(socket_num < 0){
	fprintf(stderr,"%s:initialize error. exit.\n",av[0]);
	exit(1);
    }
    HWND msgwin = NewWin();
    HANDLE th = CreateThread(NULL,0,recv_xim,msgwin,0,NULL);

    DEBUGLOG(CH_GLOBAL,"wime " WIME_VER_STR " %dbit " __DATE__ " " __TIME__ "\n",(int)sizeof(void*)*8);
    DEBUGDO(CH_GLOBAL,ime_info());

    ImSemStart(socket_num);
    ShmStartServer(socket_num);
    MSG msg;
    while(GetMessage(&msg, NULL, 0, 0) >0) {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }

    CloseHandle(th);
    DestroyWindow(msgwin);
    ImSemUnlink(socket_num);
    DEBUGLOG(CH_GLOBAL,"EXIT\n");
    return 0;
}

/*
  fnをオープンしてLogFileにセットする
*/
static void open_logfile(const char* fn,const char* mode)
{
    if(LogFile != stdout){
	fclose(LogFile);
	LogFile = stdout;
    }
    if(fn!=NULL && *fn!=0){
	if(strcmp(fn,"-")==0)
	    LogFile = stdout;
	else if((LogFile = fopen(fn,mode)) == NULL){
	    ERR("cannot open log file '%s'\n",fn);
	    LogFile = stdout;
	}
    }
}

bool AtInit(WMCANNAPROTO* tab[]);

//ime別の初期化
static bool ime_sp(const char* arg,void* unused)
{
    struct{
	char* name;
	bool (*init)(WMCANNAPROTO* tab[]);
    } tab[]={
	{"atok",AtInit},
	{NULL,NULL}
    };
    int n;
    
    for(n=0; tab[n].name!=NULL && strcmp(tab[n].name,arg)!=0; ++n)
	;
    return tab[n].name!=NULL && tab[n].init(WmCannaTab);
}

//ウィンドウプロシージャのコールバック関数テーブル
static void init_cb(void)
{
    static WMCANNAPROTO wm_canna_tab0[]={
	Init,		/*0x00*/	NULL,		/*0x01*/
	Finalize,	/*0x02*/	CreateContext,	/*0x03*/
	DupContext,	/*0x04*/	CloseContext,	/*0x05*/
	GetDicList,	/*0x06*/	GetDirList,	/*0x07*/
	MountDic,	/*0x08*/	UnmountDic,	/*0x09*/
	RemountDic,	/*0x0a*/	MountDicList,	/*0x0b*/
	QueryDic,	/*0x0c*/	DefineWord,	/*0x0d*/
	DeleteWord,	/*0x0e*/	BeginConv,	/*0x0f*/
	EndConv,	/*0x10*/	GetCandiList,	/*0x11*/
	GetYomi,	/*0x12*/	SubstYomi,	/*0x13*/
	StoreYomi,	/*0x14*/	StoreRange,	/*0x15*/
	GetLastYomi,	/*0x16*/	FlushYomi,	/*0x17*/
	RemoveYomi,	/*0x18*/	GetSimpleKanji,	/*0x19*/
	ResizePause,	/*0x1a*/	GetHinshi,	/*0x1b*/
	GetLex,		/*0x1c*/	GetStatus,	/*0x1d*/
	SetLocale,	/*0x1e*/	AutoConv,	/*0x1f*/
	QueryExt,	/*0x20*/	SetAppName,	/*0x21*/
	NoticeGroup,	/*0x22*/	NULL,		/*0x23*/
	KillServer	/*0x24*/
    };
    static WMCANNAPROTO wm_canna_tab1[]={
	NULL,		/*0x00*/	GetServerInfo,	/*0x01*/
	GetAcl,		/*0x02*/	CreateDic,	/*0x03*/
	DeleteDic,	/*0x04*/	RenameDic,	/*0x05*/
	GetWordTextDic,	/*0x06*/	ListDic,	/*0x07*/
	Sync,		/*0x08*/	ChmodDic,	/*0x09*/
	CopyDic,	/*0x0a*/
    };
#define INITTAB(name) [WIME_##name & 0xff]=name
    static WMCANNAPROTO wm_canna_tab2[]={
	NULL,		/*00*/
	//so/pkt.hのプロトコル番号,canna.cのquery_ext()も変更すること
	INITTAB(OpenDialog),
	INITTAB(SetCompositionWin),
	INITTAB(GetCompositionWin),
	INITTAB(SendKey),
	INITTAB(EnableIme),
	INITTAB(MoveShadowWin),
	INITTAB(SetCompositionFont),
	INITTAB(GetCompositionStr),
	INITTAB(SetCandidateWin),
	INITTAB(RegXWin),
	INITTAB(GetResultStr),
	INITTAB(SetResultStr),
	INITTAB(Reconvert),
	INITTAB(SetImeFocus),
	INITTAB(ShowToolbar),
	INITTAB(GetStyleList),
	INITTAB(ReloadConf),
	INITTAB(FlushMsg),
	INITTAB(ShowCandidateWin),
	INITTAB(SelectCandidate),
	INITTAB(CloseCandidateWin),
	INITTAB(DumpContext),
	INITTAB(SetDebugChannel),
    };
#undef INITTAB
    
    CanFunMax[0] = ITEMS(wm_canna_tab0);
    CanFunMax[1] = ITEMS(wm_canna_tab1);
    CanFunMax[2] = ITEMS(wm_canna_tab2);

    WmCannaTab[0] = wm_canna_tab0;
    WmCannaTab[1] = wm_canna_tab1;
    WmCannaTab[2] = wm_canna_tab2;
}

static void reg_class(void)
{
    WNDCLASS wc={0};

    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = ClassName;
    if(!RegisterClass(&wc)){
	ERR("fail RegisterClass '%s'\n",ClassName);
	exit(1);
    }
}

uint16_t* get_cs_a(HIMC imc,DWORD index)
{
    int sz;
    uint16_t* u16=NULL;
    if((sz = ImmGetCompositionStringA(imc,index,NULL,0)) > 0){
	char* buf = malloc(sz+1);
	ImmGetCompositionStringA(imc,index,buf,sz);
	buf[sz] = 0;
	u16 = SjToU16(NULL,buf,-1);
	free(buf);
    }
    return u16;
}
uint16_t* get_cs_w(HIMC imc,DWORD index)
{
    int sz;
    uint16_t* u16=NULL;
    if((sz = ImmGetCompositionStringW(imc,index,NULL,0)) > 0){
	u16 = calloc(sz+1,2);
	ImmGetCompositionStringW(imc,index,u16,sz);
	u16[sz] = 0;
    }
    return u16;
}

/* GlobalData_t::ImeVersion
   デフォルトで０を返す。-eオプションで関数を変える。
 */
static int default_ime_version(void)
{
    return 0;
}

static void set_wimedata(struct GlobalData_t* wd)
{
    int p = ImmGetProperty(GetKeyboardLayout(0),IGP_PROPERTY);
    if(p & IME_PROP_UNICODE){
	wd->CharStep = 1;
	wd->SetCompStr = ImmSetCompositionStringW;
	wd->GetCompStr = get_cs_w;
    }else{
	wd->CharStep = 2;
	wd->SetCompStr = ImmSetCompositionStringA;
	wd->GetCompStr = get_cs_a;
    }
    wd->GetCandidate = GetCandidateW;
    wd->CandIndexStart=0; //((p & IME_PROP_CANDLIST_START_FROM_1) ? 1 : 0); ???余計おかしくなった。
    wd->ImeVersion = default_ime_version;
}

//ソケット番号を返す。エラーの時-1
static int initialize(int ac,char* av[])
{
    const int use_tcp=0;
    OptArg oa[]={
	{NULL,'e',required_argument,ime_sp,NULL,"specify ime","<ime>"},
    };

    LogFile = stdout;
    LogMark = 'w';
    init_cb();
    set_wimedata(&WimeData); //メモ書き参照
    CustomPrintf();

    int socket_num = CmdlineOpt(ac,av,oa,ITEMS(oa),"[filename]	log file name");
    LogFileName = av[optind];
    open_logfile(LogFileName,"w");
    
    setbuf(stdout,NULL);
    reg_class();

    if(!ImInit(socket_num,use_tcp)){
	return -1;
    }

    int st = ImReadSetting(&WimeData); //まだログは出せない
    DEBUGLOG(CH_GLOBAL,"load hinshi file:status %d\n",st);

    InitClientData();
    return socket_num;
}

#ifndef IME_PROP_ACCEPT_WIDE_VKEY
#define IME_PROP_ACCEPT_WIDE_VKEY 0x20
#endif

static void ime_info(void)
{
    BitDesc igp_prop[] = {BITDESC(IME_PROP_AT_CARET),
			  BITDESC(IME_PROP_SPECIAL_UI),
			  BITDESC(IME_PROP_CANDLIST_START_FROM_1),
			  BITDESC(IME_PROP_UNICODE),
			  BITDESC(IME_PROP_COMPLETE_ON_UNSELECT),
			  BITDESC(IME_PROP_END_UNLOAD),
			  BITDESC(IME_PROP_KBD_CHAR_FIRST),
			  BITDESC(IME_PROP_NEED_ALTKEY),
			  BITDESC(IME_PROP_IGNORE_UPKEYS),
			  BITDESC(IME_PROP_NO_KEYS_ON_CLOSE),
			  BITDESC(IME_PROP_ACCEPT_WIDE_VKEY),
			  {0,NULL}};
    BitDesc igp_ui[] = {BITDESC(UI_CAP_2700),
			BITDESC(UI_CAP_ROT90),
			BITDESC(UI_CAP_ROTANY),
			{0,NULL}};
    BitDesc igp_comp[] = {BITDESC(SCS_CAP_COMPSTR),
			  BITDESC(SCS_CAP_MAKEREAD),
			  BITDESC(SCS_CAP_SETRECONVERTSTRING),
			  {0,NULL}};
    BitDesc igp_sel[] = {BITDESC(SELECT_CAP_CONVERSION),
			 BITDESC(SELECT_CAP_SENTENCE),
			 {0,NULL}};
    BitDesc igp_ver[] = {BITDESC(IMEVER_0310),
			 BITDESC(IMEVER_0400),
			 {0,NULL}};
    BitDesc igp_conv[] = {BITDESC(IME_CMODE_NATIVE),
			  BITDESC(IME_CMODE_KATAKANA),
			  BITDESC(IME_CMODE_LANGUAGE),
			  BITDESC(IME_CMODE_FULLSHAPE),
			  BITDESC(IME_CMODE_ROMAN),
			  BITDESC(IME_CMODE_CHARCODE),
			  BITDESC(IME_CMODE_HANJACONVERT),
			  BITDESC(IME_CMODE_SOFTKBD),
			  BITDESC(IME_CMODE_NOCONVERSION),
			  BITDESC(IME_CMODE_EUDC),
			  BITDESC(IME_CMODE_SYMBOL),
			  BITDESC(IME_CMODE_FIXED),
			  {0,NULL}};
    BitDesc igp_sen[] = {BITDESC(IME_SMODE_NONE),
			 BITDESC(IME_SMODE_PLAURALCLAUSE),
			 BITDESC(IME_SMODE_SINGLECONVERT),
			 BITDESC(IME_SMODE_AUTOMATIC),
			 BITDESC(IME_SMODE_PHRASEPREDICT),
			 BITDESC(IME_SMODE_CONVERSATION),
			 {0,NULL}};

    HKL kl = GetKeyboardLayout(0);
    unsigned sz = ImmGetIMEFileName(kl,NULL,0);
    char ime_fn[sz+1];
    ime_fn[0] = 0;
    ImmGetIMEFileName(kl,ime_fn,sz);

    sz = ImmGetDescription(kl,NULL,0);
    char desc[sz+1];
    desc[sz] = 0;
    ImmGetDescription(kl,desc,sz);

    MESG("kb layout    %p\n",kl);
    MESG("ime filename '%s'\n",ime_fn);
    MESG("description  '%s'\n",desc);
    MESG("property\n");

    struct{
	const char* str;
	DWORD index;
	const BitDesc* bits;
    } prop[]={{"conersion     ",IGP_CONVERSION,igp_conv},
	      {"ime-version   ",IGP_GETIMEVERSION,igp_ver},
	      {"property      ",IGP_PROPERTY,igp_prop},
	      {"select        ",IGP_SELECT,igp_sel},
	      {"sentence      ",IGP_SENTENCE,igp_sen},
	      {"set-comp-str  ",IGP_SETCOMPSTR,igp_comp},
	      {"ui            ",IGP_UI,igp_ui},
	      {NULL,0,NULL}};
    for(int n=0; prop[n].str!=NULL; ++n){
	Array* buf = FlagStr(ImmGetProperty(kl,prop[n].index),prop[n].bits,NULL);
	MESG("\t%s%s\n",prop[n].str,(char*)ArAdr(buf));
	free(ArDelete(buf));
    }
}

static int MsgSn;
#define SN_FORM "%05d"
#define SN_MAX 100000

static int print_logfile_str(char mark,const char* s)
{
    int r = fprintf(LogFile,"[%c][" SN_FORM "]",mark,MsgSn);
    if((DebugChannel & CH_TIME)){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC,&t); 
	r += fprintf(LogFile,"[%lu.%lu]",t.tv_sec,t.tv_nsec/1000);
    }
    r += fprintf(LogFile,"%s",s);
    if(r > 0)
	MsgSn = (MsgSn+1)%SN_MAX;
    return r;
}

//!!! 同期を取った方がいいか？
void log_req(const Req15_t* r)
{
    /*
      要求:type15
	p1=マーク文字
	p2=使わない
	p3=文字列
      応答:type2
    */
    bool st=true;
    for(int n=0; n<2 && !(st=(print_logfile_str(Swap4(r->p1),r->p3)>=0)); ++n){
	open_logfile(LogFileName,"a");
    }
    Reply2(WIME_Log&0xff,WIME_Log>>8,st);
}

//wime本体が使うMsg()
bool Msg(char dummy UNUSED,const char* fmt,...)
{
    va_list vl;
    va_start(vl,fmt);
    print_logfile_str(LogMark,"");
    vfprintf(LogFile,fmt,vl);
    va_end(vl);
    MsgSn %= SN_MAX;
    return true;
}

/*
  かんなのパケットを受信する
  !!! いいかげん関数名を変えよう
*/
DWORD WINAPI recv_xim(void* h0)
{
    Array chbuf;
    int rsz,fd;
    CanHeader* ch;
    HWND h=(HWND)h0;

    ArNew(&chbuf,1,NULL);
    ArAlloc(&chbuf,CANNAHEADERSIZE);

    while((fd = ImSelect()) > 0){
	ch = ArAdr(&chbuf);
	rsz = ImRead(ch,CANNAHEADERSIZE);
	if(rsz <= 0){ //切断
	    DEBUGLOG(CH_GLOBAL,"disconnect fd %d\n",fd);
	    ImDisconnect();
	    CloseConnection(fd);
	    continue;
	}
	if(ch->Major>0 && (ch->Length = Swap2(ch->Length))>0){
	    //initはmajor=0になるのでデータは読み込まれない。
	    ch = ArAlloc(&chbuf,CANNAHEADERSIZE+ch->Length);
	    ImRead(ch+1,ch->Length);
	}
	if(((ch->Minor<<8)|ch->Major) == WIME_Log){
	    log_req((Req15_t*)ch);
	    continue;
	}
	//DEBUGLOG(CH_CANNA,"canna packet:major 0x%x, minor 0x%x, len %d [%*D]\n",ch->Major,ch->Minor,ch->Length,(ch->Major==0?CANNEADERSIZE:ch->Length),(ch->Major==0?ch:ch+1));
	SendMessageW(h,WM_CANNA_PACKET,(WPARAM)ch,(LPARAM)fd);
    }
    ArDelete(&chbuf);
    DEBUGLOG(CH_GLOBAL,"EXIT\n");
    return 0;
}

/*
  ime制御用のダミーウィンドウ
 */
HWND NewWin(void)
{
    HWND h = CreateWindow(ClassName,"",WS_POPUP,0,0,0,0,NULL,NULL,NULL,NULL);
    DEBUGLOG(CH_GLOBAL,"window %p, def-ime-wnd %p\n",h,ImmGetDefaultIMEWnd(h));
    return h;
}

static int aux_input(HWND h)
{
    int16_t cxn;

    HIMC imc = ImmGetContext(h);
    CannaContext_t* cx = FindContext(h,&cxn);
    DEBUGDO(CH_GLOBAL,{MESG("context %hd, xid 0x%x\n",cxn,cx->XWin); DbgComp(imc,__func__);});

    if(cx->XWin != 0) //念のためチェックしておく
	ImAuxInput(cx->XWin);
    ImmReleaseContext(h,imc);
    return 0;
}

void dbg_filter_msg(int bit,uint16_t cxn,const CannaContext_t* cx,HWND wh,UINT msg,WPARAM wp,LPARAM lp);
void dbg_imc(HWND wh,const CannaContext_t* cx);
void dbg_cx_info(uint16_t cxn,const CannaContext_t* cx,HWND w);

int notify_cmd_to_cx_flag(unsigned cmd)
{
    int a=0;
    switch(cmd){
    case IMN_OPENCANDIDATE:
	a = CATCH_OPEN_CAND;
	break;
    case IMN_CHANGECANDIDATE:
	a = CATCH_CHG_CAND;
	break;
    }
    return a;
}

//ウィンドウプロシージャ
LRESULT CALLBACK wnd_proc(HWND wh,UINT msg,WPARAM wp,LPARAM lp)
{
    LRESULT r=0;
    CannaContext_t* cx;
    int16_t cxn;

    DEBUGLOG(CH_WINMSG,"msg 0x%x window %p\n",msg,wh);

    switch(msg){
    case WM_IME_COMPOSITION: //10f
	cx = FindContext(wh,&cxn);
	DEBUGDO(CH_COMPOSITION,dbg_filter_msg(PROC_COMP_MSG,cxn,cx,wh,msg,wp,lp));
	DEBUGDO(CH_COMPO_IMC,dbg_imc(wh,cx));
	if(cx!=NULL){
	    if(lp==(GCS_RESULTSTR|GCS_RESULTCLAUSE) && (cx->Flags & SEND_KEY)==0){
		/* 読みなしで結果文字列だけ→パレットツールなどからの入力
		   [3.3.0]漢字モードでスペースキーを押すと全角スペースになるが、読み文字
		   列に半角スペースがセットされない。そのため外部入力と区別がつかない。
		   if()の条件にwm_wime_send_key()が呼ばれていないかを追加してみる。
		*/
		DEBUGLOG(CH_GLOBAL,"aux input\n");
		r = aux_input(wh);
		break;
	    }
	    if((cx->Flags & PROC_COMP_MSG)!=0){
		r = cx->ImeWnd!=NULL ? SendMessageW(cx->ImeWnd,msg,wp,lp):DefWindowProc(wh,msg,wp,lp);
	    }
	    cx->Flags &= ~SEND_KEY;
	}
	break;
    case WM_IME_NOTIFY: //282
	cx = FindContext(wh,&cxn);
	DEBUGDO(CH_NOTIFY,dbg_filter_msg(PROC_NOTIFY_MSG,cxn,cx,wh,msg,wp,lp));
	DEBUGDO(CH_NOTI_IMC,dbg_imc(wh,cx));
	if(cx!=NULL){
	    cx->Flags |= notify_cmd_to_cx_flag(wp);
	    if((cx->Flags & TRAP_OPEN_CAND)!=0 && (cx->Flags & (CATCH_OPEN_CAND|CATCH_CHG_CAND))!=0){
		DEBUGLOG(CH_NOTIFY,"catch open|change candi\n");
		break;
	    }
	    if((cx->Flags & PROC_NOTIFY_MSG)!=0){
		r = cx->ImeWnd!=NULL ? SendMessageW(cx->ImeWnd,msg,wp,lp):DefWindowProc(wh,msg,wp,lp);
	    }
	}
	break;
    case WM_IME_REQUEST: //288
	cx = FindContext(wh,&cxn);
	DEBUGDO(CH_REQUEST,dbg_filter_msg(0,cxn,cx,wh,msg,wp,lp));
	if(cx!=NULL && wp==IMR_RECONVERTSTRING && lp==0){ //再変換
	    cx->Flags |= PENDING_RECONV;
	    DEBUGLOG(CH_REQUEST,"context %hd: reconvert. pending\n",cxn);
	}
	if(cx!=NULL && wp==IMR_QUERYCHARPOSITION && (cx->Flags&TRAP_OPEN_CAND)!=0){
	    /*[r199]cannaとして使っているとき、このコマンドに反応せずにいたら左上(0,0)にツールチップが表示される(wimeのときはカーソル位置に表示される)。cannaでは必要ないので、画面外になるようにしてみる。
	      wimeのときでもTRAP_OPEN_CANDがセットされるときはあるが、そのときは変換候補ウィンドウは使わないので、ツールチップもいらないだろう。
	     */
	    IMECHARPOSITION* cp = (IMECHARPOSITION*)r;
	    cp->dwSize = sizeof(IMECHARPOSITION);
	    cp->dwCharPos = 0;
	    cp->pt = (POINT){-1,-1};
	    cp->cLineHeight = 0;
	    cp->rcDocument = (RECT){0};
	    r = 1;
	}
	break;

	/* !!!
	   immメッセージを正しいime-windowに送る。
	   memoの[imcとime windowについて]参照
	*/
    case WM_IME_STARTCOMPOSITION:	//0x10d
    case WM_IME_ENDCOMPOSITION:		//0x10e
    case WM_IME_SETCONTEXT:		//0x281
    case WM_IME_CONTROL:		//0x283
    case WM_IME_COMPOSITIONFULL:	//0x284
    case WM_IME_SELECT:			//0x285
    case WM_IME_CHAR:			//0x286
    case WM_IME_KEYDOWN:		//0x290
    case WM_IME_KEYUP:			//0x291
	cx = FindContext(wh,&cxn);
	DEBUGDO(CH_IMEMSG,dbg_filter_msg(-1,cxn,cx,wh,msg,wp,lp));
	if(cx==NULL || cx->ImeWnd==NULL){
	    DEBUGLOG(CH_IMEMSG,
		  "window %p, def-ime-wnd %p, msg 0x%x, context %hd:canna context or ImeWnd not found\n",
		  wh,ImmGetDefaultIMEWnd(wh),msg,cxn);
	    r = DefWindowProc(wh,msg,wp,lp);
	    break;
	}
	if((cx->Flags & PROC_NOTIFY_MSG) == 0){
	    DEBUGLOG(CH_IMEMSG,
		  "window %p, def-ime-wnd %p, msg 0x%x, context %hd, Flags 0x%x: drop message\n",
		  wh,ImmGetDefaultIMEWnd(wh),msg,cxn,cx->Flags);
	    break;
	}
	DEBUGLOG(CH_IMEMSG,"window %p, def-ime-wnd %p, context %hd:send 0x%x to ime %p\n",
	      wh,ImmGetDefaultIMEWnd(wh),cxn,msg,cx->ImeWnd);
	r = SendMessageW(cx->ImeWnd,msg,wp,lp);
	break;
    case WM_CANNA_PACKET: //wp=header lp=fd
    {
	WMCANNAPROTO func = NULL;
	CanHeader* ch = (CanHeader*)wp;
	if(ch->Minor<=WIME_MINOR && ch->Major<CanFunMax[ch->Minor])
	    func = WmCannaTab[ch->Minor][ch->Major];
	if(func != NULL)
	    r = (LRESULT)func(ch,(int)lp);
	else{
	    FATALLOG(CH_GLOBAL,"*** ILLEGAL CANNA PROTOCOL:minor 0x%x major 0x%x\n",ch->Minor,ch->Major);
	    r = (LRESULT)true;
	}
	break;
    }
    default:
	r = DefWindowProc(wh,msg,wp,lp);
    }
    DEBUGLOG(CH_WINMSG,"msg 0x%x wp 0x%x lp 0x%x -- return code 0x%lx\n",msg,(unsigned)wp,(unsigned)lp,r);
    return r;
}

void dbg_imc(HWND wh,const CannaContext_t* cx)
{
    if(cx != NULL){
	HIMC imc = ImmGetContext(wh);
	if(ImcClauseInfo(imc,GCS_COMPSTR,NULL) > 0){
	    DbgComp(imc,__func__);
	}
	ImmReleaseContext(wh,imc);
    }
}

const char* msg_name(unsigned n);
const char* dbg_wm_notify_msg(unsigned wp);
const char* dbg_wm_request_msg(unsigned wp);
Array* dbg_wm_comp_msg(unsigned lp);

void dbg_filter_msg(int bit,uint16_t cxn,const CannaContext_t* cx,HWND wh,UINT msg,WPARAM wp,LPARAM lp)
{
    char* s;
    if(cx!=NULL && (cx->Flags & bit)!=0)
	s = "through";
    else
	s = "filtering";
    MESG("%s (%s) window %p wp 0x%x lp 0x%x cx %p\n",msg_name(msg),s,wh,(unsigned)wp,(unsigned)lp,cx);
	
    switch(msg){
    case WM_IME_NOTIFY:
	MESG("    wp %s\n",dbg_wm_notify_msg(wp));
	break;
    case WM_IME_COMPOSITION:
    {
	Array* str = dbg_wm_comp_msg(lp);
	MESG("    lp %s\n",(char*)ArAdr(str));
	free(ArDelete(str));
	break;
    }
    case WM_IME_REQUEST:
	MESG("    wp %s lp %x\n",dbg_wm_request_msg(wp),(unsigned)lp);
    }
    dbg_cx_info(cxn,cx,wh);
}

#define CASE_STR(x) case x:s=#x;break

const char* dbg_wm_notify_msg(unsigned wp)
{
    const char* s = "unknown";
    switch(wp){
	CASE_STR(IMN_CLOSESTATUSWINDOW);
	CASE_STR(IMN_OPENSTATUSWINDOW);
	CASE_STR(IMN_CHANGECANDIDATE);
	CASE_STR(IMN_CLOSECANDIDATE);
	CASE_STR(IMN_OPENCANDIDATE);
	CASE_STR(IMN_SETCONVERSIONMODE);
	CASE_STR(IMN_SETSENTENCEMODE);
	CASE_STR(IMN_SETOPENSTATUS);
	CASE_STR(IMN_SETCANDIDATEPOS);
	CASE_STR(IMN_SETCOMPOSITIONFONT);
	CASE_STR(IMN_SETCOMPOSITIONWINDOW);
	CASE_STR(IMN_SETSTATUSWINDOWPOS);
	CASE_STR(IMN_GUIDELINE);
	CASE_STR(IMN_PRIVATE);
    }
    return s;
}

const char* dbg_wm_request_msg(unsigned wp)
{
    const char *s = "unknown";
    switch(wp){
	CASE_STR(IMR_CANDIDATEWINDOW);
	CASE_STR(IMR_COMPOSITIONFONT);
	CASE_STR(IMR_COMPOSITIONWINDOW);
	CASE_STR(IMR_CONFIRMRECONVERTSTRING);
	CASE_STR(IMR_DOCUMENTFEED);
	CASE_STR(IMR_QUERYCHARPOSITION);
	CASE_STR(IMR_RECONVERTSTRING);
    }
    return s;
}

Array* dbg_wm_comp_msg(unsigned lp)
{
    BitDesc bits[]={
	BITDESC(GCS_COMPREADSTR),
	BITDESC(GCS_COMPREADATTR),
	BITDESC(GCS_COMPREADCLAUSE),
	BITDESC(GCS_COMPSTR),
	BITDESC(GCS_COMPATTR),
	BITDESC(GCS_COMPCLAUSE),
	BITDESC(GCS_CURSORPOS),
	BITDESC(GCS_DELTASTART),
	BITDESC(GCS_RESULTREADSTR),
	BITDESC(GCS_RESULTREADCLAUSE),
	BITDESC(GCS_RESULTSTR),
	BITDESC(GCS_RESULTCLAUSE),
	{0,NULL}
    };
    return FlagStr(lp,bits,NULL);
}

const char* msg_name(unsigned n)
{
    const char* s;
    static char buf[2+sizeof(n)*2+1];

    switch(n){
	CASE_STR(WM_IME_SETCONTEXT);
	CASE_STR(WM_IME_COMPOSITION);
	CASE_STR(WM_IME_NOTIFY);
	CASE_STR(WM_IME_STARTCOMPOSITION);
	CASE_STR(WM_IME_ENDCOMPOSITION);
	CASE_STR(WM_IME_CONTROL);
	CASE_STR(WM_IME_COMPOSITIONFULL);
	CASE_STR(WM_IME_SELECT);
	CASE_STR(WM_IME_CHAR);
	CASE_STR(WM_IME_REQUEST);
	CASE_STR(WM_IME_KEYDOWN);
	CASE_STR(WM_IME_KEYUP);
    default:
	sprintf(buf,"0x%x",n);
	s = buf;
    }
    return s;
}

void dbg_cx_info(uint16_t cxn,const CannaContext_t* cx,HWND w)
{
    void *imewnd=NULL,*defimc=NULL;
    HIMC imc = ImmGetContext(w);
    Array flagstr;

    ArNew(&flagstr,1,NULL);
    if(cx==NULL){
	ArAddChar(&flagstr,0); //空文字列
    }else{
	imewnd = cx->ImeWnd;
	defimc = cx->DefImc;
	BitDesc bits[]={
	    BITDESC(OPEN_STATUS_WINDOW),
	    BITDESC(PROC_NOTIFY_MSG),
	    BITDESC(PROC_COMP_MSG),
	    BITDESC(PENDING_RECONV),
	    BITDESC(SEND_KEY),
	    BITDESC(TRAP_OPEN_CAND),
	    BITDESC(CATCH_OPEN_CAND),
	    BITDESC(CATCH_CHG_CAND),
	    BITDESC(USE_UTF16LE),
	    BITDESC(USE_UTF16BE),
	    {0,NULL}};
	ArPrint(&flagstr," Flags=");
	FlagStr(cx->Flags,bits,&flagstr);
	ArPrint(&flagstr,"(0x%x)",cx->Flags);
    }
    MESG("cxn %hd cx %p cx->ImeWnd %p cx->DefImc %p wnd %p imc %p%s\n",cxn,cx,imewnd,defimc,w,imc,(char*)ArAdr(&flagstr));
    ImmReleaseContext(w,imc);
    ArDelete(&flagstr);
}

//(C) 2008 thomas
