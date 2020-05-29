// -*- coding:euc-jp -*-
#ifndef WIME_SO_PKT
#define WIME_SO_PKT

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
    uint8_t Major;
    uint8_t Minor;
    uint16_t Length; //ヘッダ以外の追加データのバイト数
}__attribute__((packed)) CanHeader;

//--------------------------------------------

typedef struct{
    int32_t init;
    int32_t len;
    char info[0];
}__attribute__((packed)) Req0_t;

typedef CanHeader Req1_t;

typedef struct{
    CanHeader h;
    int16_t p1;
}__attribute__((packed)) Req2_t;

typedef struct{
    CanHeader h;
    int16_t p1;
    uint16_t p2;
}__attribute__((packed)) Req3_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    uint16_t	p2;
    uint16_t	p3;
    uint16_t	p4;
    uint16_t	p5[0];
}__attribute__((packed)) Req4_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    uint16_t	p2;
    int32_t	p3;
}__attribute__((packed)) Req5_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    int16_t	p2;
    int16_t	p3;
}__attribute__((packed)) Req7_t;

typedef struct{
    CanHeader	h;
    int16_t	p[4];
}__attribute__((packed)) Req9_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    int16_t	p2;
    int32_t	p3;
    int16_t	p4[0];
}__attribute__((packed)) Req10_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    int16_t	p2;
    uint16_t	p3[0];
}__attribute__((packed)) Req11_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    uint16_t	p2[0];
    //char	p3[0];
}__attribute__((packed)) Req12_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    char	p2[0];
    //uint16_t	p3[0];
    //uint16_t	p4;
    //uint16_t	p5;
    //uint16_t	p6;
}__attribute__((packed)) Req13_t;

typedef struct{
    CanHeader	h;
    int32_t	p1;
    int16_t	p2;
    uint16_t	p3[0];
}__attribute__((packed)) Req14_t;

typedef struct{
    CanHeader	h;
    int32_t	p1;
    int16_t	p2;
    char	p3[0];
}__attribute__((packed)) Req15_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    char	p2[0];
    //uint16_t	p3;
}__attribute__((packed)) Req16_t;

typedef struct{
    CanHeader	h;
    char	p1[0];
}__attribute__((packed)) Req17_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    char	p2[0];
    //char	p3[0];
    //uint16_t	p4;
}__attribute__((packed)) Req18_t;

typedef struct{
    CanHeader	h;
    int32_t	p1;
    uint16_t	p2;
    char	p3[0];
    //char	p4[0];
    //char	p5[0];
}__attribute__((packed)) Req21_t;

//--------------------------------------------

typedef struct{
    uint16_t minor;
    int16_t cxn;
}__attribute__((packed)) Rply0_t;

typedef struct{
    CanHeader	h;
    char	p1;
}__attribute__((packed)) Rply2_t;

typedef struct{
    CanHeader	h;
    char	p1;
    uint16_t	p2[0];
}__attribute__((packed)) Rply3_t;

typedef struct{
    CanHeader	h;
    char	p1;
    int32_t	p2[0];
}__attribute__((packed)) Rply4_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
}__attribute__((packed)) Rply5_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    char	p2[0];
}__attribute__((packed)) Rply6_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    uint16_t	p2[0];
}__attribute__((packed)) Rply7_t;

typedef struct{
    CanHeader	h;
    int16_t	p1;
    uint32_t	p2[0];
}__attribute__((packed)) Rply9_t;

typedef struct{
    CanHeader	h;
    char	p1;
    char	p2[0];
    //char	p3[];
    //int32_t	p4[];
}__attribute__((packed)) Rply10_t;

typedef struct{
    CanHeader	h;
    int32_t	pi[3];
    uint16_t	p4[0];
}__attribute__((packed)) Rply11_t;

typedef struct{
    CanHeader	h;
    uint32_t	p1;
    uint32_t	databytes;
    unsigned char bindata[0];
    //char	str;
}__attribute__((packed)) Rply64_t;

