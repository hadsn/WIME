// -*- coding:euc-jp -*-
#define _GNU_SOURCE
#include <windows.h>
#include <stdio.h>
#include <getopt.h>
#include <imm.h>
#include "canna.h"
#include "io/wimeio.h"
#include "so/wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "lib/log.h"
#include "apisup.h"
#include "version.h"

enum {
    WM_SET_COMP_STR = WM_APP,
    WM_CANNA_PACKET,	/* minor=0 */
};

struct GlobalData_t WimeData;
WMCANNAPROTO* WmCannaTab[2];
unsigned CanFunMax[2];
char ClassName[]="ImeBridge";
FILE* LogFile; //指定がなければstdoutにする
char* LogFileName;

HWND NewWin();
DWORD WINAPI recv_xim(void* h);
LRESULT CALLBACK wnd_proc(HWND wh,UINT msg,WPARAM wp,LPARAM lp);
int cmdline_opt(int ac,char* av[],int* use_tcp);
void init_cb(void);
void open_logfile(const char* fn,const char* mode);
void reg_class(void);
void ime_info(void);
void set_wimedata(struct GlobalData_t* wd);

int main(int ac,char* av[])
{
    MSG msg;
    int socket_num,st,use_tcp;

    LogFile = stdout;
    LogMark = 'w';
    init_cb();
    socket_num = cmdline_opt(ac,av,&use_tcp);
    setbuf(stdout,NULL);
    reg_class();

    set_wimedata(&WimeData); //メモ書き参照
    if(!ImInit(socket_num,use_tcp)){
	return 1;
    }

    st = ImReadSetting(&WimeData); //まだログは出せない
    InitClientData();
    HWND msgwin = NewWin();
    HANDLE th = CreateThread(NULL,0,recv_xim,msgwin,0,NULL);
    ShmStartServer();

    LOG(CH_GLOBAL,LOG_DEBUG,MESG("wime " WIME_VER_STR " %dbit " __DATE__ " " __TIME__ "\n",(int)sizeof(void*)*8));
    LOG(CH_GLOBAL,LOG_DEBUG,MESG("load hinshi file:status %d\n",st));
    LOG(CH_GLOBAL,LOG_DEBUG,ime_info());

    ImSemStart(); //ぎりぎりまで待つ
    while(GetMessage(&msg, NULL, 0, 0) >0) {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }

    CloseHandle(th);
    DestroyWindow(msgwin);
    LOG(CH_GLOBAL,LOG_DEBUG,MESG("EXIT\n"));
    return 0;
}

//ヘッダファイルにメモ書きあり。
static bool set_read_a(HIMC imc,const char* yomi)
{
    //??? ImmSetCompositionStringAのreadlenは文字数なのか？
    char* ys = EjToSj(NULL,yomi);
    bool r = ImmSetCompositionStringA(imc,SCS_SETSTR,NULL,0,ys,strlen(ys)/2);
    free(ys);
    return r;
}
static bool set_read_w(HIMC imc,const char* yomi)
{
    uint16_t* u = EjToU16(NULL,yomi);

    /* ???
       '￣'(FULLWIDTH MACRON)としてe-a1b1をu16にするとU-ffe3になるが、これを読み文字列にすると
       ImmNotifyIME()が失敗する。これがあれば'~'に書き換える。
       '＼'(e-a1c0,u-ff3c)も同様だった。なぜだろう？
    */
    for(uint16_t* p=u; *p!=0; ++p){
	switch(*p){
	case 0xffe3: *p='~'; break;
	case 0xff3c: *p='\\'; break;
	}
    }

    bool r = ImmSetCompositionStringW(imc,SCS_SETSTR,u,WcLen(u)*2,NULL,0);
    free(u);
    return r;
}

//csから文節番号nの文節をeucjpで返す。nは固定文節数を引いておくこと。
char* get_cl_a(const COMPOSITIONSTRING* cs,int str_offset,int cl_offset,int n,int nlen)
{
    const int32_t* cl = (typeof(cl))((const char*)cs + cl_offset);
    return SjToEj(NULL,(const char*)cs + str_offset + cl[n],cl[n+nlen]-cl[n]);
}
char* get_cl_w(const COMPOSITIONSTRING* cs,int str_offset,int cl_offset,int n,int nlen)
{
    const int32_t* cl = (typeof(cl))((const char*)cs + cl_offset);
    return U16ToEj(NULL,(const uint16_t*)((const char*)cs + str_offset) + cl[n],cl[n+nlen]-cl[n]);
}

