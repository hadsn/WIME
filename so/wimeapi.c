#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include "corr.h"
#include "wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "wimelog.h"
#include "exe/version.h"

/*
  initialize,finalize以外はlongjmpする可能性がある。
*/

int GlobalCxn;
jmp_buf WimeJmp;

static Array LibCxn;
#define EMPTY_CXN_CELL -1

static struct sigaction RestartSigLink; //WIMERESTARTSIGにもともとあった処理

static int recreate_context(int* cxp,void* arg UNUSED)
{
    if(*cxp != EMPTY_CXN_CELL){
	int16_t cxn;
	if(Snd1(Fd,CANNA_CREATE_CONTEXT) && Rcv5(Fd,&cxn) && cxn!=-1)
	    *cxp = cxn;
	else
	    ERR("can not create context\n");
    }
    return 1;
}

//wime再スタートのシグナル
//!!! LibCxnのアクセス時はロックすべきか？
void catch_restart_wime(int signum UNUSED)
{
    MSG("recreate context\n");
    WimeDisconnect();
    WimeInitialize(0,0);
    ArForEach(&LibCxn,(ArForEachFunc)recreate_context,NULL);
}

__attribute__((constructor))
void wime_api_init(void)
{
    /*
      ソケットのファイルが残っている場合socket()は成功するのでFdがつくられるが、
      そこに書き込もうとするとSIGPIPEが起きる。
      !!!いっそのことSIGPIPEのエラーハンドラからlongjmpした方がわかりやすいか？
    */
    signal(SIGPIPE,SIG_IGN);

    struct sigaction a;
    memset(&a,0,sizeof(a));
    a.sa_handler = catch_restart_wime;
    sigaction(WIMERESTARTSIG,&a,&RestartSigLink);
}


//LibCxnの空きセルの初期化
static void libcxn_ctr(void* adr)
{
    *(int*)adr = EMPTY_CXN_CELL;
}

//logmark==0のときは再スタートシグナルハンドラからの呼び出し
bool WimeInitialize(int socket_num,int logmark)
{
    //3.3の仕様書は3.6p4だったり2.0だったりしてるが,とりあえず3.6にしておく。
    //???環境変数USERは必ずあるとしていいのか？
    bool st=false;
    int minor,cxn;

    SocketPath = MakeSocketPath(socket_num);
    if(WimeConnect()){
	if(Snd0(Fd,"3.6",getenv("USER")) && (cxn = Rcv0(Fd,&minor))!=-1 && minor==WIME_CANNA_MINOR){
	    st = true;

	    //LibCxn[0]はグローバルコンテキスト
	    if(logmark == 0)
		*(int*)ArAdr(&LibCxn) = cxn;
	    else
		*(int*)ArAlloc(ArNewPs(&LibCxn,sizeof(int),libcxn_ctr,16),1) = cxn;

	    WimeShmInit(logmark);
	}else
	    WimeDisconnect();
    }
    return st;
}

static int close_all_context(int* cxp,void* arg UNUSED)
{
    if(*cxp != EMPTY_CXN_CELL)
	CannaCloseContext(*cxp);
    return 1;
}

bool WimeFinalize(void)
{
    bool st = false;
    char code;

    if(Fd != -1){
	//開いているコンテキストを全部閉じる
	ArForEach(&LibCxn,(ArForEachFunc)close_all_context,NULL);
	ArDelete(&LibCxn);

	st = (Snd1(Fd,CANNA_FINALIZE) && Rcv2(Fd,&code) && code==0);
	WimeDisconnect();
	WimeShmFin();
    }
    return st;
}

//connect()できたかどうかだけなので、trueでもwimeがいない可能性もある
bool WimeIsConnected()
{
    return (Fd!=-1);
}

static void error_jump(void)
{
    WimeDisconnect();
    longjmp(WimeJmp,1);
}

static int translate_cx(int n)
{
    if(n<0 || n>=ArUsing(&LibCxn))
	longjmp(WimeJmp,1);
    return *(int*)ArElem(&LibCxn,n);
}
	
