#define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <windows.h>
#include <imm.h>
#include <process.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include "canna.h"
#include "io/wimeio.h"
#include "so/wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
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
unsigned __stdcall recv_xim(void* h);
LRESULT CALLBACK wnd_proc(HWND wh,UINT msg,WPARAM wp,LPARAM lp);
int cmdline_opt(int ac,char *av[],int* use_tcp);
void init_cb(void);
void open_logfile(const char* fn,const char* mode);
void reg_class(void);
void ime_info(void);
void set_wimedata(struct GlobalData_t* wd);

int main(int ac,char* av[])
{
    MSG msg;
    HANDLE th;
    int socket_num,st,use_tcp;
    HWND msgwin;

    Verbose = 1;
    LogFile = stdout;
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
    th = (HANDLE)_beginthreadex(NULL,0,recv_xim,msgwin=NewWin(),0,NULL);
    WimeShmInit(LOGMARK);

    LOG("load hinshi file:status %d\n",st);
    VERBOSE(ime_info());

    WimeSemStart(); //ぎりぎりまで待つ
    while(GetMessage(&msg, NULL, 0, 0) >0) {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }

    WimeShmFin();
    CloseHandle(th);
    DestroyWindow(msgwin);
    LOG("EXIT\n");
    return 0;
}

//ヘッダファイルにメモ書きあり。
static bool set_read_a(HIMC imc,const char* yomi)
{
    //??? ImmSetCompositionStringAのreadlenは文字数なのか？
    char *ys = EjToSj(NULL,yomi);
    bool r = ImmSetCompositionStringA(imc,SCS_SETSTR,NULL,0,ys,strlen(ys)/2);
    free(ys);
    return r;
}
static bool set_read_w(HIMC imc,const char* yomi)
{
    uint16_t *u = EjToU16(NULL,yomi);

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
    const int32_t *cl = (typeof(cl))((const char*)cs + cl_offset);
    return SjToEj(NULL,(const char*)cs + str_offset + cl[n],cl[n+nlen]-cl[n]);
}
char* get_cl_w(const COMPOSITIONSTRING* cs,int str_offset,int cl_offset,int n,int nlen)
{
    const int32_t *cl = (typeof(cl))((const char*)cs + cl_offset);
    return U16ToEj(NULL,(const uint16_t*)((const char*)cs + str_offset) + cl[n],cl[n+nlen]-cl[n]);
}

