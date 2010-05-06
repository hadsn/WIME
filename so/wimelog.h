#ifndef WIME_SO_WIMELOG
#define WIME_SO_WIMELOG

#include <stdarg.h>
#include <stdbool.h>
#include "lib/log.h"

#ifdef __cplusplus
extern "C"{
#endif

#define WimeLog Msg
bool WimeLogV(char mark,const char* fmt,va_list vl);

#ifdef __cplusplus
}
#endif

#endif
