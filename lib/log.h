// -*- coding:euc-jp -*-
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

//これをどこかで実装する必要がある
bool Msg(char mark,const char* fmt,...) __attribute__((format(printf,2,3)));
extern int Verbose;
extern char LogMark;

#define MESG(fmt,...) Msg(LogMark,"%s:" fmt,__func__,## __VA_ARGS__)
#define ERR(fmt,...) do{MESG(fmt,## __VA_ARGS__);fprintf(stderr,"[%c]%s:%d:" fmt,LogMark,__func__,__LINE__,## __VA_ARGS__);}while(0)


#define CH_GLOBAL	(1<<0) //一般情報
#define CH_COMPOSITION	(1<<1) //WM_IME_COMPOSITIONメッセージ
#define CH_NOTIFY	(1<<2) //WM_IME_NOTIFYメッセージ
#define CH_REQUEST	(1<<3) //WM_IME_REQUESTメッセージ
#define CH_IMEMSG	(1<<4) //上記以外のimeメッセージ
#define CH_CANNA	(1<<5) //cannaの関数
#define CH_XIM		(1<<6) //XIM
#define CH_GTK		(1<<7) //Gtk-im
#define CH_QT		(1<<8)
#define CH_IBUS		(1<<9)
#define CH_WINMSG	(1<<10) //windows message全部
#define CH_TIME		(1<<11)
#define CH_MAXBIT	11

typedef enum{
    LOG_CRITICAL,
    LOG_IMPORTANT,
    LOG_MESSAGE,
    LOG_DEBUG,
} VerboseLevel;

#define LOG(ch,level,f) do{if(((ch)&DebugChannel) && (level)<=Verbose){f;}}while(0)
extern int DebugChannel;
void ParseChannelStr(const char* str0);
void ParseChannelEnv(int def_ch);

#ifdef __cplusplus
}
#endif


//(C) 2009 thomas
