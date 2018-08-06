#ifndef WIME_EXE_APISUP
#define WIME_EXE_APISUP

#include "canna.h"

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
CannaContext_t* ValidContext(int16_t cxn,const char* msgtag);
CannaContext_t* FindContext(HWND wh,int16_t* cxn);
ClientData_t* FindClient(int fd);
CannaContext_t* ResetContext(CannaContext_t* cx);
void CheckCloseStWin(CannaContext_t* cx);
void ReplaceWindow(void);
void FromClientToU16(const CannaContext_t* cx,uint16_t* str);
void FromU16ToClient(const CannaContext_t* cx,Array* str);

int GetAttrCl(HIMC imc,char at,const CannaContext_t* cx);
ChangeTargetStatus SetTarget(HIMC imc,int tn,const CannaContext_t* cx);
void DbgComp(HIMC imc,const char* tag);
void SaveFixedClause(HIMC imc,CannaContext_t* cx);
char GetAttr(HIMC imc,int cl,const CannaContext_t* cx);
Array* ClauseStr(HIMC imc,const CannaContext_t* cx,int req,int cl_start,int cl_end,Array* str,bool zen);
CannaContext_t* GetContext(int16_t cxn,HIMC* imc,const char* func_name);
int ImcClauseInfo(HIMC imc,int req,Array* cl_info);
Array* ImcClauseAttr(HIMC imc,int req,Array* at);
Array* ImcClauseStr(HIMC imc,int req,int cl_start,int cl_end,Array* str,bool zen);

uint16_t Req2(CanHeader*);
void Req3(CanHeader*,int16_t* p1,uint16_t* p2);
uint16_t* Req4(CanHeader*,int16_t* p1,uint16_t* p2,uint16_t* p3,uint16_t* p4);
void Req5(CanHeader*,int16_t* p1,uint16_t* p2,int32_t* p3);
void Req6(CanHeader*,int16_t* p1,int16_t* p2,uint16_t* p3);
void Req7(CanHeader*,int16_t* p1,int16_t* p2,int16_t* p3);
void Req8(CanHeader*,int16_t* p1,int16_t* p2,int16_t* p3,uint16_t* p4);
void Req9(CanHeader*,int16_t* p1,int16_t* p2,int16_t* p3,int16_t* p4);
void *Req10(Req10_t*,int16_t* p1,int16_t* p2,int32_t* p3);
uint16_t* Req11(CanHeader*,int16_t* p1,int16_t* p2);
char* Req12(Req12_t*,int16_t* p1,uint16_t** p2);
char* Req13(Req13_t*,int16_t* p1,uint16_t** p3,uint16_t* p4,uint16_t* p5,uint16_t* p6);
uint16_t* Req14(CanHeader*,int32_t* p1,int16_t* p2);
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
bool Reply7(uint8_t mj,uint8_t mn,int16_t p1,uint16_t* p2,int p2len);
bool Reply8(uint8_t mj,uint8_t mn,int16_t p1,uint16_t* p2,int p2len,uint16_t* p3,int p3len);
bool Reply9(uint8_t mj,uint8_t mn,int16_t p1,uint32_t* p2,int p2len);
bool Reply10(uint8_t mj,uint8_t mn,char p1,const char* p2,const char* p3,const int32_t* p4,int p4size);
bool Reply64(uint8_t mj,uint8_t mn,unsigned p1,const void* bin,unsigned bytes,const char* str,int strbytes);
bool ReplyN(uint8_t mj,uint8_t mn,const void* p,unsigned size);

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