typedef void* (*cv_fun_t)(void*,const void*,int);
void* get_cs(HIMC imc,DWORD index,LONG WINAPI (*gcs)(HIMC,DWORD,LPVOID,DWORD),cv_fun_t cv)
{
    int sz;
    void *ej=NULL ,*buf;

    if((sz = (*gcs)(imc,index,NULL,0)) > 0){
	buf = malloc(sz+sizeof(int));
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
    WNDCLASS wc;

    memset(&wc,0,sizeof(wc));
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
    typedef struct{
	int bit;
	const char *desc;
    } pair;
#define def_pair(x) {x,#x}

    HKL kl;
    unsigned sz;
    pair igp_prop[] = {def_pair(IME_PROP_AT_CARET),
		       def_pair(IME_PROP_SPECIAL_UI),
		       def_pair(IME_PROP_CANDLIST_START_FROM_1),
		       def_pair(IME_PROP_UNICODE),
		       def_pair(IME_PROP_COMPLETE_ON_UNSELECT),
		       def_pair(IME_PROP_END_UNLOAD),
		       def_pair(IME_PROP_KBD_CHAR_FIRST),
		       def_pair(IME_PROP_NEED_ALTKEY),
		       def_pair(IME_PROP_IGNORE_UPKEYS),
		       def_pair(IME_PROP_NO_KEYS_ON_CLOSE),
		       def_pair(IME_PROP_ACCEPT_WIDE_VKEY),
    };
    pair igp_ui[] = {def_pair(UI_CAP_2700),
		   def_pair(UI_CAP_ROT90),
		   def_pair(UI_CAP_ROTANY)};
    pair igp_comp[] = {def_pair(SCS_CAP_COMPSTR),
		     def_pair(SCS_CAP_MAKEREAD),
		     def_pair(SCS_CAP_SETRECONVERTSTRING)};
    pair igp_sel[] = {def_pair(SELECT_CAP_CONVERSION),
		    def_pair(SELECT_CAP_SENTENCE)};
    pair igp_ver[] = {def_pair(IMEVER_0310),
		    def_pair(IMEVER_0400)};
    pair igp_conv[] = {def_pair(IME_CMODE_NATIVE),
		       def_pair(IME_CMODE_KATAKANA),
		       def_pair(IME_CMODE_LANGUAGE),
		       def_pair(IME_CMODE_FULLSHAPE),
		       def_pair(IME_CMODE_ROMAN),
		       def_pair(IME_CMODE_CHARCODE),
		       def_pair(IME_CMODE_HANJACONVERT),
		       def_pair(IME_CMODE_SOFTKBD),
		       def_pair(IME_CMODE_NOCONVERSION),
		       def_pair(IME_CMODE_EUDC),
		       def_pair(IME_CMODE_SYMBOL),
		       def_pair(IME_CMODE_FIXED),
    };
    pair igp_sen[] = {def_pair(IME_SMODE_NONE),
		      def_pair(IME_SMODE_PLAURALCLAUSE),
		      def_pair(IME_SMODE_SINGLECONVERT),
		      def_pair(IME_SMODE_AUTOMATIC),
		      def_pair(IME_SMODE_PHRASEPREDICT),
		      def_pair(IME_SMODE_CONVERSATION),
    };
    Array buf;
#undef def_pair

    Array* bit_name(int val,pair* p,int ni,Array* buf){
	const char *sep = "";
	*(char*)ArAlloc(buf,1) = 0;
	while(--ni >= 0){
	    if(val & p->bit){
		ArExpand(buf,strlen(p->desc)+1);
		strcat(strcat(ArAdr(buf),sep),p->desc);
		sep = "|";
	    }
	    val &= ~p->bit;
	    ++p;
	}
	if(val != 0){
	    char b[128];
	    sprintf(b,"%s0x%x",sep,val);
	    ArExpand(buf,strlen(b));
	    strcat(ArAdr(buf),b);
	}
	return buf;
    }

    kl = GetKeyboardLayout(0);

    sz = ImmGetIMEFileName(kl,NULL,0);
    char ime_fn[sz+1];
    ime_fn[0]=0;
    ImmGetIMEFileName(kl,ime_fn,sz);

    sz = ImmGetDescription(kl,NULL,0);
    char desc[sz+1];
    desc[0]=0;
    ImmGetDescription(kl,desc,sz);

    ArNew(&buf,1,NULL);
    LOG("kb layout    0x%x\n",kl);
    LOG("ime filename '%s'\n",ime_fn);
    LOG("description  '%s'\n",desc);
    LOG("property\n");
    LOG("\tconersion     %s\n",ArAdr(bit_name(ImmGetProperty(kl,IGP_CONVERSION),igp_conv,ITEMS(igp_conv),&buf)));
    LOG("\time-version   %s\n",ArAdr(bit_name(ImmGetProperty(kl,IGP_GETIMEVERSION),igp_ver,ITEMS(igp_ver),&buf)));
    LOG("\tproperty      %s\n",ArAdr(bit_name(ImmGetProperty(kl,IGP_PROPERTY),igp_prop,ITEMS(igp_prop),&buf)));
    LOG("\tselect        %s\n",ArAdr(bit_name(ImmGetProperty(kl,IGP_SELECT),igp_sel,ITEMS(igp_sel),&buf)));
    LOG("\tsentence      %s\n",ArAdr(bit_name(ImmGetProperty(kl,IGP_SENTENCE),igp_sen,ITEMS(igp_sen),&buf)));
    LOG("\tset-comp-str  %s\n",ArAdr(bit_name(ImmGetProperty(kl,IGP_SETCOMPSTR),igp_comp,ITEMS(igp_comp),&buf)));
    LOG("\tui            %s\n",ArAdr(bit_name(ImmGetProperty(kl,IGP_UI),igp_ui,ITEMS(igp_ui),&buf)));
    ArDelete(&buf);
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
    for(int n=0; n<2 && !(st=(fprintf(LogFile,"[%c][" SN_FORM "]%s",Swap4(r->p1),MsgSn++,r->p3)>=0)); ++n){
	open_logfile(LogFileName,"a");
	MsgSn %= SN_MAX;
    }
    Reply2(WIME_LOG&0xff,WIME_LOG>>8,st);
}

#ifdef __ms_va_list
#define VA_LIST __ms_va_list
#define VA_START __ms_va_start
#define VA_END __ms_va_end
#else
#define VA_LIST va_list
#define VA_START va_start
#define VA_END va_end
#endif

//wime本体が使うMsg()
bool Msg(char dummy UNUSED,const char* fmt,...)
{
    VA_LIST vl;
    VA_START(vl,fmt);
    fprintf(LogFile,"[%c][" SN_FORM "]",LOGMARK,MsgSn++);
    vfprintf(LogFile,fmt,vl);
    VA_END(vl);
    MsgSn %= SN_MAX;
    return true;
}

/*
  かんなのパケットを受信する
  !!! いいかげん関数名を変えよう
*/
unsigned __stdcall recv_xim(void* h0)
{
    Array chbuf;
    int rsz,fd;
    CanHeader *ch;
    HWND h=(HWND)h0;

    ArNew(&chbuf,1,NULL);
    ArAlloc(&chbuf,CANNAHEADERSIZE);

    while((fd = ImSelect()) > 0){
	ch = ArAdr(&chbuf);
	rsz = ImRead(ch,CANNAHEADERSIZE);
	if(rsz <= 0){ //切断
	    LOG("disconnect fd %d\n",fd);
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
	//LOG("canna packet:major=0x%x minor=0x%x len=%d\n",ch->Major,ch->Minor,ch->Length);
	SendMessageW(h,WM_CANNA_PACKET,(WPARAM)ch,(LPARAM)fd);
    }
    ArDelete(&chbuf);
    LOG("EXIT\n");
    return 0;
}

/*
  ime制御用のダミーウィンドウ
 */
HWND NewWin(void)
{
    HWND h = CreateWindow(ClassName,"",WS_POPUP,0,0,0,0,NULL,NULL,NULL,NULL);
    LOG("window handle %p, def-ime-wnd %p\n",h,ImmGetDefaultIMEWnd(h));
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

	//追加	pkt.hのプロトコル番号,canna.cのquery_ext()も変更すること
	wm_wime_dialog,
	wm_wime_set_comp_win,
	wm_wime_get_comp_win,
	wm_wime_send_key,
	wm_wime_enable_ime,
	wm_wime_move_shadow_win,
	wm_wime_set_comp_font,
	wm_wime_get_comp_str,
	wm_wime_set_cand_win,
	wm_wime_reg_x_window,
	wm_wime_get_result_str,
	wm_wime_set_result_str,
	wm_wime_reconv,
	wm_wime_set_focus,
	wm_wime_show_toolbar,
	wm_wime_get_style_list,
	wm_wime_reset,
	wm_wime_flush_msg,
	wm_wime_show_candidate_window,
	wm_wime_select_candidate,
	wm_wime_close_candidate_window,
    };

    CanFunMax[0] = sizeof(wm_canna_tab0)/sizeof(wm_canna_tab0[0]);
    CanFunMax[1] = sizeof(wm_canna_tab1)/sizeof(wm_canna_tab1[0]);

    WmCannaTab[0] = wm_canna_tab0;
    WmCannaTab[1] = wm_canna_tab1;

}

int aux_input(HWND h)
{
    HIMC imc;
    CannaContext_t *cx;
    int16_t cxn;

    imc = ImmGetContext(h);
    cx = FindContext(h,&cxn);
    VERBOSE(MSG("context %hd, xid 0x%x\n",cxn,cx->XWin); DbgComp(imc,__func__));

    if(cx->XWin != 0) //念のためチェックしておく
	ImAuxInput(cx->XWin);
    ImmReleaseContext(h,imc);
    return 0;
}

const char* dbg_wm_comp_msg(unsigned lp);
const char* dbg_wm_notify_msg(unsigned wp);
const char* dbg_wm_request_msg(unsigned wp);
const char* msg_name(unsigned n);
void dbg_filter_msg(int bit,HWND wh,UINT msg,WPARAM wp,LPARAM lp);
void dbg_ime_msg(HWND wh,UINT msg,WPARAM wp,LPARAM lp);
void dbg_cx_info(uint16_t cxn,const CannaContext_t* cx,HWND w);
#define DEBUG 0
#if DEBUG==1
#define DBG(x) x
#else
#define DBG(x)
#endif

//ウィンドウプロシージャ
LRESULT CALLBACK wnd_proc(HWND wh,UINT msg,WPARAM wp,LPARAM lp)
{
    LRESULT r=0;
    CannaContext_t *cx;
    int16_t cxn;
    CanHeader* ch;
    WMCANNAPROTO func = NULL;

    DBG(MSG("msg 0x%x window %p\n",msg,wh));

    switch(msg){
    case WM_IME_COMPOSITION: //10f
	cx = FindContext(wh,&cxn);
	DBG(dbg_filter_msg(PROC_COMP_MSG,wh,msg,wp,lp));
	DBG(dbg_cx_info(cxn,cx,wh));
	if(cx!=NULL && lp==(GCS_RESULTSTR|GCS_RESULTCLAUSE) && !(cx->Flags & SEND_KEY)){
	    /* 読みなしで結果文字列だけ→パレットツールなどからの入力
	       [3.3.0]漢字モードでスペースキーを押すと全角スペースになるが、読み文字
	       列に半角スペースがセットされない。そのため外部入力と区別がつかない。
	       if()の条件にwm_wime_send_key()が呼ばれていないかを追加してみる。
	    */
	    LOG("aux input\n");
	    r = aux_input(wh);
	}else if(cx!=NULL && (cx->Flags & PROC_COMP_MSG)!=0){
	    r = cx->ImeWnd!=NULL ? SendMessageW(cx->ImeWnd,msg,wp,lp):DefWindowProc(wh,msg,wp,lp);
        }
	if(cx != NULL)
	    cx->Flags &= ~SEND_KEY;
	break;
    case WM_IME_NOTIFY: //282
	cx = FindContext(wh,&cxn);
	DBG(dbg_filter_msg(PROC_NOTIFY_MSG,wh,msg,wp,lp));
	DBG(dbg_cx_info(cxn,cx,wh));
	if(cx!=NULL && (cx->Flags & IN_FOCUS)==0){
	    LOG("outside focus %d\n",cxn);
	    break;
	}
	if(cx!=NULL && (cx->Flags & TRAP_OPEN_CAND)!=0){
	    //こっちを先に調べること。あるいはTRAP_OPEN_CANDとPROC_NOTIFY_MSGは排他にするか?
	    if(wp==IMN_OPENCANDIDATE){
		LOG("catch open candi\n");
		cx->Flags |= CATCH_OPEN_CAND;
		break;
	    }
	    if(wp==IMN_CHANGECANDIDATE){
		LOG("catch change candi\n");
		cx->Flags |= CATCH_CHG_CAND;
		break;
	    }
	}
	if(cx!=NULL && (cx->Flags & PROC_NOTIFY_MSG)!=0){
	    r = cx->ImeWnd!=NULL ? SendMessageW(cx->ImeWnd,msg,wp,lp):DefWindowProc(wh,msg,wp,lp);
	}
	break;
    case WM_IME_REQUEST: //288
	cx = FindContext(wh,&cxn);
	DBG(dbg_filter_msg(0,wh,msg,wp,lp));
	DBG(dbg_cx_info(cxn,cx,wh));
	if(cx!=NULL && wp==IMR_RECONVERTSTRING && lp==0){ //再変換
	    cx->Flags |= PENDING_RECONV;
	    LOG("cx %d: reconvert. pending\n",cxn);
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
	DBG(dbg_filter_msg(0,wh,msg,wp,lp));
	DBG(dbg_cx_info(cxn,cx,wh));
	if(cx!=NULL && ((cx->Flags & PROC_NOTIFY_MSG)==0 || (cx->Flags & IN_FOCUS)==0)){
	    LOG("wnd %p, def-ime-wnd %p, msg 0x%x cxn %hd Flags %x: drop message\n",wh,ImmGetDefaultIMEWnd(wh),msg,cxn,cx->Flags);
	}else if(cx==NULL){
	    LOG("wnd %p, def-ime-wnd %p, msg 0x%x cxn %hd:canna context not found\n",wh,ImmGetDefaultIMEWnd(wh),msg,cxn);
	    r = DefWindowProc(wh,msg,wp,lp);
	}else if(cx->ImeWnd==NULL){
	    LOG("wnd %p, def-ime-wnd %p, msg 0x%x cxn %hd:ime window not found\n",wh,ImmGetDefaultIMEWnd(wh),msg,cxn);
	    r = DefWindowProc(wh,msg,wp,lp);
	}else{
	    LOG("wnd %p, def-ime-wnd %p, cxn %hd:send 0x%x to ime %p\n",wh,ImmGetDefaultIMEWnd(wh),cxn,msg,cx->ImeWnd);
	    r = SendMessageW(cx->ImeWnd,msg,wp,lp);
	}
	break;

    case WM_CANNA_PACKET: //wp=header lp=fd
	ch = (CanHeader*)wp;
	if(ch->Minor<2 && ch->Major<CanFunMax[ch->Minor])
	    func = WmCannaTab[ch->Minor][ch->Major];
	if(func != NULL)
	    r = (LRESULT)func(ch,(int)lp);
	else{
	    MSG("*** ILLEGAL CANNA PROTOCOL:minor=0x%x major=0x%x\n",ch->Minor,ch->Major);
	    r = (LRESULT)true;
	}
	break;
    default:
	r = DefWindowProc(wh,msg,wp,lp);
    }
    DBG(MSG("msg 0x%x wp:0x%x lp:0x%x-- return code 0x%x\n",msg,(unsigned)wp,(unsigned)lp,r));
    return r;
}

void usage(int exit_code)
{
    printf("wime [options] [logfile]\n"
	   "  -i,--inet [port]	tcp connection\n"
	   "  -s ime		specify ime\n"
	   "  -p num		socket number\n"
	   "  -v,-v-		verbose (on,off)\n"
	   "  --version		print version\n"
	   "  -h,--help		this message\n"
	);
    exit(exit_code);
}

void print_version(void)
{
    printf("%s\n",WIME_VERSION);
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
	{"help",	no_argument,NULL,'h'},
	{"inet",	optional_argument,NULL,'i'},
	{"version",	no_argument,NULL,'vsn'},
	{NULL,0,NULL,0}
    };
    int c,socket_num=0;

    *use_tcp = 0;
    while((c = getopt_long(ac,av,"hi::p:s:v::",longopt,NULL)) != -1){
	switch(c){
	case 'h':
	    usage(0);
	case 'p':
	    socket_num = atoi(optarg);
	    break;
	case 'i':
	    *use_tcp = (optarg==NULL ? -1 : atoi(optarg));
	    break;
	case 's':
	    if(!ime_sp(optarg)){
		ERR("no available ime '%s'\n",optarg);
		exit(1);
	    }
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
	    print_version();
	    exit(0);
	default:
	    usage(1);
	}
    }
    open_logfile(LogFileName = av[optind],"w");
	
    return socket_num;
}

/*
  指定したメッセージidがきたらその配列番号。来なかったら-1
  n=個数
  UINT msg,WPARAM wp,LPARAM* lp,...
*/
int MsgLoopN(int n,...)
{
    va_list vl;
    UINT m0,ms[n];
    WPARAM w0,ws[n];
    LPARAM l0,*lp[n];
    MSG msg;
    int i;

    va_start(vl,n);
    for(i=0; i<n; ++i){
	ms[i] = va_arg(vl,UINT);
	ws[i] = va_arg(vl,WPARAM);
	lp[i] = va_arg(vl,LPARAM*);
    }
    va_end(vl);

    i = -1;
    while(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)){
	m0 = msg.message;
	w0 = msg.wParam;
	l0 = msg.lParam;
	if(GetMessage(&msg,NULL,0,0) > 0){
	    TranslateMessage(&msg);
	    DispatchMessage(&msg);
	    for(i=n; --i>=0;)
		if(m0==ms[i] && w0==ws[i]){
		    if(lp[i] != NULL)
			*lp[i] = l0;
		    goto exit_p;
		}
	}
    }
exit_p:
    return i;
}


