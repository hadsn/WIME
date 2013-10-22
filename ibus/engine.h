#ifndef __ENGINE_H__
#define __ENGINE_H__

#include <ibus.h>
#include <X11/Xlib.h>

extern Display* Disp;

typedef struct{
    IBusEngine parent;

    IBusLookupTable* CandTable;
    int WimeCxn;
    int Flags;
    int TargetNum; //注目文節番号
    int ServerLevel;
} IBusWimeEngine;

typedef struct  {
    IBusEngineClass parent;
} IBusWimeEngineClass;

#define IBUS_TYPE_WIME_ENGINE (ibus_wime_engine_get_type())
GType ibus_wime_engine_get_type(void);

extern int Flags;
#define USE_IBUS_CANDIDATE_WINDOW 1

#endif

//(C) 2012 thomas
