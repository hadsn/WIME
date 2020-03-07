// -*- coding:euc-jp -*-
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include "corr.h"
#include "wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "lib/version.h"
#include "lib/log.h"
#include "lib/list.h"

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
  socket_num:ソケットに追加する数値。負の時は環境変数を調べる
  logmark==0のときは再スタートシグナルハンドラからの呼び出し
  使用するソケット(>=0)を返す。エラーの時は-1
*/
int WimeInitialize(int socket_num,int logmark)
{
    //3.3の仕様書は3.6p4だったり2.0だったりしてるが,とりあえず3.6にしておく。
    //???環境変数USERは必ずあるとしていいのか？
    if(logmark!=0)
	LogMark=logmark;
    SocketPath = MakeSocketPath(socket_num);
    int ret = socket_num;
    if(ConnectServer()){
	int minor,cxn;
	struct passwd* pw = getpwuid(getuid());
	char user[strlen(pw->pw_name)+sizeof(USE_UTF16LE_SYM1)];
	strcat(strcpy(user,pw->pw_name),USE_UTF16LE_SYM1);
	if(Snd0(Fd,"3.6",user) && (cxn = Rcv0(Fd,&minor))!=-1 && minor==WIME_CANNA_MINOR){
	    //LibCxn[0]はグローバルコンテキスト
	    DEBUGLOG(CH_GLOBAL,"recieved cxn %d\n",cxn);
	    *(int*)ArAlloc(ArNewPs(&LibCxn,sizeof(int),libcxn_ctr,16),1) = cxn;
	}else{
	    DisconnectServer();
	    ret = -1;
	    FATALLOG(CH_GLOBAL,"fail connect server\n");
	}
    }else
	ret = -1;

    ShmStartClient(socket_num,true); //サーバーがあってもなくてもpidの記録はしておく。
    return ret;
}

