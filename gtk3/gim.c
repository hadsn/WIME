// -*- coding:euc-jp -*-
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include "so/xres.h"
#include "gim.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "lib/log.h"
#include "lib/printf.h"
#include "lib/cmdlineopt.h"
#include <ctype.h>

static GType RegisteredType;
static ToggleKey* ToggleKeys;

static const char SigCommit[] = "commit";
static const char SigPeStart[] = "preedit-start";
static const char SigPeEnd[] = "preedit-end";
static const char SigPeChanged[] = "preedit-changed";

ATImeCol ImeColor[ATIMECOMPCOL_ITEMMAX];

static void replace_context(GtkIMContext* context)
{
    IMContextWime* wi = IMCONTEXT_WIME(context);
    if(wi->ServerLevel!=RestartServerCount || wi->WimeCxn<0){
	//このコンテキストはサーバー再起動前のものと思われる。
	int old = wi->WimeCxn;
	wi->WimeCxn = CannaCreateContext();
	wi->ServerLevel = RestartServerCount;
	WimeShowToolbar(wi->WimeCxn,TRUE,FALSE);
	WimeShowCandWin(wi->WimeCxn,TRUE);
	WimeRegXWindow(wi->WimeCxn,GDK_DRAWABLE_XID(wi->Client));
	WimeMoveShadowWin(wi->WimeCxn,wi->Geom.x,wi->Geom.y,wi->Geom.width,wi->Geom.height);
	WimeSetCandWin(wi->WimeCxn,WIME_POS_POINT,wi->CandPos.x,wi->CandPos.y);
	DEBUGLOG(CH_GTK,"wime context old %d --> new %d\n",old,wi->WimeCxn);
    }
}

static gboolean ascii_mode(IMContextWime* wi,int keyval,int state)
{
    gboolean st = FALSE;

    DEBUGLOG(CH_GTK,"keyval=%x state=%x\n",keyval,state);

    //修飾キー単体、修飾キーが押されているときはimでの処理はしない
    gunichar ukey = gdk_keyval_to_unicode(keyval);
    if((state & ~(ShiftMask|LockMask|Mod2Mask))==0 && !IsModifierKey(keyval) && isprint(ukey)){
	gchar buf[7]={0};
	g_unichar_to_utf8(ukey,buf);
	g_signal_emit_by_name(wi,SigCommit,buf);
	st = TRUE;
	DEBUGLOG(CH_GTK,"commit %x\n",(unsigned)ukey);
    }
    return st;
}

static char* commit(IMContextWime* wi,char* u)
{
    free(wi->PreeditStr);
    wi->PreeditStr = NULL;
    g_signal_emit_by_name(wi,SigPeChanged); //前編集文字列を消す
    g_signal_emit_by_name(wi,SigPeEnd);
    g_signal_emit_by_name(wi,SigCommit,u);
    return u;
}

char* gwime_get_surrounding(int* cursor_pos,void* arg)
{
    gchar* sur;
    gint cursor;
    char* ans=NULL;
    IMContextWime* wi = (typeof(wi))arg;
    if(GTK_IM_CONTEXT_GET_SURROUNDING(GTK_IM_CONTEXT(wi),&sur,&cursor)){
	*cursor_pos = g_utf8_strlen(sur,cursor); //バイトオフセット→文字単位
	ans = strdup(sur);
	g_free(sur);
    }
    return ans;
}

void gwime_del_surrounding(int pos,int len,void* arg)
{
    IMContextWime* wi = (typeof(wi))arg;
    gtk_im_context_delete_surrounding(GTK_IM_CONTEXT(wi),pos,len);
}

void gwime_preedit(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    IMContextWime* wi = (typeof(wi))arg;
    wi->StrInfo = *si;
    if(!wi->PreeditStr){
	DEBUGLOG(CH_GTK,"emit preedit start\n");
	g_signal_emit_by_name(wi,SigPeStart);
    }
    if(*u8 != 0){
	free(wi->PreeditStr);
	wi->PreeditStr = strdup(u8);
	g_signal_emit_by_name(wi,SigPeChanged);
	DEBUGLOG(CH_GTK,"preedit string='%U'\n",u8);
    }else{
	commit(wi,(char[]){0}); //""を渡す。
	DEBUGLOG(CH_GTK,"erase preedit string\n");
    }
}

void gwime_convert(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    IMContextWime* wi = (typeof(wi))arg;
    free(wi->PreeditStr);
    wi->PreeditStr = strdup(u8);
    wi->StrInfo = *si;
    g_signal_emit_by_name(wi,SigPeChanged);
    DEBUGLOG(CH_GTK,"preedit string='%U'\n",u8);
}

