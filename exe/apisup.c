// -*- coding:euc-jp -*-
#define _GNU_SOURCE //mempcpy
#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <ddk/imm.h>
#include <limits.h>
#include "canna.h"
#include "lib/ut.h"
#include "apisup.h"
#include "lib/list.h"
#include "io/wimeio.h"
#include "lib/log.h"

#if defined(__FreeBSD__)
#include "lib/freebsd.h" //mempcpy
#endif

//入力用ウィンドウのデータ。InputWinsの要素。
typedef struct{
    HWND Win;
    HWND ImeWnd;
    HIMC DefImc;
} InputWinData;

Array Clients;
Array Context;
Array ReplyBuf;
Array InputWins;
unsigned SerialNumber; //全コンテキストの通し番号

HWND NewWin();

void reset_client_data(ClientData_t* cdt,int fd,const char* user);
CannaContext_t* reset_context(CannaContext_t* c,int fd,HWND wh,unsigned xwin);
void cd_constructor(void* p);
void cx_constructor(void* p);
int eq_wnd(const void *val,const void* elem);
int eq_fd(const void *val,const void* elem);

void InitClientData(void)
{
    ArNewPs(&Clients,sizeof(ClientData_t),cd_constructor,8);
    ArNewPs(&Context,sizeof(CannaContext_t),cx_constructor,8);
    ArNew(&InputWins,sizeof(InputWinData),NULL);
    ArNew(&ReplyBuf,1,NULL);
}

//重複がないか先に確認しておくこと
int16_t OpenConnection(int fd,const char* user)
{
    int16_t cxn;
    int zero = 0;
    ClientData_t* cdt = ArFindElemIf(&Clients,0,eq_fd,&zero); //空きを探す
    reset_client_data(cdt,fd,user);
    OpenCannaContext(fd,&cxn);
    return cxn;
}

/*??? gcc4.3.3になったら,ネスト関数から親関数の変数を参照したら落ちるようになった(close_cxから親のfdを参照したらセグフォになる)。
 */
int close_cx(CannaContext_t* cx,const int* fd)
{
    if(cx->Connection==*fd && cx->Win!=NULL)
	CloseCannaContext(cx);
    return 1;
}

//ファイルディスクリプタfdのクライアント情報を削除
bool CloseConnection(int fd)
{
    ClientData_t* cdt;
    bool st=false;
    if((cdt = FindClient(fd)) != NULL){
	cdt->Connection = 0;
	free(cdt->User);
	free(cdt->App);
	free(cdt->Group);
	ArForEach(&Context,(ArForEachFunc)close_cx,&fd);
	st=true;
    }else
	LOG(CH_CANNA,LOG_MESSAGE,MESG("already closed fd %d\n",fd));
    return st;
}

ClientData_t* FindClient(int fd)
{
    int n = ArFindIf(&Clients,0,eq_fd,&fd);
    return n>=0 ? ArElem(&Clients,n) : NULL;
}

int16_t context_number(const CannaContext_t* cx)
{
    return cx - (const CannaContext_t*)ArAdr(&Context);
}


typedef struct{
    HIMC imc;
    HWND ime_win;
} EnumImeWin;

/*
  wのプロパティIMMGWL_IMCがlp->imcと同じであればfalseをかえしループを止める。
  lp->ime_winにそのときのwをセットする
*/
BOOL CALLBACK check_ime_wnd(HWND w,LPARAM lp)
{
    BOOL r=TRUE;
    if((HIMC)GetWindowLongPtrW(w,IMMGWL_IMC) == ((EnumImeWin*)lp)->imc){
	((EnumImeWin*)lp)->ime_win = w;
	r = FALSE;
    }
    return r;
}

//imcが持っているime-windowを返す
HWND get_ime_wnd(HIMC imc)
{
    EnumImeWin e={imc,NULL};
    EnumWindows(check_ime_wnd,(LPARAM)&e);
    return e.ime_win;
}

#if 0
HIMC CreateImc(CannaContext_t* cx)
{
    HIMC imc;

    cx->DefImc = ImmAssociateContext(cx->Win,imc = ImmCreateContext());

    /* !!! ime-windowをつくらせる。memo参照。
       wineの動作に依存した処理なので、wineのバージョンが変われば変更する必要があるかも
       しれない。
    */
    ImmSetOpenStatus(imc,FALSE);

    cx->ImeWnd = get_ime_wnd(imc);
    return imc;
}

CannaContext_t* DestroyImc(CannaContext_t* cx)
{
    HIMC old = ImmAssociateContext(cx->Win,cx->DefImc);

    /*!!![wime3.3.3,wine1.1.39]
      ImmDestroyContext()のときにimeウィンドウが解体されるが、そのときにセグフォを起こすことがある。
      imcと関連ウィンドウの関係がおかしくなって、無関係のウィンドウにメッセージがいくようだ。
      memoの"imcとime window"参照。
      !!! いっそ確保したウィンドウとimcは解放せずに使い回す方がいいのではないか？
    */
    SetWindowLongPtrW(cx->ImeWnd,IMMGWL_IMC,(LONG_PTR)cx->DefImc);

    /*!!!
      これも必要なはずだが、なくても今のところ大丈夫。
      ただ完全な反則なので、どうしたものか。
    */
    const WCHAR wine_imc_prop[] = {'W','i','n','e','I','m','m','H','I','M','C','P','r','o','p','e','r','t','y',0};
    SetPropW(cx->Win,wine_imc_prop,cx->DefImc);

    ImmDestroyContext(old);

    CannaContext_t* g = ArElem(&Context,0);
    SetPropW(g->Win,wine_imc_prop,cx->DefImc);
    ImmAssociateContext(g->Win,cx->DefImc); //これも追加。

    return cx;
}
#endif

