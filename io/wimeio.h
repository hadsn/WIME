#ifndef WIME_IO_WIMEIO
#define WIME_IO_WIMEIO

#include <stdbool.h>

enum{
    IM_ST_NO_ERROR
};

int ImInit(unsigned socket_num,int use_tcp);
int ImSelect(void);
int ImRead(void* buf,int len);
bool ImWrite(const void* buf,int len);
int ImDisconnect(void);
int ImReadSetting(void* globaldata);
int ImCloseAll(void);
void ImAuxInput(unsigned xw);
void ImSemStart(void);

typedef int (*PROT_INIT)(unsigned,int);
typedef int (*PROT_RD)(void*,int);
typedef bool (*PROT_WR)(const void*,int);
typedef void (*PROT_READSETTING)(void*);
typedef int (*PROT_SEL)(void);

#endif

//(C) 2009 thomas
