#ifndef WIME_SO_WIMEAPI
#define WIME_SO_WIMEAPI

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

//winの仮想キーコードの上位バイト。VkKeyScanEx()参照
#define WINMODKEY_SHIFT		(1<<0)
#define WINMODKEY_CTRL		(1<<1)
#define WINMODKEY_ALT		(1<<2)
#define WINMODKEY_HANKAKU	(1<<3)
#define WINMODKEY_LOCK		(1<<6)
#define VKMODKEY(m)		((m)<<8)

//#define VK_OEM_ATTN		0xf0	//CapsLockのvkコード

#define AUX_INPUT_MOD	(Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)

//SendKeyの戻り値
#define WIME_SENDKEY_CHGCAND	4	/*候補を変更した*/
#define WIME_SENDKEY_OPENCAND	3	/*変換候補ウィンドウが表示されようとした*/
#define WIME_SENDKEY_RECONV	2	/*再変換が要求された*/
#define WIME_SENDKEY_SUCCESS	1	/*imeに処理された*/
#define WIME_SENDKEY_ERROR	0	/*無効なコンテキスト番号*/
#define WIME_SENDKEY_NO_PROC	-1	/*imeに処理されなかった*/

typedef struct{
    int32_t	CursorPos;	//カーソル位置
    int32_t	DeltaStart;
    int32_t	TargetClause;	//注目文節の先頭位置(なければ-1)
    int32_t	TargetClLen;	//注目文節の文字数
    int32_t	Length;		//全文字数
    int32_t	TargetNum;	//注目文節番号(なければ-1)
} WimeCompStrInfo;

typedef struct{
    unsigned Size;	//この構造体の大きさ
    unsigned Code;	//品詞コード(win)
    char Desc[];	//品詞名
} WimeWordStyle;

enum{
    WIME_ERROR,

    //CFS_xxx
    WIME_POS_DEFAULT,
    WIME_POS_FORCE,
    WIME_POS_POINT,
    WIME_POS_RECT,
    WIME_POS_EXCLUDE
};

//WimeOpenIMEDialog
enum{
    WIME_DIALOG_PROPERTY,
    WIME_DIALOG_REGISTERWORD,
    WIME_DIALOG_SELECTDIC
};

//WimeGetCompFontで使うフォント情報(全部32bit)
typedef struct{
    int32_t Height, Width, Weight, Italic;
} WimeCompFontInfo;

//WimeEnableIme
#define IME_ON	1
#define IME_OFF	0
#define IME_QUERY -1

/*
  これらの関数はサーバーが死んだ場合失敗に相当する値を返す。
*/

int CannaCreateContext(void);
bool CannaCloseContext(int cxn);
bool CannaKillServer(void);
bool CannaAutoConvert(int cxn,int bufsize,int mode);
char** CannaBeginConvert(int cxn,int mode,const char* ej,int* cl);
bool CannaEndConvert(int cxn,int mode,int cl_count,const int* can_list);
char** CannaGetCandidacyList(int cxn,int cl,int* cann);
char* CannaGetYomi(int cx,int cl);

bool WimeIsConnected();
bool WimeInitialize(int socket_num,int logmark);
bool WimeFinalize(void);
int WimeGetGlobalContext(void);
bool WimeOpenIMEDialog(int type);
int* WimeListContext(int* sz);
bool WimeSetCompWin(int cxn,int style,...);
int WimeGetCompWin(int cxn,int* x,int* y,int* w,int* h);
int WimeSendKey(int cxn,unsigned sc,char** res);
bool WimeEnableIme(int cxn,int en_ime);
bool WimeMoveShadowWin(int cxn,int x,int y,int w,int h);
int WimeSetCompFont(int cxn,const char* font,unsigned bg);
char* WimeGetCompStr(int cxn,WimeCompStrInfo*);
bool WimeSetCandWin(int cxn,int style,int x,int y,...);
bool WimeRegXWindow(int cxn,unsigned w);
uint16_t* WimeGetResultStr(int cxn);
bool WimeSetResultStr(int cxn,const char* ej);
int WimeReconvert(int cxn,const uint16_t* s,int cursor,int* pos);
bool WimeSetFocus(int cxn,bool in);
bool WimeShowToolbar(int cxn,bool tb,bool comp_win);
WimeWordStyle* WimeGetStyleList(int* items);
bool WimeReset(void);
bool WimeFlushMsg(void);
bool WimeShowCandidateWindow(int cxn,bool en);
bool WimeSelectCandidate(int cxn,int index);
bool WimeCloseCandidateWindow(int cxn);
uint32_t* WimeDumpContext(int cxn,int flags,int* num);

extern int RestartServerCount;
typedef void (*WimeRestartFunc)(void);
void WimeRestartSignal(WimeRestartFunc hander,int socket_opt);

/*
  ToggleKeyの定義が必要なので、WimeProcessKeyを使うときは先にxres.hを
  インクルードしておくこと。
  xres.hはDisplayの定義が必要なので、さらにXlib.hをインクルードしなければならない。
*/
#ifdef WIME_SO_XRES
    bool WimeFilterKey(int cxn,const ToggleKey* tk,int keysym,int shift,void* arg);
    extern void (*WimePreedit)(const char* ej,const WimeCompStrInfo* si,void* arg);
    extern void (*WimeConvert)(const char* ej,const WimeCompStrInfo* si,void* arg);
    extern void (*WimeCommit)(const char* ej,void* arg);
#endif

#include "lib/log.h" //LOG,MSG,ERR

#ifdef __cplusplus
}
#endif

#endif

//(C) 2008 thomas
