// -*- coding:euc-jp -*-
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include "corr.h"
#include "wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "exe/version.h"
#include "lib/log.h"

static Array LibCxn;
#define EMPTY_CXN_CELL -1

__attribute__((constructor))
static void wime_api_init(void)
{
    struct sigaction old;
    sigaction(SIGPIPE,NULL,&old);
    if(old.sa_handler==SIG_DFL){
	struct sigaction sa = {.sa_handler=SIG_IGN};
	sigaction(SIGPIPE,&sa,NULL);
    }

    /*
      サーバーは死んだがソケットのファイルが残っている場合socket()は成功するのでFdがつくられるが、
      そこに書き込もうとするとSIGPIPEが起きる。
    */
}

//LibCxnの空きセルの初期化
static void libcxn_ctr(void* adr)
{
    *(int*)adr = EMPTY_CXN_CELL;
}

/*
  socket_numが0のときはソケットに数字を追加しない。環境変数があれば追加されるかもしれない。
  logmark==0のときは再スタートシグナルハンドラからの呼び出し
  使用するソケット(>=0)を返す。エラーの時は-1
*/
int WimeInitialize(int socket_num,int logmark)
{
    //3.3の仕様書は3.6p4だったり2.0だったりしてるが,とりあえず3.6にしておく。
    //???環境変数USERは必ずあるとしていいのか？
    if(logmark!=0)
	LogMark=logmark;
    SocketPath = MakeSocketPath(socket_num,&socket_num);
    if(ConnectServer()){
	int minor,cxn;
	if(Snd0(Fd,"3.6",getenv("USER")) && (cxn = Rcv0(Fd,&minor))!=-1 && minor==WIME_CANNA_MINOR){
	    //LibCxn[0]はグローバルコンテキスト
	    *(int*)ArAlloc(ArNewPs(&LibCxn,sizeof(int),libcxn_ctr,16),1) = cxn;
	}else{
	    DisconnectServer();
	    socket_num = -1;
	}
    }else
	socket_num = -1;

    ShmStartClient(); //サーバーがあってもなくてもpidの記録はしておく。
    return socket_num;
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

	st = (Snd1(Fd,CANNA_FINALIZE) && Rcv2(Fd,&code) && code==0);
	DisconnectServer();
	ShmEndClient();
    }
    ArDelete(&LibCxn);
    free(SocketPath);
    SocketPath=NULL;
    return st;
}

//connect()できたかどうかだけなので、trueでもwimeがいない可能性もある
bool WimeIsConnected()
{
    return (Fd!=-1);
}

//エラーの時０以下
static int translate_cx(int n)
{
    if(n<0 || n>=ArUsing(&LibCxn))
	return -1;
    return *(int*)ArElem(&LibCxn,n);
}
	
