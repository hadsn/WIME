
#define _GNU_SOURCE //mempcpy strchrnul
#define _XOPEN_SOURCE //swab
#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include "canna.h"
#include "lib/ut.h"
#include "apisup.h"
#include "lib/list.h"
#include "io/wimeio.h"
#include "lib/log.h"
#include "so/wimeapi.h"

#if defined(__FreeBSD__)
#include "lib/freebsd.h" //mempcpy
#endif

//入力用ウィンドウのデータ。InputWinsの要素。
typedef struct {
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

static void reset_client_data(ClientData_t* cdt, int fd, const char* user);
static CannaContext_t* reset_context(CannaContext_t* c, int fd, HWND wh, unsigned xwin);
void cd_constructor(void* p);
void cx_constructor(void* p);
int eq_wnd(const void* elem, const void* val);
int eq_fd(const void* elem, const void* val);

void InitClientData(void)
{
	ArNewPs(&Clients, sizeof(ClientData_t), cd_constructor, 8);
	ArNewPs(&Context, sizeof(CannaContext_t), cx_constructor, 8);
	ArNew(&InputWins, sizeof(InputWinData), NULL);
	ArNew(&ReplyBuf, 1, NULL);
}

//重複がないか先に確認しておくこと
int16_t OpenConnection(int fd, const char* user)
{
	int16_t cxn;
	ClientData_t* cdt = ArFindElemIf(&Clients, 0, eq_fd, &(int){0}); //空きを探す
	reset_client_data(cdt, fd, user);
	OpenCannaContext(fd, &cxn);
	return cxn;
}

/*??? gcc4.3.3になったら,ネスト関数から親関数の変数を参照したら落ちるようになった(close_cxから親のfdを参照したらセグフォになる)。
 */
static int close_cx(CannaContext_t* cx, const int* fd)
{
	if (cx->Connection == *fd && cx->Win != NULL)
		CloseCannaContext(cx);
	return 0;
}

//ファイルディスクリプタfdのクライアント情報を削除
bool CloseConnection(int fd)
{
	ClientData_t* cdt;
	bool st = false;
	if ((cdt = FindClient(fd)) != NULL) {
		cdt->Connection = 0;
		free(cdt->User);
		free(cdt->App);
		free(cdt->Group);
		ArForEach(&Context, (ArForEachFunc)close_cx, &fd);
		st = true;
	}
	else
		INFOLOG(CH_CANNA, "already closed fd %d\n", fd);
	return st;
}

ClientData_t* FindClient(int fd)
{
	return ArElem(&Clients, ArFindIf(&Clients, 0, eq_fd, &fd));
}

static int16_t context_number(const CannaContext_t* cx)
{
	return cx - (const CannaContext_t*)ArAdr(&Context);
}


typedef struct {
	HIMC imc;
	HWND ime_win;
} EnumImeWin;


/*
  wのプロパティIMMGWL_IMCがlp->imcと同じであればfalseをかえしループを止める。
  lp->ime_winにそのときのwをセットする
*/
BOOL CALLBACK check_ime_wnd(HWND w, LPARAM lp)
{
	BOOL r = TRUE;

	//[310]IMMGWL_IMCのあるなしでチェックするように変更
	if (GetWindowLongPtrW(w, IMMGWL_IMC) && ImmGetContext(w)) {
		((EnumImeWin*)lp)->ime_win = w;
		r = FALSE;
	}
	return r;
}

//imcが持っているime-windowを返す
HWND get_ime_wnd(HIMC imc)
{
	EnumImeWin e = { imc,NULL };
	EnumWindows(check_ime_wnd, (LPARAM)&e);
	return e.ime_win;
}

#if 0
HIMC CreateImc(CannaContext_t* cx)
{
	HIMC imc;

	cx->DefImc = ImmAssociateContext(cx->Win, imc = ImmCreateContext());

	/* !!! ime-windowをつくらせる。memo参照。
	   wineの動作に依存した処理なので、wineのバージョンが変われば変更する必要があるかも
	   しれない。
	*/
	ImmSetOpenStatus(imc, FALSE);

	cx->ImeWnd = get_ime_wnd(imc);
	return imc;
}

CannaContext_t* DestroyImc(CannaContext_t* cx)
{
	HIMC old = ImmAssociateContext(cx->Win, cx->DefImc);

	/*!!![wime3.3.3,wine1.1.39]
	  ImmDestroyContext()のときにimeウィンドウが解体されるが、そのときにセグフォを起こすことがある。
	  imcと関連ウィンドウの関係がおかしくなって、無関係のウィンドウにメッセージがいくようだ。
	  memoの"imcとime window"参照。
	  !!! いっそ確保したウィンドウとimcは解放せずに使い回す方がいいのではないか？
	*/
	SetWindowLongPtrW(cx->ImeWnd, IMMGWL_IMC, (LONG_PTR)cx->DefImc);

	/*!!!
	  これも必要なはずだが、なくても今のところ大丈夫。
	  ただ完全な反則なので、どうしたものか。
	*/
	const WCHAR wine_imc_prop[] = { 'W','i','n','e','I','m','m','H','I','M','C','P','r','o','p','e','r','t','y',0 };
	SetPropW(cx->Win, wine_imc_prop, cx->DefImc);

	ImmDestroyContext(old);

	CannaContext_t* g = ArElem(&Context, 0);
	SetPropW(g->Win, wine_imc_prop, cx->DefImc);
	ImmAssociateContext(g->Win, cx->DefImc); //これも追加。

	return cx;
}
#endif

/*!!! [3.3.3]
  imcを解放するのはあきらめて、入力用ウィンドウとimcを保存し使い回すことにする。
  InputWinDataというのがかっこうわるい。何とかならんか。
*/
static HWND pop_win(CannaContext_t* cx)
{
	HWND w;

	if (ArUsing(&InputWins) == 0) {
		HIMC imc = ImmCreateContext();
		w = NewWin();
		cx->DefImc = ImmAssociateContext(w, imc);

		/* !!! ime-windowをつくらせる。memo参照。
		   wineの動作に依存した処理なので、wineのバージョンが変われば変更する必要が
		   あるかもしれない。
		*/
		ImmSetOpenStatus(imc, TRUE); //[r32] wine1.5.14で変更があった。
		ImmSetOpenStatus(imc, FALSE);

		cx->ImeWnd = get_ime_wnd(imc);
	}
	else {
		InputWinData* dt = ArElem(&InputWins, ArUsing(&InputWins) - 1);
		w = dt->Win;
		cx->ImeWnd = dt->ImeWnd;
		cx->DefImc = dt->DefImc;
		ArDec(&InputWins);
	}

	return w;
}

static void push_win(const CannaContext_t* cx)
{
	ArAdd1(&InputWins, &(InputWinData){cx->Win, cx->ImeWnd, cx->DefImc});
}

/*
  入力ウィンドウに関する情報を取得する
*/
DupWinParam* GetWinParam(HWND w, DupWinParam* p)
{
	HIMC imc = ImmGetContext(w);
	ImmGetCandidateWindow(imc, 0, &p->CanForm); //ページ0だけ取得
	ImmGetCompositionFont(imc, &p->Font);
	ImmGetCompositionWindow(imc, &p->CompForm);
	ImmGetConversionStatus(imc, &p->ConvSt, &p->SentenceSt);
	GetWindowRect(w, &p->Rect);
	ImmReleaseContext(w, imc);
	return p;
}

/*
  入力ウィンドウに関する情報を設定する
*/
void SetWinParam(HWND w, DupWinParam* p)
{
	HIMC imc = ImmGetContext(w);
	ImmSetCandidateWindow(imc, &p->CanForm);
	ImmSetCompositionFont(imc, &p->Font);
	ImmSetCompositionWindow(imc, &p->CompForm);
	SetWindowPos(w, HWND_TOP, p->Rect.left, p->Rect.top, p->Rect.right - p->Rect.left, p->Rect.bottom - p->Rect.top, SWP_NOREDRAW);
	ImmReleaseContext(w, imc);
}

static int free_win(InputWinData* dt, void* arg UNUSED)
{
	/*!!!
	  本当ならimcも解放しなければならないが、動作が怪しいのでウィンドウだけ解放する。
	  imcはメモリに残ってしまうかもしれないが今のところやむなしとする。
	*/
	DestroyWindow(dt->Win);
	return 0;
}

//入力ウィンドウのみを作り直す
static int replace_window(CannaContext_t* cx, Array* params)
{
	if (cx->Win != NULL) {
		int cxn = ArIndex(&Context, cx);
		DEBUGLOG(CH_CANNA, "replace context %d\n", cxn);
		cx->Win = pop_win(cx);
		SetWinParam(cx->Win, ArElem(params, cxn));
	}
	return 0;
}

//使用中のコンテキストの入力ウィンドウの情報を記録してからInputWinsに入れる
static int save_window_pos(CannaContext_t* cx, Array* params)
{
	DupWinParam p;
	if (cx->Win != NULL) {
		GetWinParam(cx->Win, &p);
		push_win(cx);
	}
	ArAdd1(params, &p); //コンテキスト番号と合わせるためWinがなくてもプッシュする
	return 0;
}

//入力ウィンドウを作り直す
void ReplaceWindow(void)
{
	Array params;

	ArNew(&params, sizeof(DupWinParam), NULL);
	ArForEach(&Context, (ArForEachFunc)save_window_pos, &params);

	//ストックしている入力ウィンドウとimcを解放する
	ArForEach(&InputWins, (ArForEachFunc)free_win, NULL);
	ArClear(&InputWins);

	ArForEach(&Context, (ArForEachFunc)replace_window, &params);
	ArDelete(&params);
}

CannaContext_t* OpenCannaContext(int fd, int16_t* cxn)
{
	ClientData_t* cdata = FindClient(fd);
	if (cdata == NULL)
		return NULL;

	CannaContext_t* cx = ArFindElemIf(&Context, 0, eq_wnd, NULL);
	HWND wh = pop_win(cx);

	reset_context(cx, fd, wh, 0);
	cx->SerialNum = SerialNumber++;
	cx->Flags |= TRAP_OPEN_CAND | PROC_NOTIFY_MSG; //[r32][r107]
	if (cxn != NULL)
		*cxn = context_number(cx);

	const char use_utf16le_mark1[] = USE_UTF16LE_SYM1; //wimeaph.hにある
	const char use_utf16le_mark2[] = USE_UTF16LE_SYM2;
	const char use_utf16be_mark[] = USE_UTF16BE_SYM;
	char* mark_pos = strchrnul(cdata->User, use_utf16le_mark1[0]);
	if (strcasecmp(mark_pos, use_utf16le_mark1) == 0 || strcasecmp(mark_pos, use_utf16le_mark2) == 0)
		cx->Flags |= USE_UTF16LE;
	else if (strcasecmp(mark_pos, use_utf16be_mark) == 0)
		cx->Flags |= USE_UTF16BE;

	DEBUGLOG(CH_CANNA, "wnd %p, ime-wnd %p, def-ime-wnd %p, context %hd, cx %p\n", wh, cx->ImeWnd, ImmGetDefaultIMEWnd(wh), *cxn, cx);
	return cx;
}

void CloseCannaContext(CannaContext_t* cx)
{
	if (cx != NULL) {
		DEBUGDO(CH_CANNA, {
			HIMC imc = ImmGetContext(cx->Win);
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
CannaContext_t* ValidContext(int16_t cxn, const char* msgtag)
{
	CannaContext_t* cx = ArElem(&Context, cxn);
	if (cx == NULL || cx->Win == NULL) {
		ERRORLOG(CH_CANNA, "%s:invalid context %hd\n", msgtag, cxn);
		cx = NULL;
	}
	return cx;
}

CannaContext_t* FindContext(HWND wh, int16_t* cxn)
{
	return  ArElem(&Context, *cxn = ArFindIf(&Context, 0, eq_wnd, wh));
}

static void reset_client_data(ClientData_t* cdt, int fd, const char* user)
{
	cdt->Connection = fd;
	cdt->User = user == NULL ? NULL : strdup(user);
	cdt->App = cdt->Group = NULL;
}

static CannaContext_t* reset_context(CannaContext_t* cx, int fd, HWND wh, unsigned xwin)
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
	cx->FerMode = 0;
	return cx;
}

CannaContext_t* ResetContext(CannaContext_t* cx)
{
	return reset_context(cx, cx->Connection, cx->Win, cx->XWin);
}

void cd_constructor(void* p)
{
	reset_client_data((ClientData_t*)p, 0, NULL);
}

void candinfo_c(void* p)
{
	*(CandListPageInfo*)p = (CandListPageInfo){ 0 };
}

void cx_constructor(void* p)
{
	CannaContext_t* cx = (CannaContext_t*)p;
	*cx = (typeof(*cx)){ 0 };
	ArNewPs(&(cx->CandInfo), sizeof(CandListPageInfo), candinfo_c, 16);
	cx->SerialNum = SerialNumber++;
	ArNew(&cx->FixedStr, 2, NULL);
	ArNew(&cx->FixedYomi, 2, NULL);
	ArNew(&cx->Dics, 1, NULL);
	ArNew(&cx->DicMode, 4, NULL);
}

int eq_wnd(const void* elem, const void* val)
{
	return val == ((CannaContext_t*)elem)->Win;
}

int eq_fd(const void* elem, const void* val)
{
	return *(int*)val == ((const ClientData_t*)elem)->Connection;
}

static void bswap(uint16_t* str, int len)
{
	int byte = len * 2;
	void* buf = malloc(byte);
	swab(str, buf, byte);
	memcpy(str, buf, byte);
	free(buf);
}

//リクエストの２バイト文字列をu16にする。
void FromClientToU16(const CannaContext_t* cx, uint16_t* str)
{
	if (str != NULL) {
		if ((cx->Flags & USE_UTF16) == 0)
			//le,beとも指定なし→wejをu16(le)へ
			WejToU16(str, str);
		else if ((cx->Flags & USE_UTF16BE) != 0)
			//le,beどちらかが指定されている。
			//leなら何もする必要なし。beならバイト入れ替えしてleにする。
			bswap(str, WcLen(str));
	}
}

//u16をクライアント向けの文字コードにする。
void FromU16ToClient(const CannaContext_t* cx, Array* str)
{
	if ((cx->Flags & USE_UTF16) == 0)
		//le,beとも指定なし→u16(le)をwejへ
		U16ToWej(ArAdr(str), NULL, ArAdr(str), ArUsing(str));
	else if ((cx->Flags & USE_UTF16BE) != 0)
		//クライアントはle,beどちらか
		//leなら何もする必要なし。beならバイト入れ替えしてleにする。
		bswap(ArAdr(str), ArUsing(str));
}

////////////////////////////////////////////////////////////////////////

int16_t Req2(CanHeader* ch)
{
	return Swap2(((Req2_t*)ch)->p1);
}

void Req3(CanHeader* ch, int16_t* p1, uint16_t* p2)
{
	*p1 = Swap2(((Req3_t*)ch)->p1);
	*p2 = Swap2(((Req3_t*)ch)->p2);
}

uint16_t* Req4(CanHeader* h, int16_t* p1, uint16_t* p2, uint16_t* p3, uint16_t* p4)
{
	Req9(h, p1, (int16_t*)p2, (int16_t*)p3, (int16_t*)p4);
	return h->Length == sizeof(Req4_t) - sizeof(*h) ? NULL : ((Req4_t*)h)->p5;
}

void Req5(CanHeader* h, int16_t* p1, uint16_t* p2, int32_t* p3)
{
	Req10((Req10_t*)h, p1, (int16_t*)p2, p3);
}

void Req6(CanHeader* h, int16_t* p1, int16_t* p2, uint16_t* p3)
{
	Req9(h, p1, p2, (int16_t*)p3, NULL);
}

void Req7(CanHeader* h, int16_t* p1, int16_t* p2, int16_t* p3)
{
	Req9(h, p1, p2, p3, NULL);
}

void Req8(CanHeader* h, int16_t* p1, int16_t* p2, int16_t* p3, uint16_t* p4)
{
	Req9(h, p1, p2, p3, (int16_t*)p4);
}

void Req9(CanHeader* q, int16_t* p1, int16_t* p2, int16_t* p3, int16_t* p4)
{
	int16_t* p[] = { p1,p2,p3,p4 };
	for (int n = 0; n < 4; ++n)
		if (p[n] != NULL)
			*p[n] = Swap2(((Req9_t*)q)->p[n]);
}

void* Req10(Req10_t* q, int16_t* p1, int16_t* p2, int32_t* p3)
{
	*p1 = Swap2(q->p1);
	*p2 = Swap2(q->p2);
	*p3 = Swap4(q->p3);
	int sz = (q->h.Length - (sizeof(Req10_t) - sizeof(CanHeader))) / 2;
	while (--sz >= 0)
		Swap2p(q->p4 + sz, 1);
	return q->p4;
}

//p3はバイトの入れ換えをせずにそのまま
uint16_t* Req11(CanHeader* ch, int16_t* p1, int16_t* p2)
{
	Req3(ch, p1, (uint16_t*)p2);
	return ch->Length == sizeof(*p1) + sizeof(*p2) ? NULL : ((Req11_t*)ch)->p3;
}

char* Req12(Req12_t* q, int16_t* p1, uint16_t** p2)
{
	*p1 = Swap2(q->p1);
	*p2 = q->p2;
	return (char*)(WcChr(q->p2, 0) + 1);
}

char* Req13(Req13_t* q, int16_t* p1, uint16_t** p3, uint16_t* p4, uint16_t* p5, uint16_t* p6)
{
	*p1 = Swap2(q->p1);
	*p3 = (uint16_t*)(strchr(q->p2, 0) + 1);
	uint16_t* wp = WcChr((uint16_t*)*p3, 0) + 1;
	*p4 = *(wp++);
	*p5 = *(wp++);
	*p6 = *(wp++);
	return q->p2;
}

uint16_t* Req14(CanHeader* h, int32_t* p1, int16_t* p2)
{
	return (uint16_t*)Req15(h, p1, p2);
}

char* Req15(CanHeader* h, int32_t* p1, int16_t* p2)
{
	*p1 = Swap4(((Req15_t*)h)->p1);
	*p2 = Swap2(((Req15_t*)h)->p2);
	return h->Length > sizeof(Req15_t) - sizeof(CanHeader) ? ((Req15_t*)h)->p3 : NULL;
}

//マニュアルではタイプ18とダブっていた。ListDirectory(1-7)に使うことにする
uint16_t Req16(Req16_t* q, int16_t* p1, char** p2)
{
	*p1 = Swap2(q->p1);
	*p2 = q->p2;
	return Swap2c(strchr(*p2, 0) + 1);
}

uint16_t Req18(Req18_t* q, int16_t* p1, char** p2, char** p3)
{
	*p1 = Swap2(q->p1);
	*p2 = q->p2;
	*p3 = strchr(*p2, 0) + 1;
	return Swap2c(strchr(*p3, 0) + 1);
}

char* Req19(CanHeader* h, int32_t* p1, int16_t* p2, char** p3)
{
	*p3 = Req15(h, p1, p2);
	return strchr(*p3, 0) + 1;
}

char* Req21(CanHeader* h, int32_t* p1, int16_t* p2, char** p3, char** p4)
{
	*p4 = Req19(h, p1, p2, p3);
	return strchr(*p4, 0) + 1;
}

//----------------------------------------------------

bool send_reply(Array* r, uint8_t mj, uint8_t mn)
{
	CanHeader* h = ArAdr(r);
	h->Major = mj;
	h->Minor = mn;
	h->Length = Swap2(ArUsing(r) - sizeof(CanHeader));
	return ImWrite(h, ArUsing(r));
}

bool Reply2(uint8_t mj, uint8_t mn, char st)
{
	Rply2_t* r = ArAlloc(&ReplyBuf, sizeof(Rply2_t));
	r->p1 = st;
	return send_reply(&ReplyBuf, mj, mn);
}

//len=dataの文字数(ヌル文字を含む)
bool Reply3(uint8_t mj, uint8_t mn, char st, const uint16_t* data, int len)
{
	Rply3_t* r = ArAlloc(&ReplyBuf, sizeof(Rply3_t) + len * 2);
	r->p1 = st;
	memcpy(r->p2, data, len * 2);
	return send_reply(&ReplyBuf, mj, mn);
}

bool Reply4(uint8_t mj, uint8_t mn, char p1, const int32_t* data, int num)
{
	Rply4_t* r = ArAlloc(&ReplyBuf, sizeof(Rply4_t) + num * 4);
	r->p1 = p1;
	for (int n = 0; n < num; ++n)
		r->p2[n] = Swap4(*(data++));
	return send_reply(&ReplyBuf, mj, mn);
}

bool Reply5(uint8_t mj, uint8_t mn, int16_t st)
{
	Rply5_t* r = ArAlloc(&ReplyBuf, sizeof(Rply5_t));
	r->p1 = Swap2(st);
	return send_reply(&ReplyBuf, mj, mn);
}

/* str==NULLのときlen=0
*/
bool Reply6(uint8_t mj, uint8_t mn, int16_t i, const char* str, int len)
{
	if (str == NULL)
		len = 0;
	Rply6_t* r = ArAlloc(&ReplyBuf, sizeof(Rply6_t) + len);
	r->p1 = Swap2((uint16_t)i);
	memcpy(r->p2, str, len);
	return send_reply(&ReplyBuf, mj, mn);
}

bool Reply6s(uint8_t mj, uint8_t mn, int16_t i, const char* str)
{
	return Reply6(mj, mn, i, str, str != NULL ? strlen(str) + 1 : 0);
}

//p2len=ヌルを含む文字数
bool Reply7(uint8_t mj, uint8_t mn, int16_t p1, uint16_t* p2, int p2len)
{
	return Reply8(mj, mn, p1, p2, p2len, NULL, 0);
}

//len<0のときはWcLenで長さを得る。
bool Reply8(uint8_t mj, uint8_t mn, int16_t p1, uint16_t* p2, int p2len, uint16_t* p3, int p3len)
{
	int sendbytes = sizeof(Rply7_t);
	int p2bytes = 0, p3bytes = 0;

	if (p2 != NULL)
		sendbytes += (p2bytes = (p2len < 0 ? WcLen(p2) + 1 : p2len) * 2);
	if (p3 != NULL)
		sendbytes += (p3bytes = (p3len < 0 ? WcLen(p3) + 1 : p3len) * 2);
	Rply7_t* r = ArAlloc(&ReplyBuf, sendbytes);
	r->p1 = Swap2(p1);
	memcpy(r->p2, p2, p2bytes);
	memcpy((char*)r->p2 + p2bytes, p3, p3bytes);
	return send_reply(&ReplyBuf, mj, mn);
}

//p2len=p2の個数
bool Reply9(uint8_t mj, uint8_t mn, int16_t p1, int32_t* p2, int p2len)
{
	if (p2 == NULL)
		p2len = 0;
	Rply9_t* r = ArAlloc(&ReplyBuf, sizeof(Rply9_t) + p2len * sizeof(*p2));
	r->p1 = Swap2(p1);
	for (uint32_t* d = r->p2; p2len > 0; --p2len)
		*(d++) = Swap4(*(p2++));
	return send_reply(&ReplyBuf, mj, mn);
}

//p4sizeはp4のバイト数
bool Reply10(uint8_t mj, uint8_t mn, char p1, const char* p2, const char* p3, const int32_t* p4, unsigned p4size)
{
	char* p;
	unsigned p2size = p2 != NULL ? strlen(p2) + 1 : 0;
	unsigned p3size = p3 != NULL ? strlen(p3) + 1 : 0;
	unsigned bufsize = sizeof(Rply10_t) + p2size + p3size + p4size;
	Rply10_t* r = ArAlloc(&ReplyBuf, bufsize);
	r->p1 = p1;
	p = mempcpy(r->p2, p2, p2size);
	p = mempcpy(p, p3, p3size);

	for (; p4size > 0; p += sizeof(*p4), ++p4, p4size -= sizeof(*p4)) {
		*(int32_t*)p = Swap4(*p4);
	}

	return send_reply(&ReplyBuf, mj, mn);
}

bool Reply64(uint8_t mj, uint8_t mn, unsigned p1, const void* bin, unsigned bytes, const char* str, int strbytes)
{
	unsigned bufsize = sizeof(Rply64_t) + bytes;
	if (str == NULL)
		strbytes = 0;
	else
		if (strbytes < 0)
			strbytes = strlen(str) + 1;
	bufsize += strbytes;
	Rply64_t* r = ArAlloc(&ReplyBuf, bufsize);;
	r->p1 = p1;
	memcpy(r->bindata, bin, r->databytes = bytes);
	memcpy(r->bindata + bytes, str, strbytes);
	return send_reply(&ReplyBuf, mj, mn);
}

bool ReplyN(uint8_t mj, uint8_t mn, const void* p, unsigned size)
{
	if (p == NULL) {
		size = 0;
	}
	ArAlloc(&ReplyBuf, sizeof(CanHeader) + size);
	memcpy((CanHeader*)ArAdr(&ReplyBuf) + 1, p, size);
	return send_reply(&ReplyBuf, mj, mn);
}

////////////////////////////////////////////////////////////////////////

/*
  属性情報を書き換える。
*/
static void change_attr(Array* at, Array* cl, int oldindex, int newindex)
{
	//選択されている部分を未選択にする
	for (int pos = ARVAL(int32_t, cl, oldindex); pos < ARVAL(int32_t, cl, oldindex + 1); ++pos)
		switch (ARVAL(char, at, pos)) {
		case ATTR_TARGET_CONVERTED:
			ARVAL(char, at, pos) = ATTR_CONVERTED;
			break;
		case ATTR_TARGET_NOTCONVERTED:
			ARVAL(char, at, pos) = ATTR_INPUT;
		}

	//文節を選択する
	for (int pos = ARVAL(int32_t, cl, newindex); pos < ARVAL(int32_t, cl, newindex + 1); ++pos)
		switch (ARVAL(char, at, pos)) {
		case ATTR_CONVERTED:
			ARVAL(char, at, pos) = ATTR_TARGET_CONVERTED;
			break;
		case ATTR_INPUT:
			ARVAL(char, at, pos) = ATTR_TARGET_NOTCONVERTED;
		}
}

/*
  注目文節を変更する
*/
ChangeTargetStatus SetTarget(HIMC imc, int newindex, const CannaContext_t* cx)
{
	if (newindex < cx->FixedNum)
		return ChangeTargetFixed; //固定文節はどうしようもない

	int oldindex;
	if ((oldindex = GetAttrCl(imc, ATTR_TARGET_CONVERTED, cx)) < 0)
		oldindex = GetAttrCl(imc, ATTR_TARGET_NOTCONVERTED, cx);
	if (oldindex < 0)
		return ChangeTargetFail; //注目文節がない。???この状態を変更できるのか？
	if (oldindex == newindex) {
		//すでに注目文節になっている。
		DEBUGLOG(CH_CANNA, "clause %d is current cl.\n", newindex);
		return ChangeTargetSuccess;
	}

	/*
	  readclsなどはImmGetCompositionString()でとってくるべきだろうが、
	  wine1.0rc1ではfixmeになっているので、直接構造体を見ることにする
	*/
	Array compat, readat, compcl, readcl;
	ImcClauseAttr(imc, GCS_COMPSTR, ArNew(&compat, 1, NULL));
	ImcClauseAttr(imc, GCS_COMPREADSTR, ArNew(&readat, 1, NULL));
	ImcClauseInfo(imc, GCS_COMPSTR, ArNew(&compcl, 4, NULL));
	ImcClauseInfo(imc, GCS_COMPREADSTR, ArNew(&readcl, 4, NULL));

	change_attr(&compat, &compcl, oldindex, newindex);
	change_attr(&readat, &readcl, oldindex, newindex);

	/*
	  wineのImmSetCompositionString()ではAをWにする際データを無条件でwcharにするので、charが必要なコマンドの時はWを明示した方がよさそう。
	   compとread両方指定しないと失敗する
	*/
	ChangeTargetStatus st = ChangeTargetSuccess;
	if (!(*WimeData.SetCompStr)(imc, SCS_CHANGEATTR, ArAdr(&compat), ArUsing(&compat), ArAdr(&readat), ArUsing(&readat))) {
		ERRORLOG(CH_CANNA, "fail ImmSetCompositionStringW\n");
		st = ChangeTargetFail;
	}

	ArDelete(&compat);
	ArDelete(&readat);
	ArDelete(&compcl);
	ArDelete(&readcl);
	return st;
}

/* 文節番号cl_start以上cl_end未満までの文字列をu16で返す。zenがtrueで読み文字列なら全角にする。
   cl_end<0のとき最後の文節まで。strはクリアせず追加し、ヌル文字を付ける。
   固定文節も対称にする。
   戻り値：str  何かおかしいときはNULLが返ることもある。
*/
Array* ClauseStr(HIMC imc, const CannaContext_t* cx, int req, int cl_start, int cl_end, Array* str, bool zen)
{
	int str_start = ArUsing(str); //全角変換するときはここ以降を対象にする。
	const Array* fixedstrs = NULL;
	switch (req) {
	case GCS_COMPSTR:
	case GCS_RESULTSTR:
		fixedstrs = (const Array*)&cx->FixedStr;
		zen = false;
		break;
	case GCS_COMPREADSTR:
	case GCS_RESULTREADSTR:
		fixedstrs = (const Array*)&cx->FixedYomi;
	}
	for (int cl = cl_start; cl < cl_end && cl < cx->FixedNum; ++cl) {
		const uint16_t* u = ListInc(fixedstrs, cl);
		ArAddN(str, u, WcLen(u)); //ヌル文字はついていない。
	}
	if (zen && ArUsing(str) > str_start) {
		//追加されていればその部分を全角に変換
		int outlen;
		uint16_t* adr = ARELEM(uint16_t, str, str_start);
		U16HanToZenHira(adr, &outlen, adr, ArUsing(str) - str_start);
		ArSetUsing(str, str_start + outlen);
	}
	if (cl_end > 0 && cl_end <= cx->FixedNum) {
		//固定済み文節のみ
		return ArAdd1(str, &(uint16_t){0});
	}

	cl_start -= cx->FixedNum;
	cl_end -= cx->FixedNum;
	return ImcClauseStr(imc, req, cl_start, cl_end, str, zen);
}


/* 固定文節をcxのメンバに保存する。(u16で保存)
   読み文字列は全角ひらがなで保存する。
*/
void SaveFixedClause(HIMC imc, CannaContext_t* cx)
{
	Array str;
	int n;

	ArNew(&str, ArBlockSize(&cx->FixedStr), NULL);
	for (n = cx->FixedNum; ClauseStr(imc, cx, GCS_RESULTSTR, n, n + 1, ArClear(&str), false); ++n) {
		ListInsert(&cx->FixedStr, -1, &str);
		ClauseStr(imc, cx, GCS_RESULTREADSTR, n, n + 1, ArClear(&str), false);
		ListInsert(&cx->FixedYomi, -1, &str);
	}
	cx->FixedNum = n;
	ArDelete(&str);
}

/*
  指定属性を持つ文節の番号(固定文節込み)を返す。見つからないときは-1
*/
int GetAttrCl(HIMC imc, char at, const CannaContext_t* cx)
{
	int clindex;
	char a;
	for (clindex = 0; a = GetAttr(imc, clindex, cx), a != ATTR_INPUT_ERROR && a != at; ++clindex)
		;
	return a != ATTR_INPUT_ERROR ? clindex : -1;
}

/*
  文節長さ情報を返す。bs=4にすること。NULLのときは何も返さない。
  戻り値はimcの文節数。文字列がないときは-1。配列の大きさは(戻り値+1)になる。
  確定文節も含めるときはFixedNumを合計すること。
*/
int ImcClauseInfo(HIMC imc, int req, Array* cl_info)
{
	INPUTCONTEXT* ic = ImmLockIMC(imc);
	COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);

	unsigned array_size = 0, offset = 0;
	switch (req) {
	case GCS_COMPSTR:
		array_size = cs->dwCompClauseLen;
		offset = cs->dwCompClauseOffset;
		break;
	case GCS_COMPREADSTR:
		array_size = cs->dwCompReadClauseLen;
		offset = cs->dwCompReadClauseOffset;
		break;
	case GCS_RESULTSTR:
		array_size = cs->dwResultClauseLen;
		offset = cs->dwResultClauseOffset;
		break;
	case GCS_RESULTREADSTR:
		array_size = cs->dwResultReadClauseLen;
		offset = cs->dwResultReadClauseOffset;
	}
	int arraylen = array_size / 4; //文節数+1(=配列数)
	ArAddN(cl_info, (char*)cs + offset, arraylen);

	ImmUnlockIMCC(ic->hCompStr);
	ImmUnlockIMC(imc);
	return arraylen - 1;
}

/*
  cxnからcxとimcを得る。
  取得できなければメッセージを出力する。
  imcはreleaseすること。
*/
CannaContext_t* GetContext(int16_t cxn, HIMC* imc, const char* func_name)
{
	CannaContext_t* cx;
	*imc = NULL;
	if ((cx = ValidContext(cxn, func_name)) != NULL) { //ログはValidContext()で出る。
		if ((*imc = ImmGetContext(cx->Win)) != NULL) {
			if (GetFocus() != cx->Win) {
				SetFocus(cx->Win);
			}
		}
		else {
			ERRORLOG(CH_CANNA, "%s:cannot get imm context for %p\n", func_name, cx->Win);
			cx = NULL; //imcが取得できなければエラーでいいだろう。
		}
	}
	return cx;
}

/*
  属性配列を取得する。ブロックサイズ1。
 */
Array* ImcClauseAttr(HIMC imc, int req, Array* at)
{
	INPUTCONTEXT* ic = ImmLockIMC(imc);
	COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);

	unsigned array_size = 0, offset = 0; //=0は警告消しのため
	switch (req) {
	case GCS_COMPSTR:
		array_size = cs->dwCompAttrLen;
		offset = cs->dwCompAttrOffset;
		break;
	case GCS_COMPREADSTR:
		array_size = cs->dwCompReadAttrLen;
		offset = cs->dwCompReadAttrOffset;
		break;
	default:
		at = NULL;
	}
	ArAddN(ArClear(at), (char*)cs + offset, array_size);

	ImmUnlockIMCC(ic->hCompStr);
	ImmUnlockIMC(imc);
	return at;
}

/*
  指定文節(固定文節込み)の属性を得る。エラー(文節番号間違い)の時はATTR_INPUT_ERROR
*/
char GetAttr(HIMC imc, int cl, const CannaContext_t* cx)
{
	if ((cl -= cx->FixedNum) < 0)
		return ATTR_FIXEDCONVERTED; //??? たぶんこれのことだろう
	char at = ATTR_INPUT_ERROR;
	Array cl_info;
	int num = ImcClauseInfo(imc, GCS_COMPSTR, ArNew(&cl_info, 4, NULL));
	if (cl < num) {
		Array attr;
		ImcClauseAttr(imc, GCS_COMPSTR, ArNew(&attr, 1, NULL));
		at = ARVAL(char, &attr, ARVAL(int32_t, &cl_info, cl));
		ArDelete(&attr);
	}
	ArDelete(&cl_info);
	return at;
}

/* 文節番号cl_start以上cl_end未満までの文字列をu16で返す。zenがtrueで読み文字列なら全角にする。
   cl_end<0のとき最後の文節まで。strはクリアせず追加し、ヌル文字を付ける。
   固定文節は対象にしない。
   戻り値：str  何かおかしいときはNULLが返ることもある。
*/
Array* ImcClauseStr(HIMC imc, int req, int cl_start, int cl_end, Array* str, bool zen)
{
	Array clinfo;
	int clnum = ImcClauseInfo(imc, req, ArNew(&clinfo, 4, NULL));
	if (cl_end < 0)
		cl_end = clnum;
	if (cl_start<0 || cl_start>clnum || cl_end > clnum || cl_start == cl_end) {
		ArDelete(&clinfo);
		return NULL; //文節番号が範囲外
	}

	INPUTCONTEXT* ic = ImmLockIMC(imc);
	COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);
	unsigned str_ofs = 0;
	switch (req) {
	case GCS_COMPSTR:
		str_ofs = cs->dwCompStrOffset;
		zen = false;
		break;
	case GCS_COMPREADSTR:
		str_ofs = cs->dwCompReadStrOffset;
		break;
	case GCS_RESULTSTR:
		str_ofs = cs->dwResultStrOffset;
		zen = false;
		break;
	case GCS_RESULTREADSTR:
		str_ofs = cs->dwResultReadStrOffset;
	}

	int offset = ArUsing(str);
	int len = ARVAL(int32_t, &clinfo, cl_end) - ARVAL(int32_t, &clinfo, cl_start);
	ArAddN(str, (char*)cs + str_ofs + ARVAL(int32_t, &clinfo, cl_start) * 2, len);
	ArAdd1(str, &(uint16_t){0}); //ヌル文字
	if (zen) {
		//追加部分だけを全角にする。
		uint16_t* adr = ARELEM(uint16_t, str, offset);
		U16HanToZenHira(adr, NULL, adr, -1);
		ArSetUsing(str, WcLen(ArAdr(str)) + 1); //全体の文字数を数え直し
	}
	ImmUnlockIMCC(ic->hCompStr);
	ImmUnlockIMC(imc);
	ArDelete(&clinfo);
	return str;
}

void dbg_str(const char* tag, HIMC imc, int req)
{
	Array cl_info;
	const int cl_num = ImcClauseInfo(imc, req, ArNew(&cl_info, 4, NULL));
	MESG("\t%s-clause:size %d:%#*.4D\n", tag, cl_num + 1, cl_num + 1, ArAdr(&cl_info));
	ArDelete(&cl_info);

	Array attr;
	if (ImcClauseAttr(imc, req, ArNew(&attr, 1, NULL)) != NULL) {
		char* buf = calloc(ArUsing(&attr) + 1, 4);
		char* buf0 = buf;
		for (int pos = 0; pos < ArUsing(&attr); ++pos) {
			char sel, cnv;
			sel = cnv = '-';
			switch (ARVAL(char, &attr, pos)) {
			case ATTR_INPUT://未選択,未変換
				break;
			case ATTR_TARGET_CONVERTED://選択,変換
				sel = 's'; cnv = 'c';
				break;
			case ATTR_CONVERTED:	//未選択,変換
				cnv = 'c';
				break;
			case ATTR_TARGET_NOTCONVERTED:	//選択,未変換
				sel = 's';
				break;
			case ATTR_INPUT_ERROR:	//無効
				sel = cnv = 'x';
				break;
			case ATTR_FIXEDCONVERTED:
				cnv = 'f';
				break;
			default:	//不明な属性
				sel = cnv = '?';
			}
			*(buf++) = '[';
			*(buf++) = sel;
			*(buf++) = cnv;
			*(buf++) = ']';
		}
		MESG("\t%s-attr:size %d:%s\n", tag, ArUsing(&attr), buf0);
		free(buf0);
	}
	ArDelete(&attr);

	Array str;
	ArNew(&str, 2, NULL);
	for (int index = 0; index < cl_num; ++index) {
		ArAdd1(&str, &(uint16_t){L'['});
		ArDec(ImcClauseStr(imc, req, index, index + 1, &str, true));
		ArAdd1(&str, &(uint16_t){L']'});
	}
	ArAdd1(&str, &(uint16_t){0});
	MESG("\t%s-str:%W\n", tag, ArAdr(&str));
	ArDelete(&str);
}

void DbgComp(HIMC imc, const char* tag)
{
	if (imc == NULL) {
		MESG("imc is NULL\n");
		return;
	}
	MESG("%s:COMPOSITIONSTRING imc %p\n", tag, imc);
	dbg_str("comp", imc, GCS_COMPSTR);
	dbg_str("read", imc, GCS_COMPREADSTR);
	dbg_str("result", imc, GCS_RESULTSTR);
	dbg_str("result-read", imc, GCS_RESULTREADSTR);
	INPUTCONTEXT* ic = ImmLockIMC(imc);
	COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);
	MESG("\tcursor pos=%d  delta start=%d\n", cs->dwCursorPos, cs->dwDeltaStart);
	ImmUnlockIMCC(ic->hCompStr);
	ImmUnlockIMC(imc);
}

//(C) 2009 thomas
