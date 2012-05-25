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

#define IMCONTEXT_WIME(ins) G_TYPE_CHECK_INSTANCE_CAST(ins,RegisteredType,IMContextWime)
#define IMWIME_GET_CLASS(ins) G_TYPE_INSTANCE_GET_CLASS(ins,RegisteredType,IMContextWimeClass)
#define IMCONTEXTWIMECLASS(cl) G_TYPE_CHECK_CLASS_CAST(cl,RegisteredType,IMContextWimeClass)

#define ENABLE_IME	0x0001

#if GTK_MAJOR_VERSION>=3
  #define GDK_WINDOW_GET_GEOMETRY(win,x,y,w,h,d) gdk_window_get_geometry(win,x,y,w,h)
  #define XDISPLAY gdk_x11_get_default_xdisplay
  #define GDK_DRAWABLE_XID(x) (x!=NULL ? (unsigned)GDK_WINDOW_XID(x) : 0)
  #define IMDOMAIN "gtk30"
#else
  #define XDISPLAY GDK_DISPLAY
  #define GDK_WINDOW_GET_GEOMETRY gdk_window_get_geometry
  #define IMDOMAIN "gtk20"
#endif

#endif