void gwime_commit(const char* u8,const char* composition,const WimeCompStrInfo* si,void* arg)
{
    IMContextWime* wi = (typeof(wi))arg;
    free(commit(wi,strdup(u8)));
    if(composition != NULL){
	gwime_preedit(composition,si,arg);
    }
    DEBUGLOG(CH_GTK,"commit '%U'\n",u8);
}

/*
  keyをimが処理すればTRUEを返す
*/
gboolean imwime_filter_keypress(GtkIMContext* context,GDKEVENTKEY* ev)
{
    IMContextWime* wi = IMCONTEXT_WIME(context);
    //IMContextWimeClass* wc = IMWIME_GET_CLASS(wi);

    if(GDKEVENTKEY_GETTYPE(ev) == GDK_KEY_RELEASE)
	return FALSE;
    
    replace_context(context);
    if(!WimeIsConnected())
	return ascii_mode(wi,GDKEVENTKEY_GETVAL(ev),GDKEVENTKEY_GETSTATE(ev));

    DEBUGLOG(CH_GTK,"code 0x%hx, sym 0x%x, state 0x%x, group %hhd, string:%U\n",GDKEVENTKEY_GETCODE(ev),GDKEVENTKEY_GETVAL(ev),GDKEVENTKEY_GETSTATE(ev),GDKEVENTKEY_GETGROUP(ev),GDKEVENTKEY_GETSTRING(ev));

    gboolean st = WimeFilterKey(wi->WimeCxn,ToggleKeys,XDISPLAY,GDKEVENTKEY_GETCODE(ev),GDKEVENTKEY_GETVAL(ev),GDKEVENTKEY_GETSTATE(ev),wi);
    if(!st)
	st = ascii_mode(wi,GDKEVENTKEY_GETVAL(ev),GDKEVENTKEY_GETSTATE(ev));
    return st;
}

//utf8での文字オフセットをバイトオフセットにする
static int offset_char_to_byte(const char* u8,int char_offset)
{
    return g_utf8_offset_to_pointer(u8,char_offset) - u8;
}

void add_attr_color(PangoAttrList* attrs,int start,int end,int colindex)
{
    int col=ImeColor[colindex].Text;
    PangoAttribute* at = pango_attr_foreground_new(GETR16(col),GETG16(col),GETB16(col));
    at->start_index = start;
    at->end_index = end;
    pango_attr_list_insert(attrs,at);

    col=ImeColor[colindex].Back;
    at = pango_attr_background_new(GETR16(col),GETG16(col),GETB16(col));
    at->start_index = start;
    at->end_index = end;
    pango_attr_list_insert(attrs,at);

    if(ImeColor[colindex].UnderLine){
	at = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
	at->start_index = start;
	at->end_index = end;
	pango_attr_list_insert(attrs,at);
    }
    DEBUGLOG(CH_GTK,"pos %d-%d, color %d 0x%x/0x%x-%d\n",start,end,colindex,ImeColor[colindex].Text,ImeColor[colindex].Back,ImeColor[colindex].UnderLine);
}

//開始時にすぐ呼ばれる
void imwime_get_preedit_str(GtkIMContext* context,gchar** str,PangoAttrList** attrs,gint* cursor_pos)
{
    IMContextWime* wi = IMCONTEXT_WIME(context);
    gint cursor_dum;

    if(cursor_pos == NULL)
	cursor_pos = &cursor_dum;

    if(attrs != NULL)
	*attrs = pango_attr_list_new(); //これもないとだめ
    if(wi->PreeditStr == NULL){
	if(str != NULL)
	    *str = g_strdup("");
	*cursor_pos = 0;
	DEBUGLOG(CH_GTK,"str=\"\", cursor_pos=0\n");
	return;
    }

    if(str != NULL){
	*str = g_strdup(wi->PreeditStr);
    }
    if(attrs != NULL){
	add_attr_color(*attrs,0,strlen(*str),ATCOLINDEX_INPUT);
	if(wi->StrInfo.TargetClause>=0){ //注目文節がある
	    int cl_start = offset_char_to_byte(wi->PreeditStr,wi->StrInfo.TargetClause);
	    int cl_end = offset_char_to_byte(wi->PreeditStr,wi->StrInfo.TargetClause+wi->StrInfo.TargetClLen);
	    add_attr_color(*attrs,cl_start,cl_end,ATCOLINDEX_TARGETCONVERT);
	}
    }

    //カーソル位置はバイトではなく文字単位 [r18]入力中のカーソル移動ができなかった。
    *cursor_pos = wi->StrInfo.CursorPos;
    DEBUGLOG(CH_GTK,"str=%U, cursor_pos=%d\n",str?*str:"<str is NULL>",*cursor_pos);
}

