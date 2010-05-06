#include <stdarg.h>
#include <stdio.h>
#include "wimelog.h"
#include "lib/array.h"
#include "corr.h"

extern int Fd; //wimeapi.c
extern int ActiveFd;

static Array MsgBuf;

__attribute__((constructor))
void wime_log_init(void)
{
    ArNew(&MsgBuf,1,NULL);
}

//wimeに出力できたときtrue
bool WimeLogV(char mark,const char* fmt,va_list vl)
{
    char code;
    bool st=false;

    //wime.cのlog_req()で処理されている
    if(Fd!=-1){
	ArPrintV(ArClear(&MsgBuf),fmt,vl);
	st=(Snd15(Fd,WIME_LOG,mark,0,ArAdr(&MsgBuf)) && Rcv2(Fd,&code) && code);
    }
    return st;
}

bool Msg(char mark,const char* fmt,...)
{
    va_list vl;
    bool st;

    va_start(vl,fmt);
    st=WimeLogV(mark,fmt,vl);
    va_end(vl);
    return st;
}