static int close_all_context(int* cxp,void* arg UNUSED)
{
    if(*cxp != EMPTY_CXN_CELL)
	CannaCloseContext(*cxp);
    return 0;
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
bool WimeIsConnected(void)
{
    return (Fd!=-1);
}

static int count_context(int* cxp,int* counter)
{
    if(*cxp != EMPTY_CXN_CELL)
	++ *counter;
    return 0;
}

//オープンされているコンテキストの数
int WimeOpenedContext(void)
{
    int counter=0;
    ArForEach(&LibCxn,(ArForEachFunc)count_context,&counter);
    return counter;
}

//エラーの時０以下
static int translate_cx(int n)
{
    int* cp = ArElem(&LibCxn,n);
    return cp!=NULL ? *cp : -1;
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

bool WimeOpenIMEDialog(int type)
{
    char code=-1;
    return Snd2(Fd,WIME_OpenDialog,(int16_t)type) && Rcv2(Fd,&code) && code!=-1;
}

bool CannaKillServer(void)
{
    char code;
    return Snd1(Fd,CANNA_KILL_SERVER) && Rcv2(Fd,&code) && code==0;
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

Array* u16list_to_u8list(uint16_t* u16_raw)
{
    Array lst16;
    ListRaw(ArNew(&lst16,2,NULL),u16_raw);
    char* u8_raw = U16ToU8(NULL,NULL,ArAdr(&lst16),ArUsing(&lst16));
    Array* lst8 = ListRaw(ArNew(NULL,1,NULL),u8_raw);
    free(u8_raw);
    ArDelete(&lst16);
    return lst8;
}

/*
  cxn=コンテキスト番号
  mode=モード
  yomi=utf8の読み（ひらがな）
  返値=各文節の最優先候補のリスト(utf8)。エラーの時NULL
*/
Array* CannaBeginConvert(int cxn,int mode,const char* yomi)
{
    int16_t clw;
    uint16_t* lstw=NULL;
    uint16_t* yomiw = U8ToU16(NULL,yomi);
    Array* lst8 = NULL;
    
    cxn = translate_cx(cxn);
    if(cxn>=0 && Snd14(Fd,CANNA_BEGIN_CONVERT,mode,cxn,yomiw) && Rcv7(Fd,&clw,&lstw)){
	lst8 = u16list_to_u8list(lstw);
    }
    free(yomiw);
    free(lstw);
    return lst8;
}

/*
  cxn=コンテキスト番号
  cl=文節番号
  返値=候補文字列と読みのリスト(utf8)。エラーの時NULL
 */
Array* CannaGetCandidacyList(int cxn,int cl)
{
    int16_t cnw;
    uint16_t* lstw = NULL;
    Array* lst8 = NULL;

    cxn = translate_cx(cxn);
    if(cxn>=0 && Snd6(Fd,CANNA_GET_CANDIDACY_LIST,cxn,cl,0xffff) && Rcv7(Fd,&cnw,&lstw)){
	lst8 = u16list_to_u8list(lstw);
    }
    free(lstw);
    return lst8;
}

//戻り値(utf8)はfreeすること。
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
	y = U16ToU8(NULL,NULL,y2,-1);
    }
    free(y2);
    return y;
}

//追加引数は全部int
bool WimeSetCompWin(int cxn,int style,...)
{
    char code;
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
    return cxn>=0 &&
	Snd11(Fd,WIME_SetCompositionWin,cxn,style,params,pn) && Rcv2(Fd,&code) &&
	code==1;
}

/*
  sc	下8bit=winの仮想キーコード,上8bit=winのシフトキービット
  戻り値：WIME_SENDKEY_XXXX
	imeに処理されたとき、確定文字列があればmallocでresに返す(utf8)。なければNULLが返される。
*/
int WimeSendKey(int cxn,unsigned sc,char** res)
{
    int16_t proc;

    cxn = translate_cx(cxn);
    if(cxn<0 || !Snd3(Fd,WIME_SendKey,cxn,sc) || !Rcv6(Fd,&proc,res))
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
    int16_t code;

    cxn = translate_cx(cxn);
    return (cxn>=0 && Snd3(Fd,WIME_EnableIme,cxn,en_ime) && Rcv5(Fd,&code) && code==1);
}

/*
  imcをもつ影ウィンドウの位置か大きさを変更する
  (x,y),(w,h)それぞれのどちらかが負であれば使用しない
*/
bool WimeMoveShadowWin(int cxn,int x,int y,int w,int h)
{
    int16_t ax[]={x,y,w,h};
    char code=false;

    cxn = translate_cx(cxn);
    return 
	cxn>=0 &&
	Snd11(Fd,WIME_MoveShadowWin,cxn,0,(uint16_t*)ax,ITEMS(ax)) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  変換ウィンドウのフォントと背景色を指定する
  返値：フォントの高さ(ピクセル)。エラーの時0
*/
int WimeSetCompFont(int cxn,const char* font,unsigned bg)
{
    int16_t h;

    cxn = translate_cx(cxn);
    if(cxn<0 || !Snd15(Fd,WIME_SetCompositionFont,bg,cxn,font) || !Rcv5(Fd,&h))
	h = 0;
    return h;
}

/*
  変換途中の文字列(utf8)とカーソル情報を得る。必要なければNULLでもよい。
  文字列はmallocで確保される(なければNULLが返る)。
  !!!エラーの時もNULLが返るが、きちんとエラーコードを返すべきか？
 */
char* WimeGetCompStr(int cxn,WimeCompStrInfo* si)
{
    int code=-1;
    char* str=NULL;

    cxn = translate_cx(cxn);
    bool st = (cxn>=0 && Snd2(Fd,WIME_GetCompositionStr,cxn) && Rcv64(Fd,(unsigned*)&code,(void**)&si,NULL,&str));
    return (st && code>0) ? str : (free(str),NULL);
}

/*
  ImmGetCompositionWindow
  返値:WIME_POS_xxx
	エラーの時0
*/
int WimeGetCompWin(int cxn,int* x,int* y,int* w,int* h)
{
    int v[5],*vp;
    char st=false;

    cxn = translate_cx(cxn);
    if(cxn>=0 && Snd2(Fd,WIME_GetCompositionWin,cxn) && Rcv4(Fd,&st,vp=v) && st){
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
    int16_t ax[6]={x,y};
    char code=false;

    cxn = translate_cx(cxn);
    if(style == WIME_POS_EXCLUDE){
	va_list vl;
	va_start(vl,y);
	for(int n=0; n<4; ++n)
	    ax[2+n] = va_arg(vl,int);
	va_end(vl);
    }
    return
	cxn>=0 &&
	Snd11(Fd,WIME_SetCandidateWin,cxn,style,(uint16_t*)ax,ITEMS(ax)) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  cxnに対応するXのウィンドウを登録する。
*/
bool WimeRegXWindow(int cxn,unsigned w)
{
    cxn = translate_cx(cxn);
    PktRegXWin p = {cxn,w};
    return cxn>=0 && SndN(Fd,WIME_RegXWin,&p,sizeof(p));
}

/*
  結果文字列をutf8で返す。
  戻り値はfreeすること。
  !!!エラーの時もNULLが返るが、きちんとエラーコードを返すべきか？
*/
char* WimeGetResultStr(int cxn)
{
    CanHeader* q = NULL;
    char* u8 = NULL;
    
    cxn = translate_cx(cxn);
    if(cxn>=0 && Snd2(Fd,WIME_GetResultStr,(int16_t)cxn) && (q=RcvN(Fd,NULL,0))!=NULL){
	if(q->Length != 0)
	    u8 = U16ToU8(NULL,NULL,(uint16_t*)(q+1),-1);
    }
    free(q);
    return u8;
}

/*
  u8を変換完了文字列としてcxnに送る。
  cxnが負の時はその絶対値をwime serverでのコンテキストidとしてそのまま使う
*/
bool WimeSetResultStr(int cxn,const char* u8)
{
    bool st=false;
    cxn = cxn>0 ? translate_cx(cxn) : -cxn;
    uint16_t* u16 = U8ToU16(NULL,u8);
    if(cxn>=0 && Snd11(Fd,WIME_SetResultStr,cxn,0,u16,-1))
	st = true;
    free(u16);
    return st;
}

/*
  再変換する。
  u8=再変換文字列(utf8)
  cursor=カーソル位置(文字単位)
  戻り値：対象部分の長さ（文字単位）。エラーの時０
	pos=対象部分の開始位置（文字単位）。
*/
int WimeReconvert(int cxn,const char* u8,int cursor,int* pos)
{
    char code;
    int32_t info[2];
    
    cxn = translate_cx(cxn);
    uint16_t* u16 = U8ToU16(NULL,u8);
    if(cxn<0 || !Snd11(Fd,WIME_Reconvert,cxn,cursor,u16,-1) || !Rcv4(Fd,&code,info) || !code){
	info[1] = 0;
    }
    free(u16);
    *pos = info[0];
    return info[1];
}

/*
  フォーカスの移動を知らせる
*/
bool WimeSetFocus(int cxn,bool in)
{
    cxn = translate_cx(cxn);
    int32_t p[] = {cxn,in};
    return cxn>=0 && SndN(Fd,WIME_SetImeFocus,p,sizeof(p));
}

/*
  imeのツールバーを表示する
  tb		ツールバーを表示
  comp_win	変換ウィンドウを使う
*/
bool WimeShowToolbar(int cxn,bool tb,bool comp_win)
{
    cxn = translate_cx(cxn);
    return cxn>=0 && Snd7(Fd,WIME_ShowToolbar,cxn,tb,comp_win);
}

#define ALGN(n) ((n/sizeof(int)+1)*sizeof(int))

/*
  単語登録に使う品詞の一覧を得る(utf8)
  戻り値 品詞名のリスト(utf8) エラーがあったときNULL
	items:配列の要素数
	code:コードの配列。freeすること
*/
Array* WimeGetStyleList(int* items,int** code)
{
    *code = NULL;
    Array* desclist=NULL;
    char* desc;
    if(Snd1(Fd,WIME_GetStyleList) && Rcv64(Fd,(unsigned*)items,(void**)code,NULL,&desc)){
	desclist = ListRaw(ArNew(NULL,1,NULL),desc);
	free(desc);
    }
    return desclist;
}

/*
  設定ファイルを再読み込みする
*/
bool WimeReset(void)
{
    void* r;
    char buf[sizeof(CanHeader)+sizeof(int)];
    CanHeader* ch = (CanHeader*)buf;

    return
	Snd1(Fd,WIME_ReloadConf) &&
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
    void* r;
    char buf[sizeof(CanHeader)+sizeof(int)];
    CanHeader* ch = (CanHeader*)buf;

    return
	Snd1(Fd,WIME_FlushMsg) &&
	(r=RcvN(Fd,ch,sizeof(buf)))!=NULL &&
	r==ch &&
	*(int*)(ch+1)==0;
}

/*
  変換候補ウィンドウの表示/非表示
*/
bool WimeShowCandidateWindow(int cxn,bool en)
{
    char code=false;

    cxn = translate_cx(cxn);
    return
	cxn>=0 &&
	Snd3(Fd,WIME_ShowCandidateWin,cxn,en) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  変換候補を選択する
  !!!WimeShowCandidateWindowと同じコード
*/
bool WimeSelectCandidate(int cxn,int index)
{
    char code=false;

    cxn = translate_cx(cxn);
    return
	cxn>=0 &&
	Snd3(Fd,WIME_SelectCandidate,cxn,index) &&
	Rcv2(Fd,&code) &&
	code;
}

/*
  変換候補ウィンドウを閉じる。
*/
bool WimeCloseCandidateWindow(int cxn)
{
    cxn = translate_cx(cxn);
    return cxn>=0 && Snd2(Fd,WIME_CloseCandidateWin,cxn);
}

/*
  デバッグ用
  cxnが負の時は変換せずに使用する。
  num={contex,flags}の数。エラーの時-1
  戻り値はfreeすること。
*/
uint32_t* WimeDumpContext(bool do_set,int cxn,int flags,int* num)
{
    *num = -1;
    if(cxn >= 0){
	if((cxn = translate_cx(cxn)) < 0){
	    return NULL; //コンテキスト番号間違い
	}
    }else
	cxn = -cxn;
    int16_t p1;
    uint32_t* p2;
    return Snd6(Fd,WIME_DumpContext,do_set,cxn,flags) && Rcv9v(Fd,&p1,&p2)>=0 ?
	(*num=p1,p2) : NULL;
}

/*
  verboseレベルとchannelを設定し直す。
*/
bool WimeSetDebugChannel(int level,int ch)
{
    return Snd5(Fd,WIME_SetDebugChannel,level,0,ch);
}

#include <X11/Xlib.h>
#include "xres.h"
#include "winkey.h"

//この３つは必ず指定すること
void (*WimePreedit)(const char* u8,const WimeCompStrInfo* si,void* arg);
void (*WimeConvert)(const char* u8,const WimeCompStrInfo* si,void* arg);
void (*WimeCommit)(const char* u8,void* arg);
//この２つはなくてもいい
char* (*WimeGetSurrounding)(void* arg,int* cursor_pos);
void (*WimeDeleteSurrounding)(void* arg,int pos,int len);

/*
		str	si
  処理なし	NULL	CursorPos=-1
  コントロール	NULL	CursorPos=0
  入力中	文字列	TargetClause=-1,CursorPos>=0
  変換中	文字列	TargetClause>=0
  確定		文字列	TargetClause=-1,CursorPos=-1
  文字列はutf8

  文字を処理したらtrueを返す
*/
bool WimeFilterKey(int cxn,const ToggleKey* tk,int keysym,int shift,void* arg)
{
    char* str;
    int pos,len,cursor;

    /*
    if((shift & 0xff) == AUX_INPUT_MOD) //[atok]パレットからの入力
	return aux_input();
    */

    if(IsToggleKey(tk,keysym,shift)){
	bool st=true;
	if(!WimeEnableIme(cxn,IME_QUERY)){
	    //漢字モード開始
	    DEBUGLOG(CH_GLOBAL,"cxn %d:enable ime\n",cxn);
	    st = WimeEnableIme(cxn,IME_ON);
	}else{
	    //漢字モード終了
	    if((str = WimeGetCompStr(cxn,NULL)) == NULL){
		DEBUGLOG(CH_GLOBAL,"cxn %d:disable ime\n",cxn);
		st = WimeEnableIme(cxn,IME_OFF);
	    }
	    /*else
	      変換途中の文字列があれば漢字モードを続ける
	    */
	    free(str); //変換途中の文字列は破棄する。
	}
	return st;
    }
    
    if(!WimeEnableIme(cxn,IME_QUERY)){
	return false; //直接入力中
    }

    //変換中
    unsigned wk = ConvToVk(keysym,shift);
    DEBUGLOG(CH_GLOBAL,"keysym 0x%x,shift 0x%x --> windows vk 0x%x\n",keysym,shift,wk);
    switch(WimeSendKey(cxn,wk,&str)){
    case WIME_SENDKEY_RECONV: //再変換キーだった
	if(WimeGetSurrounding == NULL){
	    return false;
	}
	{
	    char* u8 = (*WimeGetSurrounding)(arg,&cursor);
	    DEBUGLOG(CH_GLOBAL,"cursor %d '%U'\n",cursor,u8);
	    if((len = WimeReconvert(cxn,u8,cursor,&pos)) == 0){
		return false;
	    }
	}
	pos -= cursor; //元の文字列を消す（カーソルからの相対位置）
	DEBUGLOG(CH_GLOBAL,"delete pos %d,len %d\n",pos,len);
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
	WimeCompStrInfo si;
	if((str = WimeGetCompStr(cxn,&si)) != NULL){
	    if(si.TargetClause == -1)
		(*WimePreedit)(str,&si,arg);	//入力途中
	    else
		(*WimeConvert)(str,&si,arg);	//変換中
	}else{
	    (*WimePreedit)("",&si,arg); //bsなどで変換文字列がなくなった。
	}
    }else{
	(*WimeCommit)(str,arg);			//確定
    }
    free(str);
    return true;
}



int RestartServerCount;
static WimeRestartFunc RestartFunc;
static void restart_server(int signum UNUSED)
{
    PidTableElt elt = {0};
    ShmGetPidData(getpid(),&elt);

    DisconnectServer(); //fdを作り直すために前のfdを閉じる。
    //SemWait(NULL,elt.SocketNum);
    //SemUnlink(elt.SocketNum);
    WimeInitialize(elt.SocketNum,0);
    ++RestartServerCount;
    INFOLOG(CH_GLOBAL,"pid %d,catch server restart signal\n",(int)getpid());
    if(RestartFunc!=NULL)
	(*RestartFunc)();
}

/*
  サーバーが再起動したとき、再接続後に呼ばれる関数を登録する。必要なければNULLを指定する。
*/
void WimeRestartSignal(WimeRestartFunc handler)
{
    struct sigaction act = {.sa_handler = restart_server};
    if(sigaction(WIMERESTARTSIG,&act,NULL)!=0){
	ERR("fail sigaction:(%d) %m\n",errno);
    }
    RestartFunc = handler;
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
	st=(Snd15(Fd,WIME_Log,mark,0,ArAdr(&MsgBuf)) && Rcv2(Fd,&code) && code);
    }else{
	printf("[%c]%s",mark,(const char*)ArAdr(&MsgBuf));
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

#if 1
/*
  data,sizeにヘッダは含むが使用しない。sizeはこちらでヘッダ分を減算する。
  返されたデータはヘッダを含む。free()すること。
  入出力ともデータをそのまま渡すので、整数パラメータのバイトオーダーに注意すること。
 */
void* WimeRawData(int major,int minor,const void* data,int size)
{
    return SndN(Fd,(minor<<8)|major,((CanHeader*)data)+1,size-sizeof(CanHeader)) ? RcvN(Fd,NULL,0) : NULL;
}
#endif

//(C) 2008 thomas
