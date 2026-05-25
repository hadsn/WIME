#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    enum {
        IM_ST_NO_ERROR
    };

    int ImInit(int socket_num, int use_tcp);
    int ImSelect(void);
    int ImRead(void* buf, int len);
    bool ImWrite(const void* buf, int len);
    int ImDisconnect(void);
    int ImReadSetting(void* globaldata);
    int ImCloseAll(void);
    void ImAuxInput(unsigned xw);
    void ImSemStart(int socket_num);
    void ImSemUnlink(int socket_num);

    typedef int (*PROT_INIT)(unsigned, int);
    typedef int (*PROT_RD)(void*, int);
    typedef bool (*PROT_WR)(const void*, int);
    typedef void (*PROT_READSETTING)(void*);
    typedef int (*PROT_SEL)(void);

#ifdef __cplusplus
}
#endif

//(C) 2009 thomas