/*!!! [3.3.3]
  imcを解放するのはあきらめて、入力用ウィンドウとimcを保存し使い回すことにする。
  InputWinDataというのがかっこうわるい。何とかならんか。
*/
HWND pop_win(CannaContext_t* cx)
{
    HWND w;

    if(ArUsing(&InputWins) == 0){
	HIMC imc = ImmCreateContext();
	w = NewWin();
	cx->DefImc = ImmAssociateContext(w,imc);

	/* !!! ime-windowをつくらせる。memo参照。
	   wineの動作に依存した処理なので、wineのバージョンが変われば変更する必要が
	   あるかもしれない。
	*/
	ImmSetOpenStatus(imc,TRUE); //[r32] wine1.5.14で変更があった。
	ImmSetOpenStatus(imc,FALSE);

	cx->ImeWnd = get_ime_wnd(imc);
    }else{
	InputWinData* dt = ArElem(&InputWins,ArUsing(&InputWins)-1);
	w = dt->Win;
	cx->ImeWnd = dt->ImeWnd;
	cx->DefImc = dt->DefImc;
	ArDec(&InputWins);
    }
    return w;
}

void push_win(const CannaContext_t* cx)
{
    ArAdd1(&InputWins,&(InputWinData){cx->Win,cx->ImeWnd,cx->DefImc});
}

/*
  入力ウィンドウに関する情報を取得する
*/
DupWinParam* GetWinParam(HWND w,DupWinParam* p)
{
    HIMC imc = ImmGetContext(w);
    ImmGetCandidateWindow(imc,sizeof(CANDIDATEFORM),&p->CanForm);
    ImmGetCompositionFont(imc,&p->Font);
    ImmGetCompositionWindow(imc,&p->CompForm);
    ImmGetConversionStatus(imc,&p->ConvSt,&p->SentenceSt);
    GetWindowRect(w,&p->Rect);
    ImmReleaseContext(w,imc);
    return p;
}

/*
  入力ウィンドウに関する情報を設定する
*/
void SetWinParam(HWND w,DupWinParam* p)
{
    HIMC imc = ImmGetContext(w);
    ImmSetCandidateWindow(imc,&p->CanForm);
    ImmSetCompositionFont(imc,&p->Font);
    ImmSetCompositionWindow(imc,&p->CompForm);
    ImmSetConversionStatus(imc,p->ConvSt,p->SentenceSt);
    SetWindowPos(w,HWND_TOP,p->Rect.left,p->Rect.top,p->Rect.right-p->Rect.left,p->Rect.bottom-p->Rect.top,SWP_NOREDRAW);
    ImmReleaseContext(w,imc);
}

int free_win(InputWinData* dt,void* arg UNUSED)
{
    /*!!!
      本当ならimcも解放しなければならないが、動作が怪しいのでウィンドウだけ解放する。
      imcはメモリに残ってしまうかもしれないが今のところやむなしとする。
    */
    DestroyWindow(dt->Win);
    return 1;
}

//入力ウィンドウのみを作り直す
int replace_window(CannaContext_t* cx,Array* params)
{
    if(cx->Win != NULL){
	int cxn = ArIndex(&Context,cx);
	LOG(CH_CANNA,LOG_DEBUG,MESG("replace context %d\n",cxn));
	cx->Win = pop_win(cx);
	SetWinParam(cx->Win,ArElem(params,cxn));
    }
    return 1;
}

//使用中のコンテキストの入力ウィンドウの情報を記録してからInputWinsに入れる
int save_window_pos(CannaContext_t* cx,Array* params)
{
    DupWinParam p;
    if(cx->Win != NULL){
	GetWinParam(cx->Win,&p);
	push_win(cx);
    }
    ArAdd1(params,&p); //コンテキスト番号と合わせるためWinがなくてもプッシュする
    return 1;
}

//入力ウィンドウを作り直す
void ReplaceWindow(void)
{
    Array params;

    ArNew(&params,sizeof(DupWinParam),NULL);
    ArForEach(&Context,(ArForEachFunc)save_window_pos,&params);

    //ストックしている入力ウィンドウとimcを解放する
    ArForEach(&InputWins,(ArForEachFunc)free_win,NULL);
    ArClear(&InputWins);

    ArForEach(&Context,(ArForEachFunc)replace_window,&params);
    ArDelete(&params);
}

CannaContext_t* OpenCannaContext(int fd,int16_t* cxn)
{
    CannaContext_t* cx = ArFindElemIf(&Context,0,eq_wnd,NULL);
    HWND wh = pop_win(cx);

    reset_context(cx,fd,wh,0);
    cx->SerialNum = SerialNumber++;
    cx->Flags |= TRAP_OPEN_CAND|PROC_NOTIFY_MSG; //[r32][r107]
    if(cxn != NULL)
	*cxn = context_number(cx);
    LOG(CH_CANNA,LOG_DEBUG,MESG("wnd %p, ime-wnd %p, def-ime-wnd %p, context %hd, cx %p\n",wh,cx->ImeWnd,ImmGetDefaultIMEWnd(wh),*cxn,cx));
    return cx;
}

void CloseCannaContext(CannaContext_t* cx)
{
    if(cx != NULL){
	LOG(CH_CANNA,LOG_DEBUG,{
		HIMC imc=ImmGetContext(cx->Win);
		MESG("context %hd, wnd %p, ime-wnd %p, imc %p, default-imc %p\n",context_number(cx),cx->Win,cx->ImeWnd,imc,cx->DefImc);
		ImmReleaseContext(cx->Win,imc);
	    });
	push_win(cx);
	cx->Win = NULL;
	ArDelete(&cx->CandInfo);
	ArDelete(&cx->FixedStr);
	ArDelete(&cx->FixedYomi);
	ArDelete(&cx->Dics);
	ArDelete(&cx->DicMode);
    }
}

