
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    //これをどこかで実装する必要がある
    bool Msg(char mark, const char* fmt, ...)
#ifndef __clang__
        __attribute__((format(gnu_printf, 2, 3)))
#endif
        ;

    extern int Verbose;
    extern char LogMark;
    extern int DebugChannel;

#define MESG(fmt,...) Msg(LogMark,"%s:" fmt,__func__,## __VA_ARGS__)
#define ERR(fmt,...) do{MESG(fmt,## __VA_ARGS__);fprintf(stdout,"[%c]%s:%d:" fmt,LogMark,__func__,__LINE__,## __VA_ARGS__); fflush(stdout);}while(0)


#define CH_GLOBAL	(1<<0) //一般情報
#define CH_COMPOSITION	(1<<1) //WM_IME_COMPOSITIONメッセージ
#define CH_NOTIFY	(1<<2) //WM_IME_NOTIFYメッセージ
#define CH_REQUEST	(1<<3) //WM_IME_REQUESTメッセージ
#define CH_IMEMSG	(1<<4) //上記以外のimeメッセージ
#define CH_CANNA	(1<<5) //cannaの関数
#define CH_XIM		(1<<6) //XIM
#define CH_GTK		(1<<7) //Gtk-im
#define CH_QT		(1<<8) //qt
#define CH_WINMSG	(1<<10) //その他のwindows message
#define CH_TIME		(1<<11) //経過時間
#define CH_COMPO_IMC	(1<<12) //WM_IME_COMPOSITIONのIMC
#define CH_NOTI_IMC	(1<<13) //WM_IME_NOTIFYのIMC
#define CH_REQ_IMC	(1<<14) //WM_IME_REQUESTのIMC
#define CH_MAXBIT	14

    typedef enum {
        LOG_FATAL,
        LOG_ERROR,
        /* WARNING, */
        LOG_INFO,
        LOG_DEBUG,
        LOG_MAX
    } VerboseLevel;

#define LOG(ch,level,f) do{if(((ch)&DebugChannel) && (level)<=Verbose){f;}}while(0)

#define FATALLOG(ch,fmt,...)	LOG(ch,LOG_FATAL,MESG(fmt,## __VA_ARGS__))
#define FATALDO(ch,func)	LOG(ch,LOG_FATAL,func)
#define ERRORLOG(ch,fmt,...)	LOG(ch,LOG_ERROR,MESG(fmt,## __VA_ARGS__))
#define ERRORDO(ch,func)	LOG(ch,LOG_ERROR,func)
#define INFOLOG(ch,fmt,...)	LOG(ch,LOG_INFO,MESG(fmt,## __VA_ARGS__))
#define INFODO(ch,func)		LOG(ch,LOG_INFO,func)
#define DEBUGLOG(ch,fmt,...)	LOG(ch,LOG_DEBUG,MESG(fmt,## __VA_ARGS__))
#define DEBUGDO(ch,func)	LOG(ch,LOG_DEBUG,func)

#ifdef __cplusplus
}
#endif


//(C) 2009 thomas
