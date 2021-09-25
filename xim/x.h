// -*- coding:euc-jp -*-
#pragma once

#include <stdint.h>
#include <X11/Xproto.h>

#ifdef __cplusplus
extern "C" {
#endif

enum{
    XIM_CONNECT			=1,
    XIM_CONNECT_REPLY,
    XIM_DISCONNECT,
    XIM_DISCONNECT_REPLY,

    XIM_AUTH_REQUIRED		=10,
    XIM_AUTH_REPLY,
    XIM_AUTH_NEXT,
    XIM_AUTH_SETUP,
    XIM_AUTH_NG,

    XIM_ERROR			=20,

    XIM_OPEN			=30,
    XIM_OPEN_REPLY,
    XIM_CLOSE,
    XIM_CLOSE_REPLY,
    XIM_REGISTER_TRIGGERKEYS,
    XIM_TRIGGER_NOTIFY,
    XIM_TRIGGER_NOTIFY_REPLY,
    XIM_SET_EVENT_MASK,
    XIM_ENCODING_NEGOTIATION,
    XIM_ENCODING_NEGOTIATION_REPLY,
    XIM_QUERY_EXTENSION,
    XIM_QUERY_EXTENSION_REPLY,
    XIM_SET_IM_VALUES,
    XIM_SET_IM_VALUES_REPLY,
    XIM_GET_IM_VALUES,
    XIM_GET_IM_VALUES_REPLY,

    XIM_CREATE_IC		=50,
    XIM_CREATE_IC_REPLY,
    XIM_DESTROY_IC,
    XIM_DESTROY_IC_REPLY,
    XIM_SET_IC_VALUES,
    XIM_SET_IC_VALUES_REPLY,
    XIM_GET_IC_VALUES,
    XIM_GET_IC_VALUES_REPLY,
    XIM_SET_IC_FOCUS,
    XIM_UNSET_IC_FOCUS,
    XIM_FORWARD_EVENT,		//60
    XIM_SYNC,
    XIM_SYNC_REPLY,
    XIM_COMMIT,
    XIM_RESET_IC,
    XIM_RESET_IC_REPLY,

    XIM_GEOMETRY		=70,
    XIM_STR_CONVERTION,
    XIM_STR_CONVERTION_REPLY,
    XIM_PREEDIT_START,
    XIM_PREEDIT_START_REPLY,
    XIM_PREEDIT_DRAW,
    XIM_PREEDIT_CARET,
    XIM_PREEDIT_CARET_REPLY,
    XIM_PREEDIT_DONE,
    XIM_STATUS_START,
    XIM_STATUS_DRAW,
    XIM_STATUS_DONE,
    XIM_PREEDITSTATE,

    XIM_PROTO_END,

    XIM_EXT_BEGIN		=129,
    XIM_EXT_SET_EVENT_MASK	=XIM_EXT_BEGIN,
    XIM_EXT_FORWARD_KEYEVENT,
    XIM_EXT_MOVE,
    XIM_EXT_END
};

typedef struct{
    uint8_t major;
    uint8_t minor;
    uint16_t len;
}__attribute__((packed)) XimHeader;

typedef struct{
    XimHeader h;
    uint8_t order;
    uint8_t dummy;
    uint16_t client_major;
    uint16_t client_minor;
    uint16_t auth_nums;
    char *names[];
}__attribute__((packed)) XimConnect;

typedef struct{
    XimHeader h;
    uint8_t len;
    char str[];
    //char padding[]
}__attribute__((packed)) XimOpen;

typedef struct{
    XimHeader h;
    uint16_t imid;
    uint16_t dummy;
}__attribute__((packed)) XimClose;

typedef struct{
    XimHeader h;
    uint16_t p1;
    uint16_t p2;
}__attribute__((packed)) XimData_ww;

typedef struct{
    uint16_t id;
    uint16_t type;
    uint16_t len;
    char attr[];
    //char padding[]
}__attribute__((packed)) XimAttr;

typedef enum{
    ATTR_TYPE_SEP,
    ATTR_TYPE_BYTE,
    ATTR_TYPE_WORD,
    ATTR_TYPE_DWORD,
    ATTR_TYPE_STR,
    ATTR_TYPE_WINDOW,
    ATTR_TYPE_STYLES		=10,
    ATTR_TYPE_RECTANGLE,
    ATTR_TYPE_POINT,
    ATTR_TYPE_FONTSET,
    ATTR_TYPE_OPTIONS,		//??? libX11のXimProto.hにXIMOptionsがある
    ATTR_TYPE_HOTKEYTRIGGER,
    ATTR_TYPE_HOTKEYSTATE,
    ATTR_TYPE_STRCONV,
    ATTR_TYPE_PREEDIT_STATE,
    ATTR_TYPE_RESET_STATE,
    ATTR_TYPE_NESTEDLIST	=0x7fff,

} XimAttrType;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	flag;
    uint16_t	code;
    int16_t	length;
    uint16_t	detail_type;
    char	detail[];
    //char	pad[];
}__attribute__((packed)) XimError;

