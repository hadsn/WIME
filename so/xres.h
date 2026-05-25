#pragma once

/* wimeapi.hでこの定義をチェックしているので残しておく。*/
#ifndef WIME_SO_XRES
#define WIME_SO_XRES
#endif

#include <stdbool.h>
#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

    extern const char XResConvKey[];
    extern const char XResDefFont[];
    extern const char XResDisableSty[];

    typedef enum {
        IMESTATUS_NO_TOGGLE, IMESTATUS_TOGGLE, IMESTATUS_ON, IMESTATUS_OFF
    } ImeStateKeyType;
    typedef struct {
        ImeStateKeyType Type;
        unsigned Key, Mod;
    } ToggleKey;

#define MODESWITCHMASK (1<<13)

    void InitDatabase(Display* disp, const char* postfix);
    const char* GetResource(Display* disp, const char* res);
    ToggleKey* GetConvKeyFromResource(Display* disp);
    ImeStateKeyType IsToggleKey(const ToggleKey* keylist, unsigned key, unsigned mod);
    char* GetCompFont(Display* disp);
    KeySym KeycodeToKeysym(Display* disp, KeyCode kc, unsigned state, int shiftlevel);

#ifdef __cplusplus
}
#endif

//(C) 2009 thomas