//エラーの時-1
int CannaCreateContext(void)
{
    int16_t cxn;
    int idx=-1,*adr,emp=EMPTY_CXN_CELL;

    if(!Snd1(Fd,CANNA_CREATE_CONTEXT) || !Rcv5(Fd,&cxn))
	error_jump();
    if(cxn != -1){
	if((idx = ArFind(&LibCxn,PID_CLIENT,&emp)) == -1){
	    idx = ArUsing(&LibCxn);
	    adr = ArExpand(&LibCxn,1);
	}else
	    adr = ArElem(&LibCxn,idx);
	*adr = cxn;
    }
    return idx;
}

bool CannaCloseContext(int cxn)
{
    char code;
    int *adr;
    bool st;

    adr = ArElem(&LibCxn,cxn);
    if(adr==NULL || !Snd2(Fd,CANNA_CLOSE_CONTEXT,*adr) || !Rcv2(Fd,&code))
	error_jump();
    if((st = (code == 0)))
	*adr = EMPTY_CXN_CELL;
    return st;
}

int WimeGetGlobalContext(void)
{
    return 0;
}

/*
  拡張プロトコル番号を返す
  エラーの時0
*/
static int query_extension(const char* name)
{
    int num=0;
    char code;
    const char *names[]={name,NULL};
    if(!Snd17a(Fd,CANNA_QUERY_EXTENSIONS,names) || !Rcv2(Fd,&code))
	error_jump();
    num = code+1; //返される番号は"プロトコル番号-1"。+1するのでエラーの時0になる
    if(num != 0)
	num |= 0x0100;
    return num;
}

bool WimeOpenIMEDialog(int type)
{
    static int prn=0;
    char code=-1;

    if(prn == 0)
	prn = query_extension(__func__);
    if(prn!=0){
	if(!Snd2(Fd,prn,(int16_t)type) || !Rcv2(Fd,&code))
	    error_jump();
    }
    return code!=-1;
}

bool CannaKillServer(void)
{
    char code;
    if(!Snd1(Fd,CANNA_KILL_SERVER) || !Rcv2(Fd,&code))
	error_jump();
    return code==0;
}

bool CannaAutoConvert(int cxn,int bufsize,int mode)
{
    char code;
    cxn = translate_cx(cxn);
    if(!Snd5(Fd,CANNA_AUTO_CONVERT,(int16_t)cxn,(uint16_t)bufsize,(int32_t)mode) || !Rcv2(Fd,&code))
	error_jump();
    return code==0;
}

bool CannaEndConvert(int cxn,int mode,int cl_count,int* can_list,int list_len)
{
    bool st;
    char code;
    cxn = translate_cx(cxn);

    //intの配列をint16の配列に変換する。はじめからint16を受けるようにした方がいいか？
    int16_t* clist16 = malloc(sizeof(*clist16)*list_len);
    for(int n=0; n<list_len; ++n)
	clist16[n] = can_list[n];

    st = (!Snd10(Fd,CANNA_END_CONVERT,cxn,cl_count,mode,clist16,list_len) || !Rcv2(Fd,&code));
    free(clist16);
    if(st)
	error_jump();
    return code==0;
}

/*
  cxn=コンテキスト番号
  mode=モード
  ej=読み（ひらがな）
  返値=各文節の最優先候補のリスト。エラーの時NULL
  cl=文節数
  リストはchar*の配列。各文字列と配列本体をfreeすること。
*/
char** CannaBeginConvert(int cxn,int mode,const char* ej,int* cl)
{
    int n;
    int16_t clw;
    uint16_t* lstw=NULL;
    char** canl;
    uint16_t* cej = ToWc(NULL,ej);
    bool st = (!Snd14(Fd,CANNA_BEGIN_CONVERT,mode,translate_cx(cxn),cej) || !Rcv7(Fd,&clw,&lstw));
    free(cej);
    if(st){
	free(lstw);
	error_jump();
    }
    canl = malloc((*cl=clw)*sizeof(char*));
    for(n=0; n<*cl; ++n)
	canl[n] = ToMb(StrListNthWc(lstw,*cl,n));
    free(lstw);
    return canl;
}