//エラーの時-1
int CannaCreateContext(void)
{
    int16_t cxn;
    int idx=-1,*adr,emp=EMPTY_CXN_CELL;

    if(Snd1(Fd,CANNA_CREATE_CONTEXT) && Rcv5(Fd,&cxn) && cxn!=-1){
	const int min_context=1; //0はグローバルコンテキスト
	if((idx = ArFind(&LibCxn,min_context,&emp)) == -1){
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
    bool st=false;

    int t_cxn = translate_cx(cxn);
    if(t_cxn>=0 && Snd2(Fd,CANNA_CLOSE_CONTEXT,t_cxn) && Rcv2(Fd,&code) && code==0){
	*(int*)ArElem(&LibCxn,cxn) = EMPTY_CXN_CELL;
	st = true;
    }
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
    const char* names[]={name,NULL};

    if(!Snd17a(Fd,CANNA_QUERY_EXTENSIONS,names) || !Rcv2(Fd,&code))
	return 0;
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
    return prn!=0 && Snd2(Fd,prn,(int16_t)type) && Rcv2(Fd,&code) && code!=-1;
}

bool CannaKillServer(void)
{
    char code;
    return Snd1(Fd,CANNA_KILL_SERVER) && Rcv2(Fd,&code) && code==0;
}

bool CannaAutoConvert(int cxn,int bufsize,int mode)
{
    char code;

    cxn = translate_cx(cxn);
    return cxn>=0 &&
	Snd5(Fd,CANNA_AUTO_CONVERT,(int16_t)cxn,(uint16_t)bufsize,(int32_t)mode) &&
	Rcv2(Fd,&code) &&
	code==0;
}

/* 変換終了
   mode		0なら学習しない。→現在のところ、変換を取り消す。
   cl_count	文節数。０の時は現在の候補で確定する。
   can_list	各文節のカレント候補番号のリスト(cl_count個)
*/
bool CannaEndConvert(int cxn,int mode,int cl_count,const int* can_list)
{
    char code;
    cxn = translate_cx(cxn);

    //intの配列をint16の配列に変換する。はじめからint16を受けるようにした方がいいか？
    int16_t* clist16 = malloc(sizeof(*clist16)*cl_count);
    for(int n=0; n<cl_count; ++n)
	clist16[n] = can_list[n];

    bool st = (cxn>=0 &&
	       Snd10(Fd,CANNA_END_CONVERT,cxn,cl_count,mode,clist16,cl_count) &&
	       Rcv2(Fd,&code) &&
	       code==0);
    free(clist16);
    return st;
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
    //int n;
    int16_t clw;
    uint16_t* lstw=NULL;
    uint16_t* cej = ToWc(NULL,ej);

    cxn = translate_cx(cxn);
    bool st = (cxn>=0 && Snd14(Fd,CANNA_BEGIN_CONVERT,mode,cxn,cej) && Rcv7(Fd,&clw,&lstw));
    free(cej);
    if(!st){
	free(lstw);
	return NULL;
    }
    char** canl = malloc((*cl=clw)*sizeof(char*));
    for(int n=0; n<*cl; ++n)
	canl[n] = ToMb(StrListNthWc(lstw,*cl,n));
    free(lstw);
    return canl;
}

/*
  cxn=コンテキスト番号
  cl=文節番号
  返値=候補文字列と読みのリスト。エラーの時NULL
  cann=候補数。読みの数も含む。NULLでもok。
  リストはchar*の配列。最後にNULLポインタがある。各文字列と配列本体をfreeすること。
 */
char** CannaGetCandidacyList(int cxn,int cl,int* cann)
{
    int n,cann_dummy;
    int16_t cnw;
    uint16_t* lstw=NULL;

    cxn = translate_cx(cxn);
    bool st = (cxn>=0 && Snd6(Fd,CANNA_GET_CANDIDACY_LIST,cxn,cl,0xffff) && Rcv7(Fd,&cnw,&lstw));
    if(!st){
	free(lstw);
	return NULL;
    }

    if(cann==NULL)
	cann=&cann_dummy;
    char** canl = malloc(((*cann=cnw)+1)*sizeof(char*));
    for(n=0; n<*cann; ++n)
	canl[n] = ToMb(StrListNthWc(lstw,*cann,n));
    canl[n]=NULL;
    free(lstw);
    return canl;
}

//戻り値はfreeすること。
//エラーの時NULL
char* CannaGetYomi(int cxn,int cl)
{
    int16_t ylen;
    uint16_t* y2=NULL;
    char* y=NULL;
    const int bufsize=1024;

    cxn = translate_cx(cxn);
    bool st= (cxn>=0 && Snd6(Fd,CANNA_GET_YOMI,cxn,cl,bufsize) && Rcv7(Fd,&ylen,&y2));
    if(st){
	y=ToMb(y2);
    }
    free(y2);
    return y;
}

/*
  戻り値はmallocで確保される。エラーの時NULL
*/
int* WimeListContext(int* len)
{
    static int prn=0;
    int* lst=NULL;
    Rply7_t* r;

    *len = 0;
    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0 && Snd1(Fd,prn) && (r = RcvN(Fd,NULL,0))!=NULL){
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
    char code;
    bool st=false;

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
	st = (cxn>=0 && Snd11(Fd,prn,cxn,style,params,pn) && Rcv2(Fd,&code) && code==1);
    }
    return st;
}

/*
  sc	下8bit=winの仮想キーコード,上8bit=winのシフトキービット
  戻り値：WIME_SENDKEY_XXXX
	imeに処理されたとき、確定文字列があればmallocでresに返す。なければNULLが返される。
*/
int WimeSendKey(int cxn,unsigned sc,char** res)
{
    static int prn=0;
    int16_t proc;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn==0 || cxn<0 || !Snd3(Fd,prn,cxn,sc) || !Rcv6(Fd,&proc,res))
	proc = WIME_SENDKEY_ERROR;
    return proc;
}