//--------------------------------------------

typedef struct{
    int32_t cxn;
    uint32_t xwin;
}__attribute__((packed)) PktRegXWin;

typedef struct{
    int32_t count;
    int32_t desc_max;
    int32_t code[];
    //char desc[];
}__attribute__((packed)) PktStyleList;

//--------------------------------------------

//wm_wime_send_charのコード
enum{
    SENDCHAR_ERROR,
    SENDCHAR_IGNORE,
    SENDCHAR_SUCCESS,
    SENDCHAR_OPENCAND,
    SENDCHAR_CLOSECAND,
    SENDCHAR_CONFIRM
};

//かんなプロトコル番号  上８ビット=minor 下８ビット=major
enum{
    CANNA_INITIALIZE		=1,
    CANNA_FINALIZE,
    CANNA_CREATE_CONTEXT,
    CANNA_DUPLICATE_CONTEXT,
    CANNA_CLOSE_CONTEXT,
    CANNA_GET_DICTIONARLY_LIST,
    CANNA_GET_DIRECTORY_LIST,
    CANNA_MOUNT_DICTIONARY,
    CANNA_UNMOUNT_DICTIONARY,
    CANNA_REMOUNT_DICTIONARY,
    CANNA_GET_MOUNT_DIC_LIST,
    CANNA_QUERY_DICTIONARY,
    CANNA_DEFINE_WORD,
    CANNA_DELETE_WORD,
    CANNA_BEGIN_CONVERT,
    CANNA_END_CONVERT,
    CANNA_GET_CANDIDACY_LIST,
    CANNA_GET_YOMI,
    CANNA_SUBST_YOMI,
    CANNA_STORE_YOMI,
    CANNA_STORE_RANGE,
    CANNA_GET_LAST_YOMI,
    CANNA_FLUSH_YOMI,
    CANNA_REMOVE_YOMI,
    CANNA_GET_SIMPLE_KANJI,
    CANNA_RESIZE_PAUSE,
    CANNA_GET_HINSHI,
    CANNA_GET_LEX,
    CANNA_GET_STATUS,
    CANNA_SET_LOCALE,
    CANNA_AUTO_CONVERT,
    CANNA_QUERY_EXTENSIONS,
    CANNA_SET_APP_NAME,
    CANNA_NOTICE_GROUP_NAME,
    CANNA_DUMMY1,
    CANNA_KILL_SERVER,

    CANNA_GET_SERVER_INFO	=0x0101,
    CANNA_GET_ACL,
    CANNA_CREATE_DIC,
    CANNA_DELETE_DIC,
    CANNA_RENAME_DIC,
    CANNA_GET_WORD_TEXT_DIC,
    CANNA_LIST_DIC,
    CANNA_SYNC,
    CANNA_CHMOD_DIC,
    CANNA_COPY_DIC,
};

#define WIME_MINOR 2
enum{
    WIME_OpenDialog		= (WIME_MINOR<<8)|1,
    WIME_SetCompositionWin,
    WIME_GetCompositionWin,
    WIME_SendKey,
    WIME_EnableIme,
    WIME_MoveShadowWin,
    WIME_SetCompositionFont,
    WIME_GetCompositionStr,
    WIME_SetCandidateWin,
    WIME_RegXWin,
    WIME_GetResultStr,
    WIME_SetResultStr,
    WIME_Reconvert,
    WIME_SetImeFocus,
    WIME_ShowToolbar,
    WIME_GetStyleList,
    WIME_ReloadConf,
    WIME_FlushMsg,
    WIME_ShowCandidateWin,
    WIME_SelectCandidate,
    WIME_CloseCandidateWin,
    WIME_DumpContext,
    WIME_SetDebugChannel,
    WIME_GetColor,
    WIME_GetCandidateWin,
    
    WIME_Log
};

#ifdef __cplusplus
}
#endif

#endif

//(C) 2008 thomas