/*
  cxn=コンテキスト番号
  cl=文節番号
  返値=候補文字列と読みのリスト。
  cann=候補数。読みの数も含む。NULLでもok。
  リストはchar*の配列。最後にNULLポインタがある。各文字列と配列本体をfreeすること。
 */
char** CannaGetCandidacyList(int cxn,int cl,int* cann)
{
    int n,cann_dummy;
    int16_t cnw;
    uint16_t* lstw=NULL;
    char** canl;
    bool st = (!Snd6(Fd,CANNA_GET_CANDIDACY_LIST,translate_cx(cxn),cl,0xffff) || !Rcv7(Fd,&cnw,&lstw));
    if(st){
	free(lstw);
	error_jump();
    }

    if(cann==NULL)
	cann=&cann_dummy;
    canl = malloc(((*cann=cnw)+1)*sizeof(char*));
    for(n=0; n<*cann; ++n)
	canl[n] = ToMb(StrListNthWc(lstw,*cann,n));
    canl[n]=NULL;
    free(lstw);
    return canl;
}

/*
  戻り値はmallocで確保される
*/
int* WimeListContext(int* len)
{
    static int prn=0;
    int *lst=NULL;
    Rply7_t *r;

    *len = 0;
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	if(!Snd1(Fd,prn) || (r = RcvN(Fd,NULL,0))==NULL)
	    error_jump();
	lst = malloc((*len = r->p1)*sizeof(int));
	for(int x=0; x<*len; ++x)
	    lst[x] = Swap2(r->p2[x]);
	free(r);
    }
    return lst;
}

//追加引数は全部int
bool WimeSetCompWin(int cxn,int style,...)
{
    static int prn=0;
    char code=false;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	int pn;
	uint16_t params[4];
	va_list vl;

	va_start(vl,style);
	switch(style){
	case WIME_POS_DEFAULT:
	    pn = 0;
	    break;
	case WIME_POS_FORCE:
	case WIME_POS_POINT:
	    pn = 2;
	    break;
	case WIME_POS_RECT:
	    pn = 4;
	}
	for(int n=0; n<pn; ++n)
	    params[n] = va_arg(vl,int);
	va_end(vl);
	cxn = translate_cx(cxn);
	if(!Snd11(Fd,prn,cxn,style,params,pn) || !Rcv2(Fd,&code))
	    error_jump();
    }
    return code;
}

/*
  sc	下8bit=winの仮想キーコード,上8bit=winのシフトキービット
  戻り値：WIME_SENDKEY_XXXX
	imeに処理されたとき、確定文字列があればmallocでresに返す。なければNULLが返される。
*/
int WimeSendKey(int cxn,unsigned sc,char** res)
{
    static int prn=0;
    int16_t proc=0;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	if(!Snd3(Fd,prn,cxn,sc) || !Rcv6(Fd,&proc,res))
	    error_jump();
    }
    return proc;
}

/*
  en_ime  0=ime off, 1=ime on, -1=問い合わせ
*/
bool WimeEnableIme(int cxn,int en_ime)
{
    static int prn=0;
    int16_t st;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd3(Fd,prn,translate_cx(cxn),en_ime) || !Rcv5(Fd,&st))
	    error_jump();
    return st!=-1 ? (st!=0) : false;
}

/*
  imcをもつ影ウィンドウの位置か大きさを変更する
  (x,y),(w,h)それぞれのどちらかが負であれば使用しない
*/
bool WimeMoveShadowWin(int cxn,int x,int y,int w,int h)
{
    static int prn=0;
    int16_t ax[]={x,y,w,h};
    char code=false;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd11(Fd,prn,cxn,0,(uint16_t*)ax,ITEMS(ax)) || !Rcv2(Fd,&code))
	    error_jump();
    return code;
}