typedef void* (*cv_fun_t)(void*,const void*,int);
void* get_cs(HIMC imc,DWORD index,LONG WINAPI (*gcs)(HIMC,DWORD,LPVOID,DWORD),cv_fun_t cv)
{
    int sz;
    void* ej=NULL;

    if((sz = (*gcs)(imc,index,NULL,0)) > 0){
	void* buf = malloc(sz+sizeof(int));
	(*gcs)(imc,index,buf,sz);
	*(int*)((char*)buf+sz) = 0;
	ej = cv(NULL,buf,-1);
	free(buf);
    }
    return ej;
}
void* get_cs_a(HIMC imc,DWORD index)
{
    return get_cs(imc,index,ImmGetCompositionStringA,(cv_fun_t)SjToEj);
}
void* get_cs_w(HIMC imc,DWORD index)
{
    return get_cs(imc,index,ImmGetCompositionStringW,(cv_fun_t)U16ToEj);
}

void set_wimedata(struct GlobalData_t* wd)
{
    int p = ImmGetProperty(GetKeyboardLayout(0),IGP_PROPERTY);
    if(p & IME_PROP_UNICODE){
	wd->SetRead = set_read_w;
	wd->GetClause = get_cl_w;
	wd->GetCandidate = GetCandidateW;
	wd->CharSize = 1;
	wd->SetCompStr = ImmSetCompositionStringW;
	wd->GetCompStr = get_cs_w;
    }else{
	wd->SetRead = set_read_a;
	wd->GetClause = get_cl_a;
	wd->GetCandidate = GetCandidateA;
	wd->CharSize = 2;
	wd->SetCompStr = ImmSetCompositionStringA;
	wd->GetCompStr = get_cs_a;
    }
    wd->CandIndexStart=0; //((p & IME_PROP_CANDLIST_START_FROM_1) ? 1 : 0); ???余計おかしくなった。

    HKL kl = GetKeyboardLayout(0);
    unsigned sz = ImmGetIMEFileName(kl,NULL,0);
    if(sz > 0){
	/* レジストリの内容によってはフルパスが得られるときがあるので、
	   ベース名のみ取り出して調べる。*/
	///??? sオプションで処理するようにした方がいいのでは？
	char ime_fn[sz+1],*bn=ime_fn,dum[sz+1];
	ImmGetIMEFileName(kl,ime_fn,sz);
	GetFullPathName(ime_fn,sz,dum,&bn);
	if(strncasecmp(bn,"atok",4) == 0)
	    wd->GetCandidate = GetCandidateAtok;
    }
}

void reg_class(void)
{
    WNDCLASS wc={0};

    wc.lpfnWndProc = wnd_proc;
    wc.lpszClassName = ClassName;
    if(!RegisterClass(&wc)){
	ERR("fail RegisterClass '%s'\n",ClassName);
	exit(1);
    }
}

#ifndef IME_PROP_ACCEPT_WIDE_VKEY
#define IME_PROP_ACCEPT_WIDE_VKEY 0x20
#endif

void ime_info(void)
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
    char ime_fn[sz+1];    //char ime_fn[sz+1]={[0]=0} はエラーになった。
    ime_fn[0] = 0;
    ImmGetIMEFileName(kl,ime_fn,sz);

    sz = ImmGetDescription(kl,NULL,0);
    char desc[sz+1];
    desc[0] = 0;
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

bool AtInit(WMCANNAPROTO* tab[]);

//ime別の初期化
bool ime_sp(const char* ime)
{
    typedef bool (*init_func_t)(WMCANNAPROTO* tab[]);
    struct{
	char* name;
	init_func_t init;
    } tab[]={
	{"atok",AtInit},
	{NULL,NULL}
    };
    int n;
    
    for(n=0; tab[n].name!=NULL && strcmp(tab[n].name,ime)!=0; ++n)
	;
    return tab[n].name!=NULL && tab[n].init(WmCannaTab);
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
    Reply2(WIME_LOG&0xff,WIME_LOG>>8,st);
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
	    LOG(CH_GLOBAL,LOG_DEBUG,MESG("disconnect fd %d\n",fd));
	    ImDisconnect();
	    CloseConnection(fd);
	    continue;
	}
	if(ch->Major>0 && (ch->Length = Swap2(ch->Length))>0){
	    ch = ArAlloc(&chbuf,CANNAHEADERSIZE+ch->Length);
	    ImRead(ch+1,ch->Length);
	}
	if(ch->Minor*256+ch->Major == WIME_LOG){
	    log_req((Req15_t*)ch);
	    continue;
	}
	//LOG("canna packet:major 0x%x, minor 0x%x, len %d\n",ch->Major,ch->Minor,ch->Length);
	SendMessageW(h,WM_CANNA_PACKET,(WPARAM)ch,(LPARAM)fd);
    }
    ArDelete(&chbuf);
    LOG(CH_GLOBAL,LOG_DEBUG,MESG("EXIT\n"));
    return 0;
}