typedef struct{
    uint8_t len;
    char str[];
}__attribute__((packed)) Str;

typedef struct{
    uint16_t	sz;
    char	str[0];
    //pad
}__attribute__((packed)) String;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	sz;
    Str		ext[];
    //char	pad[];
}__attribute__((packed)) XimQueryExtension;

typedef struct{
    uint8_t	major;
    uint8_t	minor;
    uint16_t	len;
    char	name[];
    //char	pad[]
}__attribute__((packed)) Ext;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	len;
    Ext		ext[];
}__attribute__((packed)) XimQueryExtensionReply;

typedef struct{
    uint16_t	len;
    char	info[];
    //		pad(2+len)
}__attribute__((packed)) EncodingInfo;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	names_len;
    Str		enc[];
    //		pad(names_len)
    //		part2
}__attribute__((packed)) XimEncodingNego;

typedef struct{
    uint16_t	info_len;
    uint16_t	unused;
    EncodingInfo enc[];
}__attribute__((packed)) XimEncNegoPart2;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	sz;
    uint16_t	id[];
    //		pad(len)
}__attribute__((packed)) XimGetImValues;

typedef struct{
    uint16_t	id;
    uint16_t	sz;
    char	value[];
    //		pad(len)
}__attribute__((packed)) Attribute; //XIMATTRIBUTE,XICATTRIBUTE

typedef struct{
    uint16_t	count;
    uint16_t	unused;
    uint32_t	styles[];
}__attribute__((packed)) Styles;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	sz;
    Attribute	attrs[];
}__attribute__((packed)) XimCreateIc;

//imidとicidのみ
typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
}__attribute__((packed)) XimImIc;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint32_t	filter_event_mask;
    uint32_t	intercept_event_mask;
    uint32_t	select_event_mask;
    uint32_t	forward_event_mask;
    uint32_t	sync_event_mask;
}__attribute__((packed)) XimExtSetEventMask;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	flag;
    uint16_t	sn;
    xEvent	ev; //Xproto.h
}__attribute__((packed)) XimForwardEvent;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	flag;
    uint16_t	sn;
    uint8_t	type;
    uint8_t	keycode;
    uint16_t	state;
    uint32_t	time;
    uint32_t	window;
}__attribute__((packed)) XimExtForwardKeyEvent;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint32_t	flag;
    uint32_t	keys_list;
    uint32_t	event_mask;
}__attribute__((packed)) XimTriggerNotify;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	sz;
    uint16_t	unused;
    Attribute	attr[];
}__attribute__((packed)) XimSetIcValues;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	sz;
    uint16_t	atid[0];
    //char	pad[];
}__attribute__((packed)) XimGetIcValues;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint32_t	forward_mask;
    uint32_t	sync_mask;
}__attribute__((packed)) XimSetEventMask;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    int32_t	value;
}__attribute__((packed)) XimPreeditStartReply;

typedef struct{
    uint16_t	Feedback;
    uint16_t	Size;
    char	Str[0];
    //pad
    //sz
    //dum
    //list of feedback
}__attribute__((packed)) XimStrConvText;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	len;
    char	str[];
    //char	pad[];
}__attribute__((packed)) XimResetIcReply;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    int16_t	x;
    int16_t	y;
}__attribute__((packed)) XimExtMove;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    int32_t	caret;
    int32_t	chg_first;
    int32_t	chg_length;
    int32_t	status;
    int16_t	str_len;
    char	str[0];
    //		pad(2+len)
}__attribute__((packed)) XimPreeditDraw1;

typedef struct{
    int16_t	feedback_len;
    int16_t	dum;
    int32_t	feedback[0];
}__attribute__((packed)) XimPreeditDraw2;

#define PREEDIT_DRAW_NO_STR	1
#define PREEDIT_DRAW_NO_FB	2

typedef enum{
    BAD_ALLOC	=1,
    BAD_STYLE,
    BAD_CLIENT_WINDOW,
    BAD_FOCUS_WINDOW,
    BAD_AREA,
    BAD_SPOT_LOCATION,
    BAD_COLOR_MAP,
    BAD_ATOM,
    BAD_PIXEL,
    BAD_PIXMAP,
    BAD_NAME,
    BAD_CURSOR,
    BAD_PROTOCOL,
    BAD_FOREGROUND,
    BAD_BACKGROUND,
    LOCALE_NOT_SUPPORTED,
    BAD_SOMETHING	=999
} XimErrorCode;

static inline int Pad(int n)
{
    return (4 - n%4) % 4;
}

static inline Str* IncStr(const Str* s)
{
    return (Str*)((char*)s + sizeof(Str) + s->len);
}

static inline int StringSize(String* s)
{
    return sizeof(String) + s->sz + Pad(2+s->sz);
}

#ifdef __cplusplus
}
#endif

//(C) 2009 thomas
