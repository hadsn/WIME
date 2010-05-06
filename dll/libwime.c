/*
  linux側soを呼びだすためのwin側dll。
*/

#include <windef.h>
#include <stdio.h>
#include "io/wimeio.h"
#include "so/wimelog.h"
#include "so/wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"

BOOL DllMain(HINSTANCE dllhandle UNUSED,DWORD reason UNUSED,void* dum UNUSED)
{
#if 0
    const char *m;

    switch(reason){
    case DLL_PROCESS_ATTACH:
	m="DLL_PROCESS_ATTACH";
	break;
    case DLL_THREAD_ATTACH:
	m="DLL_THREAD_ATTACH";
	break;
    case DLL_THREAD_DETACH:
	m="DLL_THREAD_DETACH";
	break;
    case DLL_PROCESS_DETACH:
	m="DLL_PROCESS_DETACH";
	break;
    default:
	m="something";
    }
    printf("handle=%p,reason=%s\n",dllhandle,m);
#endif
    return TRUE;
}
    
int ProxyImInit(int socket_num)
{
    return ImInit(socket_num);
}

int ProxyImSelect(void)
{
    return ImSelect();
}

int ProxyImRead(void* buf,int len)
{
    return ImRead(buf,len);
}

bool ProxyImWrite(const void* buf,int len)
{
    return ImWrite(buf,len);
}

int ProxyImDisconnect(void)
{
    return ImDisconnect();
}

int ProxyImReadSetting(void* gd)
{
    return ImReadSetting(gd);
}

int ProxyImCloseAll(void)
{
    return ImCloseAll();
}

bool ProxyWimeConnect(void)
{
    return WimeConnect();
}
void ProxyWimeDisconnect(void)
{
    WimeDisconnect();
}

void ProxyWimeShmInit(int logmark)
{
    WimeShmInit(logmark);
}

void ProxyWimeShmFin(void)
{
    WimeShmFin();
}

void ProxyImAuxInput(unsigned xw)
{
    ImAuxInput(xw);
}

void ProxyWimeSemStart(void)
{
    WimeSemStart();
}