//ウィンドウの移動／大きさ変更でもこれが呼ばれるようだ
//area:カーソルの場所と大きさ(クライアントウィンドウの相対位置)
void imwime_set_cursor_loc(GtkIMContext* context,GdkRectangle* area)
{
    replace_context(context);
    IMContextWime* wi = IMCONTEXT_WIME(context);
    if(wi->Client != NULL){
	gint dum;
	GdkRectangle geom;
	GDK_WINDOW_GET_GEOMETRY(wi->Client,&dum,&dum,&geom.width,&geom.height,&dum);
	gdk_window_get_origin(wi->Client,&geom.x,&geom.y);
	if(!gdk_rectangle_equal(&wi->Geom,&geom)){
	    //ウィンドウが動いたとき
	    wi->Geom = geom;
	    WimeMoveShadowWin(wi->WimeCxn,geom.x,geom.y,geom.width,geom.height);
	    DEBUGLOG(CH_GTK,"shadow window (%d,%d) %dx%d\n",geom.x,geom.y,geom.width,geom.height);
	    WimeSetCompWin(wi->WimeCxn,WIME_POS_POINT,area->x,area->y); //use_preedit()用
	    wi->CandPos.x=-1; //候補ウィンドウの位置も更新させる
	}
	if(wi->PreeditStr!=NULL || !gdk_rectangle_equal(&wi->CandPos,area)){
	    wi->CandPos = *area;
	    int cs_y_global = geom.y+area->y; //キャレットのy座標(絶対位置)
	    const int updown_limit=300;
	    if(SCREEN_HEIGHT(wi->Client)-cs_y_global < updown_limit){
		/*キャレットの位置が下がって候補ウィンドウが入力位置に重なるようなら候補ウィンドウを
		  入力行の上に表示させたい。しかしどうすればいいか？ 候補ウィンドウの高さは分からないので
		  候補ウィンドウの座標は設定できない。CFS_EXCLUDEで入力行をrcAreaに指定してもうまく
		  動作しない。いい方法が分かるまでは、画面下からupdown_limitドット以下になったら
		  次のように設定する。ましにはなるがまだ希望通りの動作にはならない。
		 */
		WimeSetCandWin(wi->WimeCxn,WIME_POS_EXCLUDE,area->x,area->y,
			       area->x,area->y-area->height,1,area->height);
	    }else{
		//キャレットの下に候補ウィンドウを表示させる。
		WimeSetCandWin(wi->WimeCxn,WIME_POS_POINT,area->x,area->y+area->height);
	    }
	}
    }
}

void imwime_set_client_window(GtkIMContext* context,CLIENT_TYPE* window)
{
    replace_context(context);

    IMContextWime* wi = IMCONTEXT_WIME(context);
    wi->Client = window;
    DEBUGLOG(CH_GTK,"cxn=%d gdkwin=%p xwin=0x%lx\n",wi->WimeCxn,window,(unsigned long)GDK_DRAWABLE_XID(window));
    WimeRegXWindow(wi->WimeCxn,GDK_DRAWABLE_XID(window));
}

void imwime_set_focus(GtkIMContext* context,gboolean state,const char* msg)
{
    replace_context(context);

    IMContextWime* wi = IMCONTEXT_WIME(context);
    DEBUGLOG(CH_GTK,"cxn=%d focus %s.\n",wi->WimeCxn,msg);
    WimeSetFocus(wi->WimeCxn,state);
}

void imwime_focus_in(GtkIMContext* context)
{
    /*[r200]変換をキャンセルするとステータスウィンドウが出なくなるバグの対処。
      (ついでに、先にこちらを呼んでおくと、最初のフォーカス移動でステータスウィンドウが表示される。)
     */
    IMContextWime* wi = IMCONTEXT_WIME(context);
    WimeShowToolbar(wi->WimeCxn,TRUE,FALSE);

    WimeEnableIme(wi->WimeCxn,wi->EnableIme); //[r242]
    
    imwime_set_focus(context,TRUE,"in");
}