/*
  ime制御用のダミーウィンドウ
 */
HWND NewWin(void)
{
    HWND h = CreateWindow(ClassName,"",WS_POPUP,0,0,0,0,NULL,NULL,NULL,NULL);
    LOG(CH_GLOBAL,LOG_DEBUG,MESG("window %p, def-ime-wnd %p\n",h,ImmGetDefaultIMEWnd(h)));
    return h;
}

//ウィンドウプロシージャのコールバック関数テーブル
void init_cb(void)
{
    static WMCANNAPROTO wm_canna_tab0[]={
	Init,		/*00*/		NULL,		/*01*/
	Finalize,	/*02*/		CreateContext,	/*03*/
	DupContext,	/*04*/		CloseContext,	/*05*/
	GetDicList,	/*06*/		GetDirList,	/*07*/
	MountDic,	/*08*/		UnmountDic,	/*09*/
	RemountDic,	/*0a*/		MountDicList,	/*0b*/
	QueryDic,	/*0c*/		DefineWord,	/*0d*/
	DeleteWord,	/*0e*/		BeginConv,	/*0f*/
	EndConv,	/*10*/		GetCandiList,	/*11*/
	GetYomi,	/*12*/		SubstYomi,	/*13*/
	StoreYomi,	/*14*/		StoreRange,	/*15*/
	GetLastYomi,	/*16*/		FlushYomi,	/*17*/
	RemoveYomi,	/*18*/		GetSimpleKanji,	/*19*/
	ResizePause,	/*1a*/		GetHinshi,	/*1b*/
	GetLex,		/*1c*/		GetStatus,	/*1d*/
	SetLocale,	/*1e*/		AutoConv,	/*1f*/
	QueryExt,	/*20*/		SetAppName,	/*21*/
	NoticeGroup,	/*22*/		NULL,		/*23*/
	KillServer	/*24*/
    };
    static WMCANNAPROTO wm_canna_tab1[]={
	NULL,		/*00*/		GetServerInfo,	/*01*/
	GetAcl,		/*02*/		CreateDic,	/*03*/
	DeleteDic,	/*04*/		RenameDic,	/*05*/
	GetWordTextDic,	/*06*/		ListDic,	/*07*/
	Sync,		/*08*/		ChmodDic,	/*09*/
	CopyDic,	/*0a*/

	//追加	so/pkt.hのプロトコル番号,canna.cのquery_ext()も変更すること
	OpenDialog,
	SetCompositionWin,
	GetCompositionWin,
	SendKey,
	EnableIme,
	MoveShadowWin,
	SetCompositionFont,
	GetCompositionStr,
	SetCandidateWin,
	RegXWin,
	GetResultStr,
	SetResultStr,
	Reconvert,
	SetImeFocus,
	ShowToolbar,
	GetStyleList,
	ReloadConf,
	FlushMsg,
	ShowCandidateWin,
	SelectCandidate,
	CloseCandidateWin,
	DumpContext,
	SetDebugChannel,
    };

    CanFunMax[0] = ITEMS(wm_canna_tab0);
    CanFunMax[1] = ITEMS(wm_canna_tab1);

    WmCannaTab[0] = wm_canna_tab0;
    WmCannaTab[1] = wm_canna_tab1;

}

int aux_input(HWND h)
{
    int16_t cxn;

    HIMC imc = ImmGetContext(h);
    CannaContext_t* cx = FindContext(h,&cxn);
    LOG(CH_GLOBAL,LOG_DEBUG,{MESG("context %hd, xid 0x%x\n",cxn,cx->XWin); DbgComp(imc,__func__);});

    if(cx->XWin != 0) //念のためチェックしておく
	ImAuxInput(cx->XWin);
    ImmReleaseContext(h,imc);
    return 0;
}