/*
  変換ウィンドウのフォントと背景色を指定する
  返値：フォントの高さ(ピクセル)。エラーの時0
*/
int WimeSetCompFont(int cxn,const char* font,unsigned bg)
{
    static int prn=0;
    int16_t h=0;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd15(Fd,prn,bg,cxn,font) || !Rcv5(Fd,&h))
	    error_jump();
    return h;
}

/*
  変換途中の文字列とカーソル情報を得る。
  文字列はmallocで確保される。
 */
char* WimeGetCompStr(int cxn,WimeCompStrInfo* si)
{
    static int prn=0;
    char status=-1,*str=NULL,*dum=NULL;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd2(Fd,prn,cxn) || !Rcv10(Fd,&status,&str,&dum,(int32_t*)si))
	    error_jump();
    free(dum);
    if(status < 0){ //変換途中の文字列はなかった
	free(str);
	str = NULL;
    }
    return str;
}

/*
  ImmGetCompositionWindow
  返値:WIME_POS_xxx
	エラーの時0
*/
int WimeGetCompWin(int cxn,int* x,int* y,int* w,int* h)
{
    static int prn=0;
    int v[5],*vp;
    char st=false;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd2(Fd,prn,cxn) || !Rcv4(Fd,&st,vp=v))
	    error_jump();
    if(st){
	++vp; //style
	*x = *(vp++);
	*y = *(vp++);
	*w = *(vp++);
	*h = *vp;
    }else
	v[0] = 0;
    return v[0];
}

/*
  style=WIME_POS_POINT,WIME_POS_EXCLUDE
  WIME_POS_EXCLUDEのときは追加引数にx,y,w,h
*/
bool WimeSetCandWin(int cxn,int style,int x,int y,...)
{
    static int prn=0;
    int16_t ax[6]={x,y};
    char code=false;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(style == WIME_POS_EXCLUDE){
	va_list vl;
	va_start(vl,y);
	for(int n=0; n<4; ++n)
	    ax[2+n] = va_arg(vl,int);
	va_end(vl);
    }
    if(prn!=0)
	if(!Snd11(Fd,prn,cxn,style,(uint16_t*)ax,ITEMS(ax)) || !Rcv2(Fd,&code))
	    error_jump();
    return code;
}

/*
  cxnに対応するXのウィンドウを登録する。
*/
void WimeRegXWindow(int cxn,unsigned w)
{
    static int prn=0;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	PktRegXWin p = {translate_cx(cxn),w};
	if(!SndN(Fd,prn,&p,sizeof(p)))
	    error_jump();
    }
}

/*
  結果文字列をucs2で返す。
  戻り値はfreeすること。
*/
uint16_t* WimeGetResultStr(int cxn)
{
    static int prn=0;
    CanHeader *q=NULL;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	PktCxNum p = {translate_cx(cxn)};
	if(!SndN(Fd,prn,&p,sizeof(p)) || (q=RcvN(Fd,NULL,0))==NULL)
	    error_jump();
	if(q->Length == 0){
	    free(q);
	    q = NULL;
	}else
	    memcpy(q,q+1,q->Length);
    }
    return (uint16_t*)q;
}

/*
  ejを変換完了文字列としてcxnに送る。
  cxnが負の時はその絶対値をwime serverでのコンテキストidとしてそのまま使う
*/
bool WimeSetResultStr(int cxn,const char* ej)
{
    static int prn=0;
    bool st=false;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	char buf[sizeof(PktResultStr)+strlen(ej)+1];
	PktResultStr *p = (typeof(p))buf;
	p->cxn = cxn>0 ? translate_cx(cxn) : -cxn;
	strcpy(p->str,ej);
	if(!SndN(Fd,prn,&buf,sizeof(buf)))
	    error_jump();
	st = true;
    }
    return st;
}