//fdの確認もするか？
CannaContext_t* ValidContext(int16_t cxn,const char* msgtag)
{
    CannaContext_t* cx = ArElem(&Context,cxn);
    if(cxn<0 || cxn>=ArUsing(&Context) || cx->Win==NULL){
	LOG(CH_CANNA,LOG_MESSAGE,MESG("%s:invalid context %hd\n",msgtag,cxn));
	cx = NULL;
    }
    return cx;
}

CannaContext_t* FindContext(HWND wh,int16_t* cxn)
{
    *cxn = ArFindIf(&Context,0,eq_wnd,wh);
    return *cxn!=-1 ? ArElem(&Context,*cxn) : NULL;
}

void reset_client_data(ClientData_t* cdt,int fd,const char* user)
{
    cdt->Connection = fd;
    cdt->User = user==NULL ? NULL : strdup(user);
    cdt->App = cdt->Group = NULL;
}

CannaContext_t* reset_context(CannaContext_t* cx,int fd,HWND wh,unsigned xwin)
{
    cx->Win = wh;
    cx->Connection = fd;
    ArClear(&cx->CandInfo);
    cx->FixedNum = 0;
    cx->Flags = 0;
    ArClear(&cx->FixedStr);
    ArClear(&cx->FixedYomi);
    ArClear(&cx->Dics);
    ArClear(&cx->DicMode);
    cx->XWin = xwin;
    return cx;
}

CannaContext_t* ResetContext(CannaContext_t* cx)
{
    return reset_context(cx,cx->Connection,cx->Win,cx->XWin);
}

void cd_constructor(void* p)
{
    reset_client_data((ClientData_t*)p,0,NULL);
}

void candinfo_c(void* p)
{
    *(CandListPageInfo*)p = (CandListPageInfo){0};
}

void cx_constructor(void* p)
{
    CannaContext_t* cx = (CannaContext_t*)p;
    *cx = (typeof(*cx)){0};
    ArNewPs(&(cx->CandInfo),sizeof(CandListPageInfo),candinfo_c,16);
    cx->SerialNum = SerialNumber++;
    ArNew(&cx->FixedStr,2,NULL);
    ArNew(&cx->FixedYomi,2,NULL);
    ArNew(&cx->Dics,1,NULL);
    ArNew(&cx->DicMode,4,NULL);
}

int eq_wnd(const void* elem,const void* val)
{
    return val==((CannaContext_t*)elem)->Win;
}

int eq_fd(const void* elem,const void* val)
{
    return *(int*)val == ((const ClientData_t*)elem)->Connection;
}

////////////////////////////////////////////////////////////////////////

uint16_t Req2(CanHeader* ch)
{
    return Swap2(((Req2_t*)ch)->p1);
}

void Req3(CanHeader* ch,int16_t* p1,uint16_t* p2)
{
    *p1 = Swap2(((Req3_t*)ch)->p1);
    *p2 = Swap2(((Req3_t*)ch)->p2);
}

//戻り値p5はfreeすること
uint16_t* Req4r(CanHeader* h,int16_t* p1,uint16_t* p2,uint16_t* p3,uint16_t* p4)
{
    Req9(h,p1,(int16_t*)p2,(int16_t*)p3,(int16_t*)p4);
    return h->Length==sizeof(Req4_t)-sizeof(*h) ? NULL : ((Req4_t*)h)->p5;
}

//cannaのwchar文字列をマルチバイト文字列にする
//戻り値p5はfreeすること
char* Req4(CanHeader* h,int16_t* p1,uint16_t* p2,uint16_t* p3,uint16_t* p4)
{
    return ToMb(Req4r(h,p1,p2,p3,p4));
}

void Req5(CanHeader* h,int16_t* p1,uint16_t* p2,int32_t* p3)
{
    Req10((Req10_t*)h,p1,(int16_t*)p2,p3);
}

void Req6(CanHeader* h,int16_t* p1,int16_t* p2,uint16_t* p3)
{
    Req9(h,p1,p2,(int16_t*)p3,NULL);
}

void Req7(CanHeader* h,int16_t* p1,int16_t* p2,int16_t* p3)
{
    Req9(h,p1,p2,p3,NULL);
}

void Req8(CanHeader* h,int16_t* p1,int16_t* p2,int16_t* p3,uint16_t* p4)
{
    Req9(h,p1,p2,p3,(int16_t*)p4);
}

void Req9(CanHeader* q,int16_t* p1,int16_t* p2,int16_t* p3,int16_t* p4)
{
    int16_t* p[]={p1,p2,p3,p4};
    for(int n=0; n<4; ++n)
	if(p[n] != NULL)
	    *p[n] = Swap2(((Req9_t*)q)->p[n]);
}

void* Req10(Req10_t* q,int16_t* p1,int16_t* p2,int32_t* p3)
{
    *p1 = Swap2(q->p1);
    *p2 = Swap2(q->p2);
    *p3 = Swap4(q->p3);
    int sz = (q->h.Length-(sizeof(Req10_t)-sizeof(CanHeader)))/2;
    while(--sz >= 0)
	Swap2p(q->p4+sz,1);
    return q->p4;
}

//p3はバイトの入れ換えをせずにそのまま
uint16_t* Req11r(CanHeader* ch,int16_t* p1,int16_t* p2)
{
    Req3(ch,p1,(uint16_t*)p2);
    return ch->Length==sizeof(*p1)+sizeof(*p2) ? NULL : ((Req11_t*)ch)->p3;
}

