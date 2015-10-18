// -*- coding:euc-jp -*-
#ifndef WIME_LIB_LOG
#define WIME_LIB_LOG

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

extern int Verbose;
extern char LogMark;

#define VERBOSE(x) do{if(Verbose){x;}}while(0)

bool Msg(char mark,const char* fmt,...); //これをどこかで実装する必要がある

#define MSG(fmt,...) Msg(LogMark,"%s:" fmt,__func__,## __VA_ARGS__)
#define LOG(fmt,...) VERBOSE(MSG(fmt,## __VA_ARGS__))
#define ERR(fmt,...) do{MSG(fmt,## __VA_ARGS__);fprintf(stderr,"[%c]%s:%d:" fmt,LogMark,__func__,__LINE__,## __VA_ARGS__);}while(0)

#define SERVER_MARK 'w'

#ifdef __cplusplus
}
#endif

#endif

//(C) 2009 thomas