#if DEBUG==1

#define CASE_STR(x) case x:s=#x;break

void dbg_ime_msg(HWND wh,UINT msg,WPARAM wp,LPARAM lp)
{
    HIMC imc=ImmGetContext(wh);
    switch(msg){
    case WM_IME_STARTCOMPOSITION ... WM_IME_KEYLAST:
    case WM_IME_SETCONTEXT ... WM_IME_KEYUP:
	dbg_filter_msg(-1,wh,msg,wp,lp);
	//DbgComp(imc,__func__);
	break;
    }
    ImmReleaseContext(wh,imc);
}

void dbg_filter_msg(int bit,HWND wh,UINT msg,WPARAM wp,LPARAM lp)
{
    char *s;
    int16_t cxn;
    CannaContext_t *cx = FindContext(wh,&cxn);
    if(cx!=NULL && (cx->Flags & bit)!=0)
	s = "pass to system";
    else
	s = "filtering";
    MSG("window %x,context %hd,ime message:%s %s %x %x\n",(unsigned)wh,cxn,s,msg_name(msg),(unsigned)wp,(unsigned)lp);
    switch(msg){
    case WM_IME_NOTIFY:
	MSG("\twp=%s\n",dbg_wm_notify_msg(wp));
	break;
    case WM_IME_COMPOSITION:
	MSG("\tlp=%s\n",dbg_wm_comp_msg(lp));
	break;
    case WM_IME_REQUEST:
	MSG("\twp=%s lp=%x\n",dbg_wm_request_msg(wp),(unsigned)lp);
    }
}

