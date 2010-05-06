#ifndef WIME_GIM_GIM
#define WIME_GIM_GIM

#include "so/wimeapi.h"
#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>

typedef struct{
    GtkIMContext Parent;
    int Flag;
    int WimeCxn;
    gchar *PreeditStr; //utf8
    WimeCompStrInfo StrInfo;
    GdkWindow* Client;
    GdkRectangle Geom,CandPos;
} IMContextWime;

typedef struct{
    GtkIMContextClass Parent;
    void (*FinalizeOrig)(GObject*);
} IMContextWimeClass;

#define IMCONTEXT_WIME(ins) GTK_CHECK_CAST(ins,RegisteredType,IMContextWime)
#define IMWIME_GET_CLASS(ins) G_TYPE_INSTANCE_GET_CLASS(ins,RegisteredType,IMContextWimeClass)
#define IMCONTEXTWIMECLASS(cl) G_TYPE_CHECK_CLASS_CAST(cl,RegisteredType,IMContextWimeClass)

#define ENABLE_IME	0x0001

#endif
