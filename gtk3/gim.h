#ifndef WIME_GIM_GIM
#define WIME_GIM_GIM

#include "so/wimeapi.h"
#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>
//#include <gdk/gdkkeysyms.h>

#if GTK_MAJOR_VERSION>=4
#include <gdk/x11/gdkx.h>
#define XDISPLAY GDK_DISPLAY_XDISPLAY(gdk_display_get_default())
#define GdkWindow GdkSurface
#define GDK_DRAWABLE_XID(wdj) (wdj!=NULL ? (unsigned)GDK_SURFACE_XID(gtk_native_get_surface(gtk_widget_get_native(wdj))) : 0)
#define IMDOMAIN "gtk40"
#define GDKEVENTKEY GdkEvent
#define GDKEVENTKEY_GETTYPE(ev) gdk_event_get_event_type(ev)
#define GDKEVENTKEY_GETVAL(ev) gdk_key_event_get_keyval(ev)
#define GDKEVENTKEY_GETSTATE(ev) gdk_event_get_modifier_state(ev)
#define GDKEVENTKEY_GETCODE(ev) gdk_key_event_get_keycode(ev)
#define GDKEVENTKEY_GETGROUP(ev) gdk_key_event_get_level(ev)
#define GDKEVENTKEY_GETSTRING(ev) gdk_keyval_name(gdk_key_event_get_keyval(ev))
#define CLIENT_TYPE GtkWidget
inline gboolean GTK_IM_CONTEXT_GET_SURROUNDING(GtkIMContext* c, char** text, int* cursor) {
    int anchor;
    return gtk_im_context_get_surrounding_with_selection(c, text, cursor, &anchor);
}
inline void GDK_WINDOW_GET_GEOMETRY(CLIENT_TYPE* win, int* dum_x, int* dum_y, int* w, int* h, int* dum) {
    *w = gtk_widget_get_width(win); *h = gtk_widget_get_height(win);
}
inline GdkSurface* get_surface(GtkWidget* wdj) {
    return gtk_native_get_surface(gtk_widget_get_native(wdj));
}
inline void gdk_window_get_origin(GtkWidget* win, int* x, int* y) {
    Window chd;
    XTranslateCoordinates(XDISPLAY, gdk_x11_surface_get_xid(get_surface(win)), gdk_x11_display_get_xrootwindow(gdk_display_get_default()), 0, 0, x, y, &chd);
    double dx, dy;
    gtk_widget_translate_coordinates(win, GTK_WIDGET(gtk_widget_get_root(win)), 0, 0, &dx, &dy);
    *x += (int)dx; *y += (int)dy;
}
inline int SCREEN_HEIGHT(CLIENT_TYPE* win) {
    GdkRectangle rec;
    GdkMonitor* mon = gdk_display_get_monitor_at_surface(gdk_display_get_default(), get_surface(win));
    gdk_x11_monitor_get_workarea(mon, &rec);
    return rec.height;
}
#define SET_CLIENT_WINDOW set_client_widget
#else
#include <gdk/gdkx.h>
#define GDKEVENTKEY GdkEventKey
#define GDKEVENTKEY_GETTYPE(ev) ev->type
#define GDKEVENTKEY_GETVAL(ev) ev->keyval
#define GDKEVENTKEY_GETSTATE(ev) ev->state
#define GDKEVENTKEY_GETCODE(ev) ev->hardware_keycode
#define GDKEVENTKEY_GETGROUP(ev) ev->group
#define GDKEVENTKEY_GETSTRING(ev) ev->string
#define CLIENT_TYPE GdkWindow
#define GTK_IM_CONTEXT_GET_SURROUNDING gtk_im_context_get_surrounding
#define SET_CLIENT_WINDOW set_client_window
#endif

#if GTK_MAJOR_VERSION==3
#define GDK_WINDOW_GET_GEOMETRY(win,x,y,w,h,d) gdk_window_get_geometry(win,x,y,w,h)
#define XDISPLAY gdk_x11_get_default_xdisplay()
#define GDK_DRAWABLE_XID(x) (x!=NULL ? (unsigned)GDK_WINDOW_XID(x) : 0)
#define IMDOMAIN "gtk30"
inline int SCREEN_HEIGHT(GdkWindow* win) {
    GdkRectangle rec;
    GdkMonitor* mon = gdk_display_get_monitor_at_window(gdk_display_get_default(), win);
    gdk_monitor_get_workarea(mon, &rec);
    return rec.height;
}
#elif GTK_MAJOR_VERSION<=2
#define XDISPLAY GDK_DISPLAY()
#define GDK_WINDOW_GET_GEOMETRY gdk_window_get_geometry
#define IMDOMAIN "gtk20"
#define SCREEN_HEIGHT(dummy) gdk_screen_get_height(gdk_screen_get_default())
inline bool gdk_rectangle_equal(const GdkRectangle* a, const GdkRectangle* b) {
    return a->x == b->x && a->y == b->y && a->width == b->width && a->height == b->height;
}
#endif

typedef struct {
    GtkIMContext Parent;
    int Flag;
    int WimeCxn;
    gchar* PreeditStr; //utf8
    WimeCompStrInfo StrInfo;
    CLIENT_TYPE* Client;
    GdkRectangle Geom, CandPos;
    int ServerLevel;
} IMContextWime;

typedef struct {
    GtkIMContextClass Parent;
    void (*FinalizeOrig)(GObject*);
} IMContextWimeClass;

#define IMCONTEXT_WIME(ins) G_TYPE_CHECK_INSTANCE_CAST(ins,RegisteredType,IMContextWime)
#define IMWIME_GET_CLASS(ins) G_TYPE_INSTANCE_GET_CLASS(ins,RegisteredType,IMContextWimeClass)
#define IMCONTEXTWIMECLASS(cl) G_TYPE_CHECK_CLASS_CAST(cl,RegisteredType,IMContextWimeClass)

#define ENABLE_IME	(1<<0)

#endif

//(C) 2009 thomas