//p3は２バイト文字列であるとして１バイト文字列に変換する
//戻り値p3はfreeすること
char* Req11(CanHeader* ch,int16_t* p1,int16_t* p2)
{
    return ToMb(Req11r(ch,p1,p2));
}

//cannaのwchar文字列をマルチバイト文字列にする
//p2はfreeすること
char* Req12(Req12_t* q,int16_t* p1,char** p2)
{
    char* p3 = (char*)(WcChr(q->p2,0)+1);
    *p1 = Swap2(q->p1);
    *p2 = ToMb(q->p2);
    return p3;
}

//p3はfreeすること
char* Req13(Req13_t* q,int16_t* p1,char** p3,uint16_t* p4,uint16_t* p5,uint16_t* p6)
{
    *p1 = Swap2(q->p1);
    *p3 = strchr(q->p2,0)+1;
    uint16_t* wp = WcChr((uint16_t*)*p3,0)+1;
    *p4 = *(wp++);
    *p5 = *(wp++);
    *p6 = *(wp++);
    *p3 = ToMb((uint16_t*)*p3);
    return q->p2;
}

//cannaのwchar文字列をマルチバイト文字列にする
//戻り値p3はfreeすること
char* Req14(CanHeader* h,int32_t* p1,int16_t* p2)
{
    Req15(h,p1,p2);
    return ToMb(((Req14_t*)h)->p3);
}

char* Req15(CanHeader* h,int32_t* p1,int16_t* p2)
{
    *p1 = Swap4(((Req15_t*)h)->p1);
    *p2 = Swap2(((Req15_t*)h)->p2);
    return h->Length>sizeof(Req15_t)-sizeof(CanHeader) ? ((Req15_t*)h)->p3 : NULL;
}

//マニュアルではタイプ18とダブっていた。ListDirectory(1-7)に使うことにする
uint16_t Req16(Req16_t* q,int16_t* p1,char** p2)
{
    *p1 = Swap2(q->p1);
    *p2 = q->p2;
    return Swap2c(strchr(*p2,0)+1);
}

uint16_t Req18(Req18_t* q,int16_t* p1,char** p2,char** p3)
{
    *p1 = Swap2(q->p1);
    *p2 = q->p2;
    *p3 = strchr(*p2,0)+1;
    return Swap2c(strchr(*p3,0)+1);
}

char* Req19(CanHeader* h,int32_t* p1,int16_t* p2,char** p3)
{
    *p3 = Req15(h,p1,p2);
    return strchr(*p3,0)+1;
}

char* Req21(CanHeader* h,int32_t* p1,int16_t* p2,char** p3,char** p4)
{
    *p4 = Req19(h,p1,p2,p3);
    return strchr(*p4,0)+1;
}

//----------------------------------------------------

bool send_reply(Array* r,uint8_t mj,uint8_t mn)
{
    CanHeader* h = ArAdr(r);
    h->Major = mj;
    h->Minor = mn;
    h->Length = Swap2(ArUsing(r)-sizeof(CanHeader));
    return ImWrite(h,ArUsing(r));
}
    
bool Reply2(uint8_t mj,uint8_t mn,char st)
{
    Rply2_t* r = ArAlloc(&ReplyBuf,sizeof(Rply2_t));
    r->p1 = st;
    return send_reply(&ReplyBuf,mj,mn);
}

//len=dataの文字数(ヌル文字を含む)
bool Reply3(uint8_t mj,uint8_t mn,char st,const uint16_t* data,int len)
{
    Rply3_t* r = ArAlloc(&ReplyBuf,sizeof(Rply3_t)+len*2);
    r->p1 = st;
    memcpy(r->p2,data,len*2);
    return send_reply(&ReplyBuf,mj,mn);
}
    
bool Reply4(uint8_t mj,uint8_t mn,char p1,const int32_t* data,int num)
{
    Rply4_t* r = ArAlloc(&ReplyBuf,sizeof(Rply4_t)+num*4);
    r->p1 = p1;
    for(int n=0; n<num; ++n)
	r->p2[n] = Swap4(*(data++));
    return send_reply(&ReplyBuf,mj,mn);
}

bool Reply5(uint8_t mj,uint8_t mn,int16_t st)
{
    Rply5_t* r = ArAlloc(&ReplyBuf,sizeof(Rply5_t));
    r->p1 = Swap2(st);
    return send_reply(&ReplyBuf,mj,mn);
}

/* str==NULLのときlen=0
*/
bool Reply6(uint8_t mj,uint8_t mn,uint16_t i,const char* str,int len)
{
    if(str==NULL)
	len = 0;
    Rply6_t* r = ArAlloc(&ReplyBuf,sizeof(Rply6_t)+len);
    r->p1 = Swap2(i);
    memcpy(r->p2,str,len);
    return send_reply(&ReplyBuf,mj,mn);
}

bool Reply6s(uint8_t mj,uint8_t mn,uint16_t i,const char* str)
{
    return Reply6(mj,mn,i,str,str!=NULL?strlen(str)+1:0);
}

//len=ヌルを含む文字数
bool Reply7(uint8_t mj,uint8_t mn,uint16_t i,uint16_t* str,int len)
{
    if(str == NULL)
	len = 0;
    len *= 2;
    Rply7_t* r = ArAlloc(&ReplyBuf,sizeof(Rply7_t)+len);
    r->p1 = Swap2(i);
    memcpy(r->p2,str,len);
    return send_reply(&ReplyBuf,mj,mn);
}

