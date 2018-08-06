#pragma once

//XkbKeycodeToKeysym
#include <X11/XKBlib.h>

#ifdef __cplusplus
extern "C"{
#endif

unsigned ConvToVk(KeySym ks,unsigned state);

//#define XKEYCODETOKEYSYM XKeycodeToKeysym
#define XKEYCODETOKEYSYM(d,k,i) XkbKeycodeToKeysym(d,k,0,i)

#ifdef __cplusplus
}
#endif

//(C) 2017 thomas