/*
  en_ime  0=ime off, 1=ime on, -1=問い合わせ
  -1のときは0/1、サーバーが死んでいるときはfalseを返す。
  設定の時成功すればtrue
*/
bool WimeEnableIme(int cxn,int en_ime)
{
    static int prn=0;
    int16_t code;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    return (prn!=0 && cxn>=0 && Snd3(Fd,prn,cxn,en_ime) && Rcv5(Fd,&code) && code==1);
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
    return prn!=0 &&
	cxn>=0 &&
	Snd11(Fd,prn,cxn,0,(uint16_t*)ax,ITEMS(ax)) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  変換ウィンドウのフォントと背景色を指定する
  返値：フォントの高さ(ピクセル)。エラーの時0
*/
int WimeSetCompFont(int cxn,const char* font,unsigned bg)
{
    static int prn=0;
    int16_t h;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn==0 || cxn<0 || !Snd15(Fd,prn,bg,cxn,font) || !Rcv5(Fd,&h))
	h = 0;
    return h;
}

/*
  変換途中の文字列とカーソル情報を得る。
  文字列はmallocで確保される(なければNULLが返る)。
  !!!エラーの時もNULLが返るが、きちんとエラーコードを返すべきか？
 */
char* WimeGetCompStr(int cxn,WimeCompStrInfo* si)
{
    static int prn=0;
    char code=-1,*str=NULL,*dum=NULL;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    bool st = (prn!=0 && cxn>=0 && Snd2(Fd,prn,cxn) && Rcv10(Fd,&code,&str,&dum,(int32_t*)si));
    free(dum);
    if(!st || code<0){ //変換途中の文字列はなかった
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
    if(prn!=0 && cxn>=0 && Snd2(Fd,prn,cxn) && Rcv4(Fd,&st,vp=v) && st){
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
    return prn!=0 &&
	cxn>=0 &&
	Snd11(Fd,prn,cxn,style,(uint16_t*)ax,ITEMS(ax)) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  cxnに対応するXのウィンドウを登録する。
*/
bool WimeRegXWindow(int cxn,unsigned w)
{
    static int prn=0;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    PktRegXWin p = {cxn,w};
    return prn!=0 && cxn>=0 && SndN(Fd,prn,&p,sizeof(p));
}

/*
  結果文字列をucs2で返す。
  戻り値はfreeすること。
  !!!エラーの時もNULLが返るが、きちんとエラーコードを返すべきか？
*/
uint16_t* WimeGetResultStr(int cxn)
{
    static int prn=0;
    CanHeader* q=NULL;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    PktCxNum p = {cxn};
    if(prn!=0 && cxn>=0 && SndN(Fd,prn,&p,sizeof(p)) && (q=RcvN(Fd,NULL,0))!=NULL){
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
	if(p->cxn>=0 && SndN(Fd,prn,&buf,sizeof(buf)))
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
    char code;
    int32_t info[2];
    
    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    if(prn==0 || cxn<0 || !Snd11(Fd,prn,cxn,cursor,s,-1) || !Rcv4(Fd,&code,info) || !code){
	info[1] = 0;
    }
    *pos = info[0];
    return info[1];
}

/*
  フォーカスの移動を知らせる
*/
bool WimeSetFocus(int cxn,bool in)
{
    static int prn=0;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    int32_t p[] = {cxn,in};
    return prn!=0 && cxn>=0 && SndN(Fd,prn,p,sizeof(p));
}

/*
  imeのツールバーを表示する
  tb		ツールバーを表示
  comp_win	変換ウィンドウを使う
*/
bool WimeShowToolbar(int cxn,bool tb,bool comp_win)
{
    static int prn=0;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    return prn!=0 && cxn>=0 && Snd7(Fd,prn,cxn,tb,comp_win);
}

#define ALGN(n) ((n/sizeof(int)+1)*sizeof(int))

/*
  単語登録に使う品詞の一覧を得る(eucjp)
  戻り値 WimeWordStyleの配列(エラーの時NULL)。items=要素数。
*/
WimeWordStyle* WimeGetStyleList(int* items)
{
    static int prn=0;
    CanHeader* ch;
    WimeWordStyle* ws_begin=NULL;

    if(prn==0)
	prn = query_extension(__func__);
    if(prn!=0 && Snd1(Fd,prn) && (ch=RcvN(Fd,NULL,0))!=NULL){
	PktStyleList* sl = (PktStyleList*)(ch+1);
	WimeWordStyle* ws = ws_begin = malloc((sizeof(WimeWordStyle)+ALGN(sl->desc_max))*sl->count);
	int* code = sl->code;
	char* desc = (char*)(sl->code+sl->count);

	*items = sl->count;
	while(-- sl->count >= 0){
	    ws->Code = *(code++);
	    int sz = strlen(strcpy(ws->Desc,desc))+1;
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
*/
bool WimeReset(void)
{
    static int prn=0;
    void* r;
    char buf[sizeof(CanHeader)+sizeof(int)];
    CanHeader* ch = (CanHeader*)buf;

    if(prn==0)
	prn = query_extension(__func__);
    return prn!=0 &&
	Snd1(Fd,prn) &&
	(r=RcvN(Fd,ch,sizeof(buf)))!=NULL &&
	r==ch &&
	*(int*)(ch+1)==0;
}

/*
  wime側のメッセージループを回す。
  !!! WimeResetと全く同じコード。
*/
bool WimeFlushMsg(void)
{
    static int prn=0;
    void* r;
    char buf[sizeof(CanHeader)+sizeof(int)];
    CanHeader* ch = (CanHeader*)buf;

    if(prn==0)
	prn = query_extension(__func__);
    return prn!=0 &&
	Snd1(Fd,prn) &&
	(r=RcvN(Fd,ch,sizeof(buf)))!=NULL &&
	r==ch &&
	*(int*)(ch+1)==0;
}

/*
  変換候補ウィンドウの表示/非表示
*/
bool WimeShowCandidateWindow(int cxn,bool en)
{
    static int prn=0;
    char code=false;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    return prn!=0 &&
	cxn>=0 &&
	Snd3(Fd,prn,cxn,en) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  変換候補を選択する
  !!!WimeShowCandidateWindowと同じコード
*/
bool WimeSelectCandidate(int cxn,int index)
{
    static int prn=0;
    char code=false;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    return prn!=0 &&
	cxn>=0 &&
	Snd3(Fd,prn,cxn,index) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  変換候補ウィンドウを閉じる。
*/
bool WimeCloseCandidateWindow(int cxn)
{
    static int prn=0;

    cxn = translate_cx(cxn);
    if(prn==0)
	prn = query_extension(__func__);
    return prn!=0 && cxn>=0 && Snd2(Fd,prn,cxn);
}

/*
  デバッグ用
  cxnが負の時は変換せずに使用する。
  num={contex,flags}の数。エラーの時-1
  戻り値はfreeすること。
*/
uint32_t* WimeDumpContext(bool do_set,int cxn,int flags,int* num)
{
    static int prn=0;

    *num = -1;
    if(cxn >= 0){
	if((cxn = translate_cx(cxn)) < 0){
	    return NULL; //コンテキスト番号間違い
	}
    }else
	cxn = -cxn;
    if(prn==0)
	prn = query_extension(__func__);
    int16_t p1;
    uint32_t* p2;
    return prn!=0 && Snd6(Fd,prn,do_set,cxn,flags) && Rcv9v(Fd,&p1,&p2)>=0 ?
	(*num=p1,p2) : NULL;
}

/*
  verboseレベルとchannelを設定し直す。
 */
bool WimeSetDebugChannel(int level,int ch)
{
    static int prn=0;

    if(prn==0)
	prn = query_extension(__func__);
    return prn!=0 && Snd5(Fd,prn,level,0,ch);
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
    char* str;
    uint16_t* u16;
    int pos,len,cursor;
    WimeCompStrInfo si;

    /*
    if((shift & 0xff) == AUX_INPUT_MOD) //[atok]パレットからの入力
	return aux_input();
    */

    if(IsToggleKey(tk,keysym,shift)){
	bool st=true;
	if(!WimeEnableIme(cxn,IME_QUERY)){
	    //漢字モード開始
	    LOG(CH_GLOBAL,LOG_DEBUG,MESG("cxn %d:enable ime\n",cxn));
	    st = WimeEnableIme(cxn,IME_ON);
	}else{
	    //漢字モード終了
	    if((str = WimeGetCompStr(cxn,&si)) == NULL){
		LOG(CH_GLOBAL,LOG_DEBUG,MESG("cxn %d:disable ime\n",cxn));
		st = WimeEnableIme(cxn,IME_OFF);
	    }
	    /*else
	      変換途中の文字列があれば漢字モードを続ける
	    */
	}
	return st;
    }
    
    if(!WimeEnableIme(cxn,IME_QUERY)){
	return false; //直接入力中
    }

    //変換中
    unsigned wk = ConvToVk(keysym,shift);
    LOG(CH_GLOBAL,LOG_DEBUG,MESG("keysym 0x%x,shift 0x%x --> windows vk 0x%x\n",keysym,shift,wk));
    switch(WimeSendKey(cxn,wk,&str)){
    case WIME_SENDKEY_RECONV: //再変換キーだった
	if(WimeGetSurrounding == NULL){
	    return false;
	}
	u16 = (*WimeGetSurrounding)(arg,&cursor);
	LOG(CH_GLOBAL,LOG_DEBUG,MESG("cursor %d strlen %d\n",cursor,WcLen(u16)));
	if((len = WimeReconvert(cxn,u16,cursor,&pos)) == 0){
	    return false;
	}
	pos -= cursor; //元の文字列を消す（カーソルからの相対位置）
	LOG(CH_GLOBAL,LOG_DEBUG,MESG("delete pos %d,len %d\n",pos,len));
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
	if(str != NULL){
	    if(si.TargetClause == -1)
		(*WimePreedit)(str,&si,arg);	//入力途中
	    else
		(*WimeConvert)(str,&si,arg);	//変換中
	}
    }else{
	(*WimeCommit)(str,arg);			//確定
    }
    free(str);
    return true;
}



int RestartServerCount;
static WimeRestartFunc RestartFunc;
static int SocketOpt;
static void restart_server(int signum UNUSED)
{
    DisconnectServer(); //fdを作り直すために前のfdを閉じる。
    SemWait(NULL);
    SemUnlink();
    WimeInitialize(SocketOpt,0);
    ++RestartServerCount;
    LOG(CH_GLOBAL,LOG_MESSAGE,MESG("pid %d,catch server restart signal\n",(int)getpid()));
    if(RestartFunc!=NULL)
	(*RestartFunc)();
}

/*
  サーバーが再起動したとき、再接続後に呼ばれる関数を登録する。必要なければNULLを指定する。
*/
void WimeRestartSignal(WimeRestartFunc handler,int socket_opt)
{
    struct sigaction act = {.sa_handler = restart_server};
    if(sigaction(WIMERESTARTSIG,&act,NULL)!=0){
	ERR("fail sigaction:(%d) %m\n",errno);
    }
    RestartFunc = handler;
    SocketOpt = socket_opt;
}

static Array MsgBuf;

__attribute__((constructor))
static void wime_log_init(void)
{
    ArNew(&MsgBuf,1,NULL);
}

//wimeに出力できたときtrue
static bool log_v(char mark,const char* fmt,va_list vl)
{
    char code;
    bool st=false;

    ArPrintV(ArClear(&MsgBuf),fmt,vl);
    if(WimeIsConnected()){
	//wime.cのlog_req()で処理されている
	st=(Snd15(Fd,WIME_LOG,mark,0,ArAdr(&MsgBuf)) && Rcv2(Fd,&code) && code);
    }else{
	printf("%s",(const char*)ArAdr(&MsgBuf));
	st=true;
    }
    return st;
}

bool Msg(char mark,const char* fmt,...)
{
    va_list vl;
    bool st;

    va_start(vl,fmt);
    st=log_v(mark,fmt,vl);
    va_end(vl);
    return st;
}

//(C) 2008 thomas
