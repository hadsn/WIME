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

//wime再スタートのシグナル
//!!! LibCxnのアクセス時はロックすべきか？
void catch_restart_wime(int signum UNUSED)
{
    int x,*cxp;
    int16_t cxn;

    MSG("recreate context\n");
    WimeDisconnect();
    WimeInitialize(0,0);

    for(x=0,cxp=ArAdr(&LibCxn); x<ArUsing(&LibCxn); ++x,++cxp){
	if(*cxp != EMPTY_CXN_CELL){
	    if(!Snd1(Fd,CANNA_CREATE_CONTEXT) || !Rcv5(Fd,&cxn) || cxn==-1)
		ERR("can not create context\n");
	    else
		*cxp = cxn;
	}
    }
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

bool WimeFinalize(void)
{
    bool st = false;
    char code;
    int x,*cxp;
    if(Fd != -1){
	//開いているコンテキストを全部閉じる
	cxp = ArAdr(&LibCxn);
	for(x=0; ++cxp,x<ArUsing(&LibCxn); ++x){
	    if(*cxp != EMPTY_CXN_CELL)
		WimeCloseContext(*cxp);
	}
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
int WimeCreateContext(void)
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

bool WimeCloseContext(int cxn)
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

bool WimeKillServer(void)
{
    char code;
    if(!Snd1(Fd,CANNA_KILL_SERVER) || !Rcv2(Fd,&code))
	error_jump();
    return code==0;
}

bool WimeAutoConvert(int cxn,int bufsize,int mode)
{
    char code;
    cxn = translate_cx(cxn);
    if(!Snd5(Fd,CANNA_AUTO_CONVERT,(int16_t)cxn,(uint16_t)bufsize,(int32_t)mode) || !Rcv2(Fd,&code))
	error_jump();
    return code==0;
}

bool WimeEndConvert(int cxn,int mode,int cl_count,int* can_list,int list_len)
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
  戻り値：再変換キーだったとき-2。
	imeに処理されなかったとき-1
	エラーの時0
	imeに処理されたとき１。
	   確定文字列があればmallocでresに返す。なければNULLが返される。
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