//p2len=p2の個数
bool Reply9(uint8_t mj,uint8_t mn,int16_t p1,uint32_t* p2,int p2len)
{
    if(p2 == NULL)
	p2len = 0;
    Rply9_t* r = ArAlloc(&ReplyBuf,sizeof(Rply9_t)+p2len*sizeof(*p2));
    r->p1 = Swap2(p1);
    for(uint32_t* d=r->p2; p2len>0; --p2len)
	*(d++) = Swap4(*(p2++));
    return send_reply(&ReplyBuf,mj,mn);
}

//p4sizeはp4のバイト数
bool Reply10(uint8_t mj,uint8_t mn,char p1,const char* p2,const char* p3,const int32_t* p4,int p4size)
{
    char* p;
    int p2size = p2!=NULL ? strlen(p2)+1 : 0;
    int p3size = p3!=NULL ? strlen(p3)+1 : 0;
    int bufsize = sizeof(Rply10_t) + p2size + p3size + p4size;
    Rply10_t* r = ArAlloc(&ReplyBuf,bufsize);
    r->p1 = p1;
    p = mempcpy(r->p2,p2,p2size);
    p = mempcpy(p,p3,p3size);

    for(; p4size>0; p+=sizeof(*p4),++p4,p4size-=sizeof(*p4)){
	*(int32_t*)p = Swap4(*p4);
    }

    return send_reply(&ReplyBuf,mj,mn);
}

bool ReplyN(uint8_t mj,uint8_t mn,const void* p,unsigned size)
{
    if(p==NULL){
	size=0;
    }
    ArAlloc(&ReplyBuf,sizeof(CanHeader)+size);
    memcpy((CanHeader*)ArAdr(&ReplyBuf)+1,p,size);
    return send_reply(&ReplyBuf,mj,mn);
}

////////////////////////////////////////////////////////////////////////

void dbg_attr(const char* tag,char* a,int len)
{
    char sel,cnv;
    char *buf=malloc(4*len+1),*bp=buf;

    for(int n=0; n<len; ++n){
	sel=cnv='-';
	switch(*(a++)){
	case ATTR_INPUT://未選択,未変換
	    break;
	case ATTR_TARGET_CONVERTED://選択,変換
	    sel='s'; cnv='c';
	    break;
	case ATTR_CONVERTED:	//未選択,変換
	    cnv='c';
	    break;
	case ATTR_TARGET_NOTCONVERTED:	//選択,未変換
	    sel='s';
	    break;
	case ATTR_INPUT_ERROR:	//無効
	    sel=cnv='x';
	    break;
	case ATTR_FIXEDCONVERTED:
	    cnv='f';
	    break;
	default:	//不明な属性
	    sel=cnv='?';
	}
	*(bp++) = '[';
	*(bp++) = sel;
	*(bp++) = cnv;
	*(bp++) = ']';
    }
    *bp = 0;
    MESG("\t%s-attr:size %d:%s\n",tag,len,buf);
    free(buf);
}

void dbg_str(const char* tag,COMPOSITIONSTRING* cs,int stroffset,
	     int cloffset,int cllen,
	     int atoffset,int atlen,
	     bool han)
{
    uint32_t* cl = (uint32_t*)((char*)cs+cloffset);
    char* ej;
    Array lb;

    cllen /= 4;
    ArNew(&lb,1,NULL);
    MESG("\t%s-clause:size %d:%s\n",tag,cllen,(char*)ArAdr(Dump4(" %d",cl,cllen,&lb)));
    if(atoffset!=0)
	dbg_attr(tag,(char*)cs+atoffset,atlen);
    ArClear(&lb);
    for(int n=0; n<cllen-1; ++n){
	ej = (*WimeData.GetClause)(cs,stroffset,cloffset,n,1);
	ArPrint(&lb,"[%s]",ej);
	free(ej);
    }
    if(han && ArUsing(&lb)>0){
	ej = HanToZen(NULL,ArAdr(&lb),-1,false,false);
	ArPrint(ArClear(&lb),"%s",ej);
	free(ej);
    }
    MESG("\t%s-str=%s\n",tag,(char*)ArAdr(&lb));
    ArDelete(&lb);
}

void DbgComp(HIMC imc,const char* tag)
{
    if(imc == NULL){
	MESG("imc is NULL\n");
	return;
    }

    INPUTCONTEXT* ic = ImmLockIMC(imc);
    COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);

    MESG("%s:COMPOSITIONSTRING imc %p\n",tag,imc);
    dbg_str("comp",cs,cs->dwCompStrOffset,cs->dwCompClauseOffset,cs->dwCompClauseLen,cs->dwCompAttrOffset,cs->dwCompAttrLen,false);
    dbg_str("read",cs,cs->dwCompReadStrOffset,cs->dwCompReadClauseOffset,cs->dwCompReadClauseLen,cs->dwCompReadAttrOffset,cs->dwCompReadAttrLen,true);

    dbg_str("result",cs,cs->dwResultStrOffset,cs->dwResultClauseOffset,cs->dwResultClauseLen,0,0,false);
    dbg_str("result-read",cs,cs->dwResultReadStrOffset,cs->dwResultReadClauseOffset,cs->dwResultReadClauseLen,0,0,true);
    MESG("\tcursor pos=%d  delta start=%d\n",cs->dwCursorPos,cs->dwDeltaStart);

    ImmUnlockIMCC(ic->hCompStr);
    ImmUnlockIMC(imc);
}