const char* dbg_wm_notify_msg(unsigned wp)
{
    const char *s;
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
    default: s="unknown";
    }
    return s;
}

const char* dbg_wm_request_msg(unsigned wp)
{
    const char *s;
    switch(wp){
	CASE_STR(IMR_CANDIDATEWINDOW);
	CASE_STR(IMR_COMPOSITIONFONT);
	CASE_STR(IMR_COMPOSITIONWINDOW);
	CASE_STR(IMR_CONFIRMRECONVERTSTRING);
	CASE_STR(IMR_DOCUMENTFEED);
	CASE_STR(IMR_QUERYCHARPOSITION);
	CASE_STR(IMR_RECONVERTSTRING);
    default: s="unknown";
    }
    return s;
}

const char* dbg_wm_comp_msg(unsigned lp)
{
#define PAIR(x) {x,#x}
    struct{
	int mask;
	const char* str;
    } bits[]={
	PAIR(GCS_COMPREADSTR),
	PAIR(GCS_COMPREADATTR),
	PAIR(GCS_COMPREADCLAUSE),
	PAIR(GCS_COMPSTR),
	PAIR(GCS_COMPATTR),
	PAIR(GCS_COMPCLAUSE),
	PAIR(GCS_CURSORPOS),
	PAIR(GCS_DELTASTART),
	PAIR(GCS_RESULTREADSTR),
	PAIR(GCS_RESULTREADCLAUSE),
	PAIR(GCS_RESULTSTR),
	PAIR(GCS_RESULTCLAUSE)
    };
#undef PAIR
    static char buf[256];
    const char *sep="";
    buf[0]=0;
    for(unsigned n=0; n<ITEMS(bits); ++n){
	if((lp & bits[n].mask)){
	    strcat(strcat(buf,sep),bits[n].str);
	    lp &= ~bits[n].mask;
	    sep="|";
	}
    }
    if(lp!=0){
	strcat(strcat(buf,sep),"0x");
	sprintf(buf+strlen(buf),"%x",lp);
	strcat(buf,")");
    }
    return buf;
}

const char* msg_name(unsigned n)
{
    const char *s;
    static char buf[80];

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
    HIMC imc=ImmGetContext(w);
    char flagstr[100];

    flagstr[0]=0;


    if(cx!=NULL){
	imewnd=cx->ImeWnd;
	defimc=cx->DefImc;

	strcpy(flagstr," Flags=");
	char* endp=flagstr+strlen(flagstr);
	char* tags[]={"OPEN_STATUS_WINDOW","PROC_NOTIFY_MSG","PROC_COMP_MSG","PENDING_RECONV","SEND_KEY","TRAP_OPEN_CAND","CATCH_OPEN_CAND","CATCH_CHG_CAND",NULL};
	for(int n=0; tags[n]!=NULL; ++n){
	    if((cx->Flags & (1<<n)) != 0){
		if(*endp!=0)
		    strcat(flagstr,"|");
		strcat(flagstr,tags[n]);
	    }
	}
	if(*endp==0)
	    strcat(flagstr,"0");
    }
    MSG("cxn %hd cx %p cx->ImeWnd %p cx->DefImc %p wnd %p imc %p%s\n",cxn,cx,imewnd,defimc,w,imc,flagstr);
    ImmReleaseContext(w,imc);
}
#endif