void dbg_filter_msg(int bit,const CannaContext_t* cx,HWND wh,UINT msg,WPARAM wp,LPARAM lp);
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

    LOG(CH_WINMSG,LOG_DEBUG,MESG("msg 0x%x window %p\n",msg,wh));

    switch(msg){
    case WM_IME_COMPOSITION: //10f
	cx = FindContext(wh,&cxn);
	LOG(CH_COMPOSITION|CH_WINMSG,LOG_DEBUG,{
		dbg_filter_msg(PROC_COMP_MSG,cx,wh,msg,wp,lp);
		dbg_cx_info(cxn,cx,wh);
		dbg_imc(wh,cx);});
	if(cx!=NULL && lp==(GCS_RESULTSTR|GCS_RESULTCLAUSE) && !(cx->Flags & SEND_KEY)){
	    /* 読みなしで結果文字列だけ→パレットツールなどからの入力
	       [3.3.0]漢字モードでスペースキーを押すと全角スペースになるが、読み文字
	       列に半角スペースがセットされない。そのため外部入力と区別がつかない。
	       if()の条件にwm_wime_send_key()が呼ばれていないかを追加してみる。
	    */
	    LOG(CH_GLOBAL,LOG_DEBUG,MESG("aux input\n"));
	    r = aux_input(wh);
	}else if(cx!=NULL && (cx->Flags & PROC_COMP_MSG)!=0){
	    r = cx->ImeWnd!=NULL ? SendMessageW(cx->ImeWnd,msg,wp,lp):DefWindowProc(wh,msg,wp,lp);
        }
	if(cx != NULL)
	    cx->Flags &= ~SEND_KEY;
	break;
    case WM_IME_NOTIFY: //282
	cx = FindContext(wh,&cxn);
	LOG(CH_NOTIFY|CH_WINMSG,LOG_DEBUG,{
		dbg_filter_msg(PROC_NOTIFY_MSG,cx,wh,msg,wp,lp);
		dbg_cx_info(cxn,cx,wh);
		dbg_imc(wh,cx);});
	if(cx!=NULL){
	    if((cx->Flags & IN_FOCUS)==0){
		LOG(CH_NOTIFY|CH_WINMSG,LOG_DEBUG,MESG("outside focus %hd\n",cxn));
		break;
	    }
	    cx->Flags |= notify_cmd_to_cx_flag(wp);
	    if((cx->Flags & TRAP_OPEN_CAND)!=0 && (cx->Flags & (CATCH_OPEN_CAND|CATCH_CHG_CAND))!=0){
		LOG(CH_NOTIFY|CH_WINMSG,LOG_DEBUG,MESG("catch open|change candi\n"));
		break;
	    }
	    if((cx->Flags & PROC_NOTIFY_MSG)!=0){
		r = cx->ImeWnd!=NULL ? SendMessageW(cx->ImeWnd,msg,wp,lp):DefWindowProc(wh,msg,wp,lp);
	    }
	}
	break;
    case WM_IME_REQUEST: //288
	cx = FindContext(wh,&cxn);
	LOG(CH_REQUEST|CH_WINMSG,LOG_DEBUG,{
		dbg_filter_msg(0,cx,wh,msg,wp,lp);
		dbg_cx_info(cxn,cx,wh);});
	if(cx!=NULL && wp==IMR_RECONVERTSTRING && lp==0){ //再変換
	    cx->Flags |= PENDING_RECONV;
	    LOG(CH_REQUEST|CH_WINMSG,LOG_DEBUG,MESG("context %hd: reconvert. pending\n",cxn));
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
	LOG(CH_IMEMSG|CH_WINMSG,LOG_DEBUG,{
		dbg_filter_msg(0,cx,wh,msg,wp,lp);
		dbg_cx_info(cxn,cx,wh);});
	if(cx!=NULL && ((cx->Flags & PROC_NOTIFY_MSG)==0 || (cx->Flags & IN_FOCUS)==0)){
	    LOG(CH_IMEMSG|CH_WINMSG,LOG_DEBUG,
		MESG("window %p, def-ime-wnd %p, msg 0x%x, context %hd, Flags 0x%x: drop message\n",
		     wh,ImmGetDefaultIMEWnd(wh),msg,cxn,cx->Flags));
	    break;
	}
	if(cx==NULL || cx->ImeWnd==NULL){
	    LOG(CH_IMEMSG|CH_WINMSG,LOG_DEBUG,
		MESG("window %p, def-ime-wnd %p, msg 0x%x, context %hd:canna context or ImeWnd not found\n",
		     wh,ImmGetDefaultIMEWnd(wh),msg,cxn));
	    r = DefWindowProc(wh,msg,wp,lp);
	    break;
	}
	LOG(CH_IMEMSG|CH_WINMSG,LOG_DEBUG,
	    MESG("window %p, def-ime-wnd %p, context %hd:send 0x%x to ime %p\n",
		 wh,ImmGetDefaultIMEWnd(wh),cxn,msg,cx->ImeWnd));
	r = SendMessageW(cx->ImeWnd,msg,wp,lp);
	break;
    case WM_CANNA_PACKET: //wp=header lp=fd
    {
	WMCANNAPROTO func = NULL;
	CanHeader* ch = (CanHeader*)wp;
	if(ch->Minor<2 && ch->Major<CanFunMax[ch->Minor])
	    func = WmCannaTab[ch->Minor][ch->Major];
	if(func != NULL)
	    r = (LRESULT)func(ch,(int)lp);
	else{
	    LOG(CH_GLOBAL|CH_WINMSG,LOG_CRITICAL,MESG("*** ILLEGAL CANNA PROTOCOL:minor 0x%x major 0x%x\n",ch->Minor,ch->Major));
	    r = (LRESULT)true;
	}
	break;
    }
    default:
	r = DefWindowProc(wh,msg,wp,lp);
    }
    LOG(CH_WINMSG,LOG_DEBUG,MESG("msg 0x%x wp 0x%x lp 0x%x -- return code 0x%lx\n",msg,(unsigned)wp,(unsigned)lp,r));
    return r;
}