/*
  現在の注目文節と同じ=-1
  失敗=0
  成功=1
*/
int change_attr(int target,char* attr,const int32_t* cls,int clslen)
{
    char a,*bp;
    int n,cur;
    
    //現在の注目文節を見つける
    for(cur=0; cur<clslen-1; ++cur)
	if(a=attr[cls[cur]],
	   (a==ATTR_TARGET_CONVERTED||a==ATTR_TARGET_NOTCONVERTED))
	    break;
    LOG(CH_CANNA,LOG_DEBUG,MESG("target change %d --> %d\n",cur,target));
    if(cur == clslen-1){
	LOG(CH_CANNA,LOG_MESSAGE,MESG("注目文節がない\n"));
	return 0; //注目文節がない
    }
    if(cur == target)
	return -1; //現在の注目文節と同じ

    //選択されている部分を未選択にする
    for(n=cls[cur+1]-cls[cur],bp=attr+cls[cur]; n>0; --n,++bp)
	switch(*bp){
	case ATTR_TARGET_CONVERTED:
	    *bp = ATTR_CONVERTED;
	    break;
	case ATTR_TARGET_NOTCONVERTED:
	    *bp = ATTR_INPUT;
	}

    //文節を選択する
    for(n=cls[target+1]-cls[target],bp=attr+cls[target]; n>0; --n,++bp)
	switch(*bp){
	case ATTR_CONVERTED:
	    *bp = ATTR_TARGET_CONVERTED;
	    break;
	case ATTR_INPUT:
	    *bp = ATTR_TARGET_NOTCONVERTED;
	}
    return 1;
}

/*
  注目文節を変更する
  tn=文節番号(0から)
*/
ChangeTargetStatus SetTarget(HIMC imc,int tn,const CannaContext_t* cx)
{
    if(tn < cx->FixedNum)
	return ChangeTargetFixed; //固定文節はどうしようもない

    int alen,readalen,r0,r1;
    INPUTCONTEXT* ic = ImmLockIMC(imc);
    COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);
    tn -= cx->FixedNum;

    /*
      readclsなどはImmGetCompositionString()でとってくるべきだろうが、
      wine1.0rc1ではfixmeになっているので、直接構造体を見ることにする
    */
    char attr[alen=cs->dwCompAttrLen],readattr[readalen=cs->dwCompReadAttrLen];
    memcpy(attr,(char*)cs+cs->dwCompAttrOffset,alen);
    memcpy(readattr,(char*)cs+cs->dwCompReadAttrOffset,readalen);

    int clslen = cs->dwCompClauseLen/4;
    int32_t* cls = (int32_t*)((char*)cs+cs->dwCompClauseOffset);
    int readclslen = cs->dwCompReadClauseLen/4;
    int32_t* readcls = (int32_t*)((char*)cs+cs->dwCompReadClauseOffset);

    //DbgComp(imc,"before");
    if((r0 = change_attr(tn,attr,cls,clslen)))
       r1 = change_attr(tn,readattr,readcls,readclslen);
    ImmUnlockIMCC(ic->hCompStr);
    ImmUnlockIMC(imc);

    /*
      wineのImmSetCompositionString()ではAをWにする際データを無条件でwcharにするので、charが必要なコマンドの時はWを明示した方がよさそう。
       compとread両方指定しないと失敗する
    */
    if(r0<0)
	LOG(CH_CANNA,LOG_DEBUG,MESG("clause %d is current cl.\n",cx->FixedNum+tn));
    if(r0>0 && r1>0){
	if(!(r0 = (*WimeData.SetCompStr)(imc,SCS_CHANGEATTR,attr,alen,readattr,readalen)))
	    LOG(CH_CANNA,LOG_MESSAGE,MESG("fail ImmSetCompositionStringW\n"));
#if 0
	else{
	    //??? ここでImmNotifyIMEは必要だろうか？
	    r0 = ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0);
	    if(r0){
		VERBOSE(MSG("after change\n"); DbgComp(imc,__FUNCTION__));
	    }else
		MSG("fail ImmNotifyIME\n");
	}
#endif
	//DbgComp(imc,"after");
    }
    return (r0!=0 ? ChangeTargetSuccess:ChangeTargetFail);
}

/* 固定文節をcxのメンバに保存する。(eucjpで保存)
   読み文字列は全角ひらがなで保存する。
*/
void SaveFixedClause(HIMC imc,CannaContext_t* cx)
{
    Array str;
    int n;

    ArNew(&str,1,NULL);
    for(n=cx->FixedNum; GetClause(imc,cx,GCS_RESULTSTR,n,n,&str,NULL); ++n){
	ListInsert(ArExpand(&cx->FixedStr,ArUsing(&str)),-1,ArAdr(&str));
	GetClause(imc,cx,GCS_RESULTREADSTR,n,n,&str,NULL);
	ListInsert(ArExpand(&cx->FixedYomi,ArUsing(&str)),-1,ArAdr(&str));
    }
    cx->FixedNum += n;
    ArDelete(&str);
}