/*
  再変換する。
  s=再変換文字列(u16)
  cursor=カーソル位置(文字単位)
  戻り値：対象部分の長さ（文字単位）。エラーの時０
	pos=対象部分の開始位置（文字単位）。
*/
int WimeReconvert(int cxn,const uint16_t* s,int cursor,int* pos)
{
    static int prn=0;
    char st=false;
    int32_t info[2];
    
    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd11(Fd,prn,cxn,cursor,s,-1) || !Rcv4(Fd,&st,info))
	    error_jump();
    if(!st)
	info[1] = 0;
    *pos = info[0];
    return info[1];
}

/*
  フォーカスの移動を知らせる
*/
void WimeSetFocus(int cxn,bool in)
{
    static int prn=0;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	int p[] = {translate_cx(cxn),in};
	if(!SndN(Fd,prn,p,sizeof(p)))
	    error_jump();
    }
}

/*
  imeのツールバーを表示する
  tb		ツールバーを表示
  comp_win	変換ウィンドウを使う
*/
void WimeShowToolbar(int cxn,bool tb,bool comp_win)
{
    static int prn=0;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	if(!Snd7(Fd,prn,translate_cx(cxn),tb,comp_win))
	    error_jump();
    }
}

#define ALGN(n) ((n/sizeof(int)+1)*sizeof(int))

/*
  単語登録に使う品詞の一覧を得る(eucjp)
  戻り値 WimeWordStyleの配列。items=要素数。
*/
WimeWordStyle* WimeGetStyleList(int* items)
{
    static int prn=0;
    CanHeader *ch;
    PktStyleList *sl;
    WimeWordStyle *ws,*ws_begin=NULL;
    int *code,sz;
    char *desc;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	if(!Snd1(Fd,prn) || (ch=RcvN(Fd,NULL,0))==NULL)
	    error_jump();

	sl = (PktStyleList*)(ch+1);
	ws = ws_begin = malloc((sizeof(WimeWordStyle)+ALGN(sl->desc_max))*sl->count);
	code = sl->code;
	desc = (char*)(sl->code+sl->count);

	*items = sl->count;
	while(-- sl->count >= 0){
	    ws->Code = *(code++);
	    sz = strlen(strcpy(ws->Desc,desc))+1;
	    desc += sz;
	    ws->Size = sizeof(WimeWordStyle)+ALGN(sz);
	    ws = (WimeWordStyle*)((char*)ws+ws->Size);
	}
	free(ch);
    }
    return ws_begin;
}

/*
  設定ファイルを再読み込みする
  ??? エラーコードでも返すか？
*/
bool WimeReset(void)
{
    static int prn=0;
    bool st=false;
    void *r;
    char buf[sizeof(CanHeader)+sizeof(int)];
    CanHeader *ch = (CanHeader*)buf;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	if(!Snd1(Fd,prn) || (r=RcvN(Fd,ch,sizeof(buf)))==NULL)
	    error_jump();
	st = (r==ch && *(int*)(ch+1)==0);
    }
    return st;
}

/*
  wime側のメッセージループを回す。
  !!! WimeResetと全く同じコード。
*/
bool WimeFlushMsg(void)
{
    static int prn=0;
    bool st=false;
    void* r;
    char buf[sizeof(CanHeader)+sizeof(int)];
    CanHeader* ch = (CanHeader*)buf;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0){
	if(!Snd1(Fd,prn) || (r=RcvN(Fd,ch,sizeof(buf)))==NULL)
	    error_jump();
	st = (r==ch && *(int*)(ch+1)==0);
    }
    return st;
}


#include <X11/Xlib.h>
#include "xres.h"
#include "winkey.h"

//この３つは必ず指定すること
void (*WimePreedit)(const char* ej,const WimeCompStrInfo* si,void* arg);
void (*WimeConvert)(const char* ej,const WimeCompStrInfo* si,void* arg);
void (*WimeCommit)(const char* ej,void* arg);
//この２つはなくてもいい
uint16_t* (*WimeGetSurrounding)(void* arg,int* cursor_pos);
void (*WimeDeleteSurrounding)(void* arg,int pos,int len);