#define COPYRIGHT "(C) 2008-2018 thomas"

void usage(int exit_code)
{
    printf("wime [options] [logfile]\n"
	   "  -i,--inet [port]	tcp connection\n"
	   "  -s ime		specify ime\n"
	   "  -p num		socket number(>=1)\n"
	   "  -v[num],-v-\t	verbose (on,off)\n"
	   "  --channel <str>	debug channel\n"
	   "  --version		print version\n"
	   "  -h,--help		this message\n"
	   COPYRIGHT "\n"
	);
    exit(exit_code);
}

void print_version(void)
{
    printf(
	WIME_VER_STR "\n"
	COPYRIGHT "\n"
	);
}

/*
  fnをオープンしてLogFileにセットする
*/
void open_logfile(const char* fn,const char* mode)
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

//ソケットのオプション番号を返す
int cmdline_opt(int ac,char* av[],int* use_tcp)
{
    struct option longopt[]={
	{"channel",	required_argument,NULL,'ch'},
	{"help",	no_argument,NULL,'h'},
	{"inet",	optional_argument,NULL,'i'},
	{"version",	no_argument,NULL,'vsn'},
	{NULL,0,NULL,0}
    };
    int c,socket_num=0,use_v=0;

    *use_tcp = 0;
    ParseChannelEnv(CH_GLOBAL);
    while((c = getopt_long(ac,av,"hi::p:e:v::",longopt,NULL)) != -1){
	switch(c){
	case 'h':
	    usage(0);
	case 'p':
	    socket_num = atoi(optarg);
	    break;
	case 'i':
	    *use_tcp = (optarg==NULL ? -1 : atoi(optarg));
	    break;
	case 'e':
	    if(!ime_sp(optarg)){
		ERR("no available ime '%s'\n",optarg);
		exit(1);
	    }
	    break;
	case 'v':
	    if(use_v == 0)
		Verbose = 0;//環境変数で設定されたかもしれないので、1回目のvのとき0に戻す。
	    ++use_v;
	    if(optarg==NULL){
		++Verbose;
	    }else if(strcmp(optarg,"-")==0)
		Verbose = 0;
	    else if(isdigit(optarg[0]))
		Verbose = optarg[0]-'0';
	    else
		usage(1);
	    break;
	case 'ch':
	    ParseChannelStr(optarg);
	    break;
	case 'vsn':
	    print_version();
	    exit(0);
	default:
	    usage(1);
	}
    }
    open_logfile(LogFileName = av[optind],"w");
    
    return socket_num;
}

void dbg_imc(HWND wh,const CannaContext_t* cx)
{
    if(cx != NULL){
	HIMC imc = ImmGetContext(wh);
	if(ClauseLen(imc,cx) > 0){
	    DbgComp(imc,__func__);
	}
	ImmReleaseContext(wh,imc);
    }
}

const char* msg_name(unsigned n);
const char* dbg_wm_notify_msg(unsigned wp);
const char* dbg_wm_request_msg(unsigned wp);
Array* dbg_wm_comp_msg(unsigned lp);

void dbg_filter_msg(int bit,const CannaContext_t* cx,HWND wh,UINT msg,WPARAM wp,LPARAM lp)
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
	    BITDESC(IN_FOCUS),
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