/* 文節番号nからn_endまでの文字列と属性を返す。(属性は文節nのものだけを返す)
   str->blocksizeが2のときはToWc()する。
   半角カナは全角ひらがなにする。
   strはクリアしてから使用している。
   戻り値：str  何かおかしいときはNULLが返ることもある。
*/
Array* GetClause(HIMC imc,const CannaContext_t* cx,int req,int n,int n_end,Array* str,char* at)
{
    INPUTCONTEXT* ic = ImmLockIMC(imc);
    COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);
    char atdum;
    Array tmpstr,*f;
    char* ej;
    
    if(at == NULL)
	at = &atdum;
    ArNew(&tmpstr,1,NULL);

    switch(req){
    case GCS_COMPSTR:
    case GCS_RESULTSTR:
	f = (Array*)&cx->FixedStr; //!!! constはどうしよう？
	break;
    case GCS_COMPREADSTR:
    case GCS_RESULTREADSTR:
	f = (Array*)&cx->FixedYomi;
    }
    for(; n<cx->FixedNum && n<=n_end; ++n){
	ej = ListInc(ArAdr(f),n);
	ArAddN(&tmpstr,ej,strlen(ej));
    }
    *at = ATTR_FIXEDCONVERTED; //??? たぶんこれのことだろう

    ej = NULL;
    if(n>=cx->FixedNum || n<n_end){
	int str_ofs,cl_ofs,at_ofs=-1,atlen=-1,clnum;
	switch(req){
	case GCS_COMPSTR:
	    str_ofs = cs->dwCompStrOffset;
	    cl_ofs = cs->dwCompClauseOffset;
	    at_ofs = cs->dwCompAttrOffset;
	    clnum = cs->dwCompClauseLen/sizeof(DWORD)-1;
	    atlen = cs->dwCompAttrLen;
	    break;
	case GCS_COMPREADSTR:
	    str_ofs = cs->dwCompReadStrOffset;
	    cl_ofs = cs->dwCompReadClauseOffset;
	    at_ofs = cs->dwCompReadAttrOffset;
	    clnum = cs->dwCompReadClauseLen/sizeof(DWORD)-1;
	    break;
	case GCS_RESULTSTR:
	    str_ofs = cs->dwResultStrOffset;
	    cl_ofs = cs->dwResultClauseOffset;
	    clnum = cs->dwResultClauseLen/sizeof(DWORD)-1;
	    break;
	case GCS_RESULTREADSTR:
	    str_ofs = cs->dwResultReadStrOffset;
	    cl_ofs = cs->dwResultReadClauseOffset;
	    clnum = cs->dwResultReadClauseLen/sizeof(DWORD)-1;
	}
	n -= cx->FixedNum;
	if(n==0 && n_end<=0 && clnum<=0 && atlen>0){
	    /*!!!
	      vje delta2.5では、入力途中での文節情報は０のままで更新されない。
	      CompAttrLenのみ入力に応じて増加する。文字そのものはCompStrに入っている。
	      apiを使えば文字列は得られるが、１文節だけがほしい場合明示的に文節情報配列をapiで取得しなければならない。
	      csを直接使った方が全ての情報をいっぺんに得られるが、正しくはapiを使うべきだろう。
	      とりあえず今は"文節情報なし&&属性情報あり"のときのみCompStr全体をapiで取得することにする。
	    */
	    ej = (*WimeData.GetCompStr)(imc,req);
	    *at = ATTR_INPUT;
	}else if(n>=clnum || n_end>=clnum){
	    str = NULL; //文節番号が範囲外
	}else{
	    if(n_end < 0)
		n_end = clnum-1;
	    ej = (*WimeData.GetClause)(cs,str_ofs,cl_ofs,n,n_end-n+1);
	    if(at_ofs != -1){
		const int32_t *cl = (typeof(cl))((const char*)cs + cl_ofs);
		*at = *((char*)cs + at_ofs + cl[n]);
	    }else
		*at = ATTR_FIXEDCONVERTED;
	}
    }
    if(ej != NULL){
	ArAddN(&tmpstr,ej,strlen(ej));
	free(ej);
    }

    if(str != NULL){
	if(req==GCS_COMPREADSTR || req==GCS_RESULTREADSTR){
	    char* z = HanToZen(NULL,ArAdr(&tmpstr),ArUsing(&tmpstr),true,true);
	    ArAddN(ArClear(&tmpstr),z,strlen(z));
	    free(z);
	}
	ArAddChar(&tmpstr,0); //ヌル文字
	ArClear(str);
	if(ArBlockSize(str) == 1)
	    ArSwap(str,&tmpstr);
	else{
	    uint16_t* w = ToWc(NULL,ArAdr(&tmpstr));
	    ArAddN(str,w,WcLen(w)+1);
	    free(w);
	}
    }

    ArDelete(&tmpstr);
    ImmUnlockIMCC(ic->hCompStr);
    ImmUnlockIMC(imc);
    return str;
}

//全文節数を返す
int ClauseLen(HIMC imc,const CannaContext_t* cx)
{
    INPUTCONTEXT* ic = ImmLockIMC(imc);
    COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);
    int len=0;
    if(cs->dwCompClauseLen > 0)
	len += cs->dwCompClauseLen/4-1;//文節数(配列数-1)
    ImmUnlockIMCC(ic->hCompStr);
    ImmUnlockIMC(imc);
    return cx->FixedNum + len;
}

/*
  指定属性を持つ文節の番号を返す
  固定文節は無視する
*/
int GetAttrCl(HIMC imc,char at,const CannaContext_t* cx)
{
    INPUTCONTEXT* ic = ImmLockIMC(imc);
    COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);
    int num;
    int cllen = cs->dwCompClauseLen/4-1;
    int32_t* cl = (int32_t*)((char*)cs+cs->dwCompClauseOffset);
    char* attr = (char*)cs+cs->dwCompAttrOffset;

    for(num=0; num<cllen; ++num){
	if(attr[cl[num]] == at)
	    break;
    }
    
    ImmUnlockIMCC(ic->hCompStr);
    ImmUnlockIMC(imc);

    return num<cllen ? cx->FixedNum+num : -1;
}

/*
  (clpos_b<=文節番号<clpos_e)までのCOMPOSITIONSTRINGをscsに追加する
  文節番号はimcでの数値。 cx->FixedNumは考慮に入れていないのであらかじめ引いておくこと。
  文節情報には最後の要素（全文字数）が付くが、useにはカウントされない。
  ResultStrは全文字列を返すことにする。

  ClauseStr(),GetInputClause()と重複している...
 */