/*
		str	si
  処理なし	NULL	CursorPos=-1
  コントロール	NULL	CursorPos=0
  入力中	文字列	TargetClause=-1,CursorPos>=0
  変換中	文字列	TargetClause>=0
  確定		文字列	TargetClause=-1,CursorPos=-1
  文字列はeuc-jp

  文字を処理したらtrueを返す
*/
bool WimeFilterKey(int cxn,const ToggleKey* tk,int keysym,int shift,void* arg)
{
    unsigned wk;
    char *str;
    uint16_t* u16;
    int pos,len,cursor;
    WimeCompStrInfo si;

    if(!WimeIsConnected())
	WimeInitialize(0,LOGMARK);
    if(setjmp(WimeJmp) != 0){
	ERR("Disconnect wime\n");
	return false;
    }

/*
    if((shift & 0xff) == AUX_INPUT_MOD) //[atok]パレットからの入力
	return aux_input();
*/

    if(IsToggleKey(tk,keysym,shift)){
	if(!WimeEnableIme(cxn,IME_QUERY)){
	    //漢字モード開始
	    LOG("cxn %d:enable ime\n",cxn);
	    WimeEnableIme(cxn,IME_ON);
	}else{
	    //漢字モード終了
	    if((str = WimeGetCompStr(cxn,&si)) == NULL){
		LOG("cxn %d:disable ime\n",cxn);
		WimeEnableIme(cxn,IME_OFF);
	    }
	    /*else
	      変換途中の文字列があれば漢字モードを続ける
	    */
	}
	return true;
    }
    
    if(!WimeEnableIme(cxn,IME_QUERY)){
	return false; //直接入力中
    }

    //変換中
    wk = ConvToVk(keysym,shift);
    LOG("keysym 0x%x,shift 0x%x --> windows vk 0x%x\n",keysym,shift,wk);
    switch(WimeSendKey(cxn,wk,&str)){
    case WIME_SENDKEY_RECONV: //再変換キーだった
	if(WimeGetSurrounding == NULL)
	    return false;
	u16 = (*WimeGetSurrounding)(arg,&cursor);
	LOG("cursor %d strlen %d\n",cursor,WcLen(u16));
	if((len = WimeReconvert(cxn,u16,cursor,&pos)) == 0)
	    return false;
	pos -= cursor; //元の文字列を消す（カーソルからの相対位置）
	LOG("delete pos %d,len %d\n",pos,len);
	(*WimeDeleteSurrounding)(arg,pos,len);
	break;
    case WIME_SENDKEY_SUCCESS: //処理された
    case WIME_SENDKEY_OPENCAND:
    case WIME_SENDKEY_CHGCAND:
	break;
    default: //処理されなかったorエラー
	return false;
    }

    if(str == NULL){
	str = WimeGetCompStr(cxn,&si);
	if(si.TargetClause == -1)
	    (*WimePreedit)(str,&si,arg);	//入力途中
	else
	    (*WimeConvert)(str,&si,arg);	//変換中
    }else{
	(*WimeCommit)(str,arg);			//確定
    }
    free(str);
    return true;
}

/*
  変換候補ウィンドウの表示/非表示
*/
bool WimeShowCandidateWindow(int cxn,bool en)
{
    static int prn=0;
    char code=false;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd3(Fd,prn,translate_cx(cxn),en) || !Rcv2(Fd,&code))
	    error_jump();
    return code;
}

/*
  変換候補を選択する
*/
bool WimeSelectCandidate(int cxn,int index)
{
    static int prn=0;
    char code=false;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd3(Fd,prn,translate_cx(cxn),index) || !Rcv2(Fd,&code))
	    error_jump();
    return code;
}

/*
  変換候補ウィンドウを閉じる。
*/
void WimeCloseCandidateWindow(int cxn)
{
    static int prn=0;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0)
	if(!Snd2(Fd,prn,translate_cx(cxn)))
	    error_jump();
}
