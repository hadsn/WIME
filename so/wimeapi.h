// -*- coding:euc-jp -*-
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lib/array.h"
#include "exe/at.h"

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

//WimeInitialize()とcanna.c:CreateContext()で使う。
#define USE_UTF16LE_SYM1 "@utf16"
#define USE_UTF16LE_SYM2 "@utf16le"
#define USE_UTF16BE_SYM  "@utf16be"

/*
  これらの関数はサーバーが死んだ場合失敗に相当する値を返す。
*/
int CannaCreateContext(void);
bool CannaCloseContext(int cxn);
bool CannaKillServer(void);
Array* CannaBeginConvert(int cxn,int mode,const char* yomi);
bool CannaEndConvert(int cxn,int mode,int cl_count,const int* can_list);
Array* CannaGetCandidacyList(int cxn,int cl);
char* CannaGetYomi(int cx,int cl);

bool WimeIsConnected(void);
int WimeInitialize(int socket_num,int logmark);
bool WimeFinalize(void);
int WimeGetGlobalContext(void);
bool WimeOpenIMEDialog(int type);
bool WimeSetCompWin(int cxn,int style,...);
int WimeGetCompWin(int cxn,int* x,int* y,int* w,int* h);
int WimeSendKey(int cxn,unsigned xk0,unsigned xk1,unsigned mod,char** res);
bool WimeEnableIme(int cxn,int en_ime);
bool WimeMoveShadowWin(int cxn,int x,int y,int w,int h);
int WimeSetCompFont(int cxn,const char* font,unsigned bg);
char* WimeGetCompStr(int cxn,WimeCompStrInfo*);
bool WimeSetCandWin(int cxn,int style,...);
bool WimeRegXWindow(int cxn,unsigned w);
char* WimeGetResultStr(int cxn);
bool WimeSetResultStr(int cxn,const char* u8);
int WimeReconvert(int cxn,const char* u8,int cursor,int* pos);
bool WimeSetFocus(int cxn,bool in);
bool WimeShowToolbar(int cxn,bool tb,bool comp_win);
Array* WimeGetStyleList(int* items,int** code);
bool WimeReset(void);
bool WimeFlushMsg(void);
bool WimeShowCandidateWindow(int cxn,bool en);
bool WimeSelectCandidate(int cxn,int index);
bool WimeCloseCandidateWindow(int cxn);
uint32_t* WimeDumpContext(bool do_set,int cxn,int flags,int* num);
bool WimeSetDebugChannel(int level,int ch);
bool WimeGetColor(int cxn,ATImeCol* tbl);
bool WimeGetCandidateWin(int cxn,int* data);
    
//オープンされているコンテキストの数
int WimeOpenedContext(void);

void* WimeRawData(int major,int minor,const void* data,int size);

extern int RestartServerCount;
typedef void (*WimeRestartFunc)(void);
void WimeRestartSignal(WimeRestartFunc hander);

/*
  ToggleKeyの定義が必要なので、WimeProcessKeyを使うときは先にxres.hを
  インクルードしておくこと。
  xres.hはDisplayの定義が必要なので、さらにXlib.hをインクルードしなければならない。
*/
#ifdef WIME_SO_XRES
    bool WimeFilterKey(int cxn,const ToggleKey* tk,Display* disp,int keycode,int keysym0,int state,void* arg);
    extern void (*WimePreedit)(const char* u8,const WimeCompStrInfo* si,void* arg);
    extern void (*WimeConvert)(const char* u8,const WimeCompStrInfo* si,void* arg);
    extern void (*WimeCommit)(const char* u8,void* arg);
    extern char* (*WimeGetSurrounding)(int* cursor_pos,void* arg); //文字列はmallocで返すこと
    extern void (*WimeDelSurrounding)(int pos,int len,void* arg);
#endif

bool Msg(char mark,const char* fmt,...); //log.h

#ifdef __cplusplus
}
#endif

//(C) 2008 thomas
