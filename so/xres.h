#ifndef WIME_SO_XRES
#define WIME_SO_XRES

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

extern const char XResConvKey[];
extern const char XResDefFont[];
extern const char XResDisableSty[];

typedef struct{
    unsigned Key,Mod;
} ToggleKey;

#define MODESWITCHMASK (1<<13)

void InitDatabase(Display* disp,const char* postfix);
const char* GetResource(Display* disp,const char* res);
ToggleKey* GetConvKeyFromResource(Display* disp);
bool IsToggleKey(const ToggleKey* keylist,unsigned key,unsigned mod);
char* GetCompFont(Display* disp);
KeySym KeycodeToKeysym(Display* disp,KeyCode kc,unsigned state,int shiftlevel);

#ifdef __cplusplus
}
#endif

#endif