void StoreComp(Array scs[],HIMC imc,int clpos_b,int clpos_e,int en)
{
    INPUTCONTEXT* ic = ImmLockIMC(imc);
    COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);

    if(clpos_e < 0)
	clpos_e = cs->dwCompClauseLen/4-1;
    int32_t* cl = (int32_t*)((char*)cs+cs->dwCompClauseOffset);
    int strlen = cl[clpos_e]-cl[clpos_b];
    int32_t* rcl = (int32_t*)((char*)cs+cs->dwCompReadClauseOffset);
    int rdlen = rcl[clpos_e]-rcl[clpos_b];

    int ncl = clpos_e-clpos_b+1; //文節数+1
    int32_t cl_s[ncl],cl_r[ncl];
    for(int n=0; n<ncl; ++n){
	cl_s[n] = cl[clpos_b+n]-cl[clpos_b];
	cl_r[n] = rcl[clpos_b+n]-rcl[clpos_b];
    }
    if(en & EN_STRCL){
	ArAddN(scs+CS_STRCL,cl_s,ncl);
	-- scs[CS_STRCL].use;
    }
    if(en & EN_READCL){
	ArAddN(scs+CS_READCL,cl_r,ncl);
	-- scs[CS_READCL].use;
    }

    if(en & EN_STR)
	ArAddN(scs+CS_STR,(uint16_t*)((char*)cs+cs->dwCompStrOffset) + cl[clpos_b],strlen);
    if(en & EN_READ)
	ArAddN(scs+CS_READ,(uint16_t*)((char*)cs+cs->dwCompReadStrOffset) + rcl[clpos_b],rdlen);

    if(en & EN_STRATTR)
	ArAddN(scs+CS_STRATTR,(char*)cs+cs->dwCompAttrOffset + cl[clpos_b],strlen);
    if(en & EN_READATTR)
	ArAddN(scs+CS_READATTR,(char*)cs+cs->dwCompReadAttrOffset + rcl[clpos_b],rdlen);

    //ResultStrは全文字列を返すことにする。
    if(en & EN_RESULT)
	ArAddN(scs+CS_RESULT,(char*)cs+cs->dwResultStrOffset,cs->dwResultStrLen);

    ImmUnlockIMCC(ic->hCompStr);
    ImmUnlockIMC(imc);
}

/*
  scsの内容をimcにセットする
  注目文節が１つもないと失敗する。StoreCompの前に文節を選択しておくこと。
*/
bool LoadComp(Array scs[],HIMC imc)
{
    if(scs[CS_STR].use == 0){
	return ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CANCEL,0);
    }

    int n=0,actmax=3;
    int act[][3]={{SCS_SETSTR,CS_STR,CS_READ},
		  {SCS_CHANGECLAUSE,CS_STRCL,CS_READCL},
		  {SCS_CHANGEATTR,CS_STRATTR,CS_READATTR},
    };
    Array *css,*csr;
    /* ??? 属性の前に文節情報を設定する。
       文節情報を設定すると自動的に先頭文節が注目文節になるようだ。
       属性を設定するとき、現在の状態と同じ場合ImmSetCompositionStringWはfailを
       返す(atokのときだけか？)。
       なので、scsの先頭文節の属性が注目文節の時は何もしないことにする。
    */

    //文節情報の最後に全文字数を追加する
    ArAdd1(scs+CS_STRCL,&scs[CS_STR].use);
    ArAdd1(scs+CS_READCL,&scs[CS_READ].use);

    char a = *(char*)(scs[CS_STRATTR].adr);
    if(a==ATTR_TARGET_CONVERTED || a==ATTR_TARGET_NOTCONVERTED)
	--actmax; //属性は設定しない

    do{
	css = scs + act[n][1];
	csr = scs + act[n][2];
    }while(ImmSetCompositionStringW(imc,act[n][0],ArAdr(css),ArUsingBytes(css),ArAdr(csr),ArUsingBytes(csr)) && ++n<actmax);

    return n==actmax && ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0);
}

void CompNew(Array scs[])
{
    int bs[CS_MAX]={[CS_STR]=2,[CS_STRCL]=4,[CS_STRATTR]=1,[CS_READ]=2,[CS_READCL]=4,[CS_READATTR]=1,[CS_RESULT]=2};

    for(int n=0; n<CS_MAX; ++n)
	ArNew(scs+n,bs[n],NULL);
}

void CompDelete(Array scs[])
{
    for(int n=0; n<CS_MAX; ++n)
	ArDelete(scs+n);
}

/*
  指定文節の属性を得る
*/
char GetAttr(HIMC imc,int cl,const CannaContext_t* cx)
{
    char a;
    GetClause(imc,cx,GCS_COMPSTR,cl,cl,NULL,&a);
    return a;
}

static int clear_focus_bit(void* elem,void* arg UNUSED)
{
    ((CannaContext_t*)elem)->Flags &= ~IN_FOCUS;
    return 1;
}

/*
  cxnからcxとimcを得る。
  取得できなければメッセージを出力する。
  imcはreleaseすること。
*/
CannaContext_t* GetContext(int16_t cxn,HIMC* imc,const char* func_name)
{
    CannaContext_t* cx;
    *imc = NULL;
    if((cx = ValidContext(cxn,func_name)) != NULL){ //ログはValidContext()で出る。
	if((*imc = ImmGetContext(cx->Win)) != NULL){
	    if(GetFocus() != cx->Win){
		SetFocus(cx->Win);
		ArForEach(&Context,clear_focus_bit,NULL);
		cx->Flags |= IN_FOCUS;
	    }
	}else{
	    LOG(CH_CANNA,LOG_MESSAGE,MESG("%s:cannot get imm context for %p\n",func_name,cx->Win));
	    cx = NULL; //imcが取得できなければエラーでいいだろう。
	}
    }
    return cx;
}

//(C) 2009 thomas