void imwime_focus_out(GtkIMContext* context)
{
    /*[r242]漢字onのまま別の漢字offのウィンドウに移るとステータスウィンドウがonのままになっている。
      focus outの時に現在の状態を保存しておき、inのときに状態を元に戻す。
      ime状態の判断箇所が複数あるのは良くないのでwi->EnableImeはここでしか使わないことにする。
      このコンテキストがime offで、ime onの場所(ステータスウィンドウon)からfocus inした場合、WimeEnableIme(false)を呼んでもコンテキストがもともとoffなのでステータスウィンドウが変化しない。offの状態にするにはいったんonにして状態を変化させ、そしてoffにしなければならない。
      a. focus inのときにWimeEnableImeを３回呼んでime状態を変化させる。
		WimeEnableIme(st);WimeEnableIme(!st);WimeEnableIme(st);
      b. focus outのときに状態を保存して常にoffにし、focus inのときに状態を戻す。
      bの方法で行うことにする。
     */
    IMContextWime* wi = IMCONTEXT_WIME(context);
    wi->EnableIme = WimeEnableIme(wi->WimeCxn,IME_QUERY);
    WimeEnableIme(wi->WimeCxn,false);
    if(wi->PreeditStr != NULL)
	commit(wi,(char[]){0}); //前編集文字列があれば消去する。

    imwime_set_focus(context,FALSE,"out");
}

void imwime_finalize(GObject* o)
{
    replace_context(GTK_IM_CONTEXT(o));

    IMContextWime* wi = IMCONTEXT_WIME(o);
    DEBUGLOG(CH_GTK,"finalize:cxn %d\n",wi->WimeCxn);
    WimeEnableIme(wi->WimeCxn,FALSE);
    WimeShowToolbar(wi->WimeCxn,FALSE,FALSE);
    CannaCloseContext(wi->WimeCxn);
    IMWIME_GET_CLASS(o)->FinalizeOrig(o);

    /* どうやればim_module_exit()が呼ばれるのか分からないので、とりあえず
       コンテキストが全部閉じられたら(グローバルコンテキストが１つ残った状態)接続を終えることにする。 */
    if(WimeOpenedContext() == 1){
	DEBUGLOG(CH_GTK,"all context closed. wime finalize\n");
	WimeFinalize();
    }
}

/*
  TreeViewのインライン入力では,入力を完了させるたびにdisposeが呼ばれるみたい。
*/
void imwime_init(GtkIMContext* cx)
{
    /* imwime_finalize()で接続が閉じられた後再度コンテキスト作成になったらもう一度接続し直す。*/
    if(!WimeIsConnected()){
	DEBUGLOG(CH_GTK,"wime disconnected. try connect\n");
	WimeInitialize(ParseEnv(CH_GLOBAL|CH_GTK),'g');
    }
    
    IMContextWime* wi = IMCONTEXT_WIME(cx);

    memset(&(wi->Parent)+1,0,sizeof(*wi)-sizeof(wi->Parent)); //IMContextWimeだけのメンバを0クリア
    wi->WimeCxn = CannaCreateContext();
    wi->ServerLevel = RestartServerCount;
    WimeShowToolbar(wi->WimeCxn,TRUE,FALSE);
    WimeShowCandWin(wi->WimeCxn,TRUE);
    DEBUGLOG(CH_GTK,"wime context %d\n",wi->WimeCxn);
}

void imwime_reset(GtkIMContext* context)
{
    IMContextWime* wi = IMCONTEXT_WIME(context);
    char* comp = WimeGetCompStr(wi->WimeCxn,NULL);
    if(comp != NULL){
	DEBUGLOG(CH_GTK,"commit preedit string:%U\n",comp);
	free(commit(wi,comp)); //compを解放
	CannaEndConvert(wi->WimeCxn,0,0,NULL);
    }
}

#if 0
void imwime_set_use_preedit(GtkIMContext* context,gboolean u)
{
    IMContextWime* wi = IMCONTEXT_WIME(context);
    u=!u;
    WimeShowToolbar(wi->WimeCxn,TRUE,u);
    DEBUGLOG(CH_GTK,"set_use_preedit:%d\n",u);
}
#endif

////////////////////////////////////////////////////////

void wime_initialize();

