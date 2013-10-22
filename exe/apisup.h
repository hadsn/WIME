#ifndef WIME_EXE_APISUP
#define WIME_EXE_APISUP

#include "canna.h"
#include "lib/log.h"

extern Array Clients;
extern Array Context;

typedef enum{
    ChangeTargetFixed = -1,
    ChangeTargetFail,
    ChangeTargetSuccess
} ChangeTargetStatus;

#define CONV_MODE (IME_CMODE_NATIVE|IME_CMODE_FULLSHAPE|IME_CMODE_ROMAN)

void InitClientData(void);
int16_t OpenConnection(int fd,const char* user);
bool CloseConnection(int fd);
CannaContext_t* OpenCannaContext(int fd,int16_t* cxn);
void CloseCannaContext(CannaContext_t* c);
CannaContext_t* ValidContext(int cxn,const char* msgtag);
CannaContext_t* FindContext(HWND wh,int16_t* cxn);
ClientData_t* FindClient(int fd);
CannaContext_t* ResetContext(CannaContext_t* cx);
void CheckCloseStWin(CannaContext_t* cx);
void ReplaceWindow(void);

int GetAttrCl(HIMC imc,char at,const CannaContext_t* cx);
ChangeTargetStatus SetTarget(HIMC imc,int tn,const CannaContext_t* cx);
void DbgComp(HIMC imc,const char* tag);
int ClauseLen(HIMC imc,const CannaContext_t* cx);
void SaveFixedClause(HIMC imc,CannaContext_t* cx);
char GetAttr(HIMC imc,int cl,const CannaContext_t* cx);
Array* GetClause(HIMC imc,const CannaContext_t* cx,int req,int n,int n_end,Array* str,char* at);

uint16_t Req2(CanHeader*);
void Req3(CanHeader*,int16_t* p1,uint16_t* p2);
uint16_t* Req4r(CanHeader*,int16_t* p1,uint16_t* p2,uint16_t* p3,uint16_t* p4);
char* Req4(CanHeader*,int16_t* p1,uint16_t* p2,uint16_t* p3,uint16_t* p4);
void Req5(CanHeader*,int16_t* p1,uint16_t* p2,int32_t* p3);
void Req6(CanHeader*,int16_t* p1,int16_t* p2,uint16_t* p3);
void Req7(CanHeader*,int16_t* p1,int16_t* p2,int16_t* p3);
void Req8(CanHeader*,int16_t* p1,int16_t* p2,int16_t* p3,uint16_t* p4);
void Req9(CanHeader*,int16_t* p1,int16_t* p2,int16_t* p3,int16_t* p4);
void *Req10(Req10_t*,int16_t* p1,int16_t* p2,int32_t* p3);
uint16_t* Req11r(CanHeader*,int16_t* p1,int16_t* p2);
char* Req11(CanHeader*,int16_t* p1,int16_t* p2);
char* Req12(Req12_t*,int16_t* p1,char** p2);
char* Req13(Req13_t*,int16_t* p1,char** p3,uint16_t* p4,uint16_t* p5,uint16_t* p6);
char* Req14(CanHeader*,int32_t* p1,int16_t* p2);
char* Req15(CanHeader*,int32_t* p1,int16_t* p2);
uint16_t Req16(Req16_t*,int16_t* p1,char** p2);
uint16_t Req18(Req18_t*,int16_t* p1,char** p2,char** p3);
char* Req19(CanHeader*,int32_t* p1,int16_t* p2,char** p3);
char* Req21(CanHeader*,int32_t* p1,int16_t* p2,char** p3,char** p4);

bool Reply2(uint8_t mj,uint8_t mn,char st);
bool Reply3(uint8_t mj,uint8_t mn,char st,const uint16_t* data,int len);
bool Reply4(uint8_t mj,uint8_t mn,char p1,const int32_t* data,int num);
bool Reply5(uint8_t mj,uint8_t mn,int16_t st);
bool Reply6(uint8_t mj,uint8_t mn,uint16_t i,const char* str,int len);
bool Reply6s(uint8_t mj,uint8_t mn,uint16_t i,const char* str);
bool Reply7(uint8_t mj,uint8_t mn,uint16_t i,uint16_t* str,int len);
bool Reply9(uint8_t mj,uint8_t mn,int16_t p1,uint32_t* p2,int p2len);
bool Reply10(uint8_t mj,uint8_t mn,char p1,const char* p2,const char* p3,const int32_t* p4,int p4size);
bool ReplyN(uint8_t mj,uint8_t mn,const void* p,unsigned size);

enum {
    CS_STR,
    CS_STRCL,
    CS_STRATTR,
    CS_READ,
    CS_READCL,
    CS_READATTR,
    CS_RESULT,
    CS_MAX
};
enum {
    EN_STR	=1<<CS_STR,
    EN_STRCL	=1<<CS_STRCL,
    EN_STRATTR	=1<<CS_STRATTR,
    EN_READ	=1<<CS_READ,
    EN_READCL	=1<<CS_READCL,
    EN_READATTR	=1<<CS_READATTR,
    EN_RESULT	=1<<CS_RESULT,
    EN_ALL	=(1<<CS_MAX)-1
};
void StoreComp(Array scs[],HIMC imc,int clpos_b,int clpos_e,int en);
bool LoadComp(Array scs[],HIMC imc);
void CompNew(Array scs[]);
void CompDelete(Array scs[]);

/*
  入力ウィンドウに関する情報を取得／設定する
*/
typedef struct{
    CANDIDATEFORM CanForm;	//ImmGetCandidateWindow
    LOGFONT Font;		//ImmGetCompositionFont
    COMPOSITIONFORM CompForm;	//ImmGetCompositionWindow
    DWORD ConvSt,SentenceSt;	//ImmGetConversionStatus
    RECT Rect;			//GetWindowRect
} DupWinParam;

DupWinParam* GetWinParam(HWND w,DupWinParam* p);
void SetWinParam(HWND w,DupWinParam* p);

#endif

//(C) 2008 thomas
