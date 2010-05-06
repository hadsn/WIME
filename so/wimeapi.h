#ifndef WIME_SO_WIMEAPI
#define WIME_SO_WIMEAPI

#include <setjmp.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stdint.h>
#include <semaphore.h>

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

#define VK_OEM_ATTN		0xf0	//CapsLockのvkコード

#define AUX_INPUT_MOD	(Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)

typedef struct{
    int32_t	CursorPos;	//カーソル位置
    int32_t	DeltaStart;
    int32_t	TargetClause;	//注目文節の先頭位置(なければ-1)
    int32_t	TargetClLen;	//注目文節の文字数
    int32_t	Length;		//全文字数
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

bool WimeIsConnected();
bool WimeInitialize(int socket_num,int logmark);
bool WimeFinalize(void);
int WimeCreateContext(void);
bool WimeCloseContext(int cxn);
int WimeGetGlobalContext(void);
bool WimeOpenIMEDialog(int type);
bool WimeKillServer(void);
bool WimeAutoConvert(int cxn,int bufsize,int mode);
bool WimeEndConvert(int cxn,int mode,int cl_count,int* can_list,int list_len);
int* WimeListContext(int* sz);
bool WimeSetCompWin(int cxn,int style,...);
int WimeGetCompWin(int cxn,int* x,int* y,int* w,int* h);
int WimeSendKey(int cxn,unsigned sc,char** res);
bool WimeEnableIme(int cxn,int en_ime);
bool WimeMoveShadowWin(int cxn,int x,int y,int w,int h);
int WimeSetCompFont(int cxn,const char* font,unsigned bg);
char* WimeGetCompStr(int cxn,WimeCompStrInfo*);
bool WimeSetCandWin(int cxn,int style,int x,int y,...);
void WimeRegXWindow(int cxn,unsigned w);
uint16_t* WimeGetResultStr(int cxn);
bool WimeSetResultStr(int cxn,const char* ej);
int WimeReconvert(int cxn,const uint16_t* s,int cursor,int* pos);
void WimeSetFocus(int cxn,bool in);
void WimeShowToolbar(int cxn,bool tb,bool comp_win);
WimeWordStyle* WimeGetStyleList(int* items);
bool WimeReset(void);
    bool WimeFlushMsg(void);

extern int GlobalCxn;
extern jmp_buf WimeJmp;

#ifdef __cplusplus
}
#endif

#endif