void imwime_class_init(GtkIMContextClass* cl)
{
    cl->filter_keypress =  imwime_filter_keypress;
    cl->get_preedit_string = imwime_get_preedit_str;
    cl->set_cursor_location = imwime_set_cursor_loc;
    cl->SET_CLIENT_WINDOW = imwime_set_client_window;
    cl->focus_in = imwime_focus_in;
    cl->focus_out = imwime_focus_out;
    cl->reset = imwime_reset;
#if 0
    cl->set_use_preedit = imwime_set_use_preedit;
#endif

    GObjectClass* o = G_OBJECT_CLASS(cl);
    IMCONTEXTWIMECLASS(cl)->FinalizeOrig = o->finalize;
    o->finalize = imwime_finalize;

    WimePreedit = gwime_preedit;
    WimeConvert = gwime_convert;
    WimeCommit = gwime_commit;
    WimeGetSurrounding = gwime_get_surrounding;
    WimeDelSurrounding = gwime_del_surrounding;

#if GTK_MAJOR_VERSION >= 4
    //gtk4はここで接続するようにしてみる。
    if(!WimeIsConnected())
	wime_initialize();
#endif
    
    DEBUGLOG(CH_GTK,IMDOMAIN "\n");
}

void imwime_class_fin(GtkIMContextClass* cl UNUSED)
{
    DEBUGLOG(CH_GTK,IMDOMAIN "\n");
}

////////////////////////////////////////////////////////
//モジュールがexportする関数

const char ContextId[] = "wime";
const char ContextName[] = "Wime";
const char DomainName[] = IMDOMAIN;
const char RegisterName[] = "IMContextWime";

static void catch_restart_signal(void)
{
    ++RestartServerCount;
    DEBUGLOG(CH_GTK,"count %d\n",RestartServerCount);
    WimeGetColor(0,ImeColor);
}

void wime_initialize()
{
    WimeInitialize(ParseEnv(CH_GLOBAL|CH_GTK),'g');
    InitDatabase(NULL,"gim");
    ToggleKeys = GetConvKeyFromResource(XDISPLAY);
    WimeRestartSignal(catch_restart_signal);
    WimeGetColor(0,ImeColor);
    DEBUGLOG(CH_GTK,IMDOMAIN "\n");
}

void im_module_init(GTypeModule* module)
{
    GTypeInfo info = {
	.class_size = sizeof(IMContextWimeClass),
	.base_init = NULL,
	.base_finalize = NULL,

	.class_init = (GClassInitFunc)imwime_class_init,
	.class_finalize = (GClassFinalizeFunc)imwime_class_fin,
	.class_data = NULL,

	.instance_size = sizeof(IMContextWime),
	.n_preallocs = 0,
	.instance_init = (typeof(info.instance_init))imwime_init,

	.value_table = NULL
    };

    CustomPrintf();
    RegisteredType = g_type_module_register_type(module,GTK_TYPE_IM_CONTEXT,RegisterName,&info,0);
#if GTK_MAJOR_VERSION < 4
    /* gtk4はvoid g_io_module_loadの時点でXに接続していないようなので、imwime_class_init()で
       wimeに接続するようにしてみる。*/
    wime_initialize();
#endif
}

void im_module_exit(void)
{
    DEBUGLOG(CH_GTK,IMDOMAIN "\n");
    WimeFinalize();
    free(ToggleKeys);
}

#if GTK_MAJOR_VERSION <= 3

GtkIMContextInfo ImwimeInfo = {
    .context_id = ContextId,
    .context_name = ContextName,
    .domain = DomainName,
    .domain_dirname = LOCALEDIR,
    .default_locales = "*"
};

GtkIMContextInfo *ImcInfoList[] = {
    &ImwimeInfo
};

void im_module_list(GtkIMContextInfo*** contexts,int* n_contexts)
{
    *contexts = ImcInfoList;
    *n_contexts = G_N_ELEMENTS(ImcInfoList);
}

GtkIMContext* im_module_create(const gchar* context_id)
{
    return strcmp(context_id,ContextId)==0 ? GTK_IM_CONTEXT(g_object_new(RegisteredType,NULL)) : NULL;
}

#endif

#if GTK_MAJOR_VERSION >= 4
void g_io_module_load(GIOModule* module)
{
    g_type_module_use(G_TYPE_MODULE(module));
    im_module_init(G_TYPE_MODULE(module));
    g_io_extension_point_implement(GTK_IM_MODULE_EXTENSION_POINT_NAME,
				   RegisteredType,
				   "wime",
				   50);
}

void g_io_module_unload(GIOModule* module)
{
    im_module_exit();
    g_type_module_unuse(G_TYPE_MODULE(module));
}

char** g_io_module_query(void)
{
    char *ext_name[]={GTK_IM_MODULE_EXTENSION_POINT_NAME,NULL};
    return g_strdupv(ext_name);
}
#endif

//(C) 2009 thomas
