#include <stdio.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <stdlib.h>
#include <X11/keysym.h>
#include "gim.h"
#include "so/winkey.h"
#include "so/xres.h"
#include "lib/ut.h"
#include "so/wimelog.h"

static GType RegisteredType;
static ToggleKey *ToggleKeys;

static const char SigCommit[] = "commit";
static const char SigPeStart[] = "preedit-start";
static const char SigPeEnd[] = "preedit-end";
static const char SigPeChanged[] = "preedit-changed";

static Array Cxns;

static gboolean ascii_mode(IMContextWime* wi,int keyval,int state)
{
    gboolean st = FALSE;

    LOG("\n");

    //修飾キー単体、修飾キーが押されているときはimでの処理はしない
    if((state & ~(ShiftMask|LockMask))==0 && !IsModifierKey(keyval)){
	gunichar ukey = gdk_keyval_to_unicode(keyval);
	if(ukey != 0){
	    gchar buf[7];
	    memset(buf,0,sizeof(buf));
	    g_unichar_to_utf8(ukey,buf);
	    g_signal_emit_by_name(wi,SigCommit,buf);
	    st = TRUE;
	    LOG("commit\n");
	}
    }
    return st;
}

static char* commit(IMContextWime* wi,char* u)
{
    wi->PreeditStr = NULL;
    g_signal_emit_by_name(wi,SigPeChanged); //前編集文字列を消す
    g_signal_emit_by_name(wi,SigPeEnd);
    g_signal_emit_by_name(wi,SigCommit,u);
    return u;
}

/*
  キーをwimeに送る。
*/
static bool send_key(IMContextWime* wi,unsigned wk,char** res)
{
    int st;
    if((st = WimeSendKey(wi->WimeCxn,wk,res)) == -2){
	//再変換キーだった
	gchar* sur;
	gint cursor;
	gunichar2* u16;
	int len,pos;

	gtk_im_context_get_surrounding(GTK_IM_CONTEXT(wi),&sur,&cursor);
	cursor = g_utf8_strlen(sur,cursor); //バイトオフセット→文字単位
	LOG("cursor %d strlen %d\n",cursor,g_utf8_strlen(sur,INT_MAX));
	u16 = g_utf8_to_utf16(sur,-1,NULL,NULL,NULL);
	len = WimeReconvert(wi->WimeCxn,u16,cursor,&pos);
	*res = NULL;
	st = (len>0);
	if(st){
	    pos -= cursor; //元の文字列を消す（カーソルからの相対位置）
	    LOG("delete pos %d,len %d\n",pos,len);
	    gtk_im_context_delete_surrounding(GTK_IM_CONTEXT(wi),pos,len);
	}
	g_free(sur);
	g_free(u16);
    }
    return st!=0;
}

static gboolean aux_input(IMContextWime* wi)
{
    uint16_t *u16;
    char *u;

    free(wi->PreeditStr);
    wi->PreeditStr = NULL;

    u = commit(wi,U16ToU8(NULL,u16=WimeGetResultStr(wi->WimeCxn),-1));
    VERBOSE(Array d;ArNew(&d,1,NULL);
	    MSG("aux input,utf8 string=%s\n",ArAdr(Dump1(" 0x%02x",u,strlen(u),&d)));
	    ArDelete(&d));
    free(u);
    free(u16);
    return TRUE;
}

/*
  keyをimが処理すればTRUEを返す
*/
gboolean imwime_filter_keypress(GtkIMContext* context,GdkEventKey* ev)
{
    unsigned wk;
    char *res;
    gboolean st = FALSE;
    IMContextWime *wi = IMCONTEXT_WIME(context);
    //IMContextWimeClass *wc = IMWIME_GET_CLASS(wi);
    KeySym xk;
    KeyCode xc;

    if(ev->type == GDK_KEY_RELEASE)
	return FALSE;

    if(!WimeIsConnected())
	WimeInitialize(0,LOGMARK);
    if(setjmp(WimeJmp) != 0){
	ERR("Disconnect wime\n");
	return ascii_mode(wi,ev->keyval,ev->state);
    }

    if((ev->state & 0xff) == AUX_INPUT_MOD) //[atok]パレットからの入力
	return aux_input(wi);

    /*
      例えば'('を入力した場合、ev->keyvalはXK_parenleftになる。一方winに対応する仮想キーコードはなく、shift+'9'と表される。
      この辺の記号はキーボードによって位置が違うため単純にテーブルをつくることもできない。
      スキャンコードを渡そうかとも思ったが、Xのキーコードとwinのスキャンコードは違う。キーコード→スキャンコードの変換関数はあるかどうか不明。
      そこで通常のキーsymをキーコードにし、それをもう一度キーsymにしてキーリストの１番目のキーsym(シフトのないキーsym)をwin仮想キーに変換する。

      imeトグルキーの比較でも問題が起きた。A-Zenkaku_Hankakuにしている場合、keyvalにくるのはZenkaku_HankakuではなくKanjiだ。(ximではキーコードが来るので問題ない）
      このため全ての入力でキーsymの変換をすることになってしまった。
      !!!トグルキーはキーコードでもっておいた方がいいだろう。
    */
    xc = XKeysymToKeycode(GDK_DISPLAY(),ev->keyval);
    xk = XKeycodeToKeysym(GDK_DISPLAY(),xc,0);
    LOG("keysym 0x%x --> keycode 0x%x --> keysym 0x%x\n",ev->keyval,xc,xk);
    if(!(wi->Flag & ENABLE_IME)){
	if(IsToggleKey(ToggleKeys,xk,ev->state)){
	    //漢字モード開始
	    WimeEnableIme(wi->WimeCxn,IME_ON);
	    wi->Flag |= ENABLE_IME;
	}else{
	    //asciiモード
	    st = ascii_mode(wi,ev->keyval,ev->state);
	}
	return st;
    }

    if(IsToggleKey(ToggleKeys,xk,ev->state)){
	//漢字モード終了。commiしていなければ漢字モードを続ける。
	if(wi->PreeditStr == NULL){
	    WimeEnableIme(wi->WimeCxn,IME_OFF);
	    wi->Flag &= ~ENABLE_IME;
	}
	return FALSE;
    }

    //変換中
    wk = ConvToVk(xk,ev->state);
    LOG("windows vk 0x%x\n",wk);
    if(!send_key(wi,wk,&res)){
	//imeに処理されなかった
	return ascii_mode(wi,ev->keyval,ev->state);
    }

    st = TRUE;
    free(wi->PreeditStr); //中身は使わないので先に解放
    if(res==NULL){ //入力途中
	if(wi->PreeditStr == NULL)
	    g_signal_emit_by_name(wi,SigPeStart);
	res = WimeGetCompStr(wi->WimeCxn,&wi->StrInfo);
	if(res!=NULL || wi->PreeditStr!=NULL){
	    /*
	      前編集文字列が１文字の時にbsを押すとresはNULLになるが
	      wi->PreeditStrにはまだ修正前の前編集文字列がある。
	      その後更にbsを押すとelse節へいく。
	    */
	    wi->PreeditStr = EjToU8(NULL,res);
	    g_signal_emit_by_name(wi,SigPeChanged);
	    LOG("preedit string='%s'\n",res);
	}else{
	    /*
	      imeに処理されたが現在も直前も前編集文字列がない場合は
	      空の状態でエンターやbsを押した場合だろう。
	    */
	    st = ascii_mode(wi,ev->keyval,ev->state);
	    LOG("control char\n");
	}
    }else{ //確定
	free(commit(wi,EjToU8(NULL,res)));
	LOG("commit '%s'\n",res);
    }
    free(res);
    return st;
}

//utf8での文字オフセットをバイトオフセットにする
static int offset_char_to_byte(const char* u8,int char_offset)
{
    return g_utf8_offset_to_pointer(u8,char_offset) - u8;
}

//開始時にすぐ呼ばれる
void imwime_get_preedit_str(GtkIMContext* context,gchar** str,PangoAttrList** attrs,gint* cursor_pos)
{
    IMContextWime *wi = IMCONTEXT_WIME(context);
    PangoAttribute *at;
    gint cursor_dum,cl_start=-1,cl_end;

    if(cursor_pos == NULL)
	cursor_pos = &cursor_dum;

    if(attrs != NULL)
	*attrs = pango_attr_list_new(); //これもないとだめ
    if(wi->PreeditStr == NULL){
	if(str != NULL)
	    *str = g_strdup("");
	*cursor_pos = 0;
	return;
    }

    if(str != NULL){
	*str = g_strdup(wi->PreeditStr);
    }
    if(wi->StrInfo.TargetClause>=0){ //注目文節がある
	cl_start = offset_char_to_byte(wi->PreeditStr,wi->StrInfo.TargetClause);
	cl_end = offset_char_to_byte(wi->PreeditStr,wi->StrInfo.TargetClause+wi->StrInfo.TargetClLen);
    }
    if(attrs != NULL){
	at = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
	at->start_index = 0;
	at->end_index = strlen(*str);
	pango_attr_list_insert(*attrs,at);

	if(cl_start >= 0){ //注目文節がある
	    at = pango_attr_background_new(0,0,0);
	    at->start_index = cl_start;
	    at->end_index = cl_end;
	    pango_attr_list_insert(*attrs,at);

	    at = pango_attr_foreground_new(-1,-1,-1);
	    at->start_index = cl_start;
	    at->end_index = cl_end;
	    pango_attr_list_insert(*attrs,at);
	}
    }

    //カーソル位置はバイトではなく文字単位
    *cursor_pos = cl_start>=0 ? wi->StrInfo.TargetClause:strlen(wi->PreeditStr);
}

//ウィンドウの移動／大きさ変更でもこれが呼ばれるようだ
void imwime_set_cursor_loc(GtkIMContext* context,GdkRectangle* area)
{
    gint d;
    GdkRectangle r;
    IMContextWime *wi = IMCONTEXT_WIME(context);

    if(!WimeIsConnected())
	WimeInitialize(0,LOGMARK);
    if(setjmp(WimeJmp) != 0){
	ERR("Disconnect wime\n");
	return;
    }

    if(wi->Client != NULL){
	gdk_window_get_geometry(wi->Client,&d,&d,&r.width,&r.height,&d);
	gdk_window_get_origin(wi->Client,&r.x,&r.y);
	if(memcmp(&wi->Geom,&r,sizeof(r)) != 0){
	    wi->Geom = r;
	    WimeMoveShadowWin(wi->WimeCxn,r.x,r.y,r.width,r.height);
	    LOG("shadow window (%d,%d) %dx%d\n",r.x,r.y,r.width,r.height);
	}
	r = *area;
	r.y += r.height+3;
	if(wi->PreeditStr!=NULL && memcmp(&wi->CandPos,&r,sizeof(r))!=0){
	    wi->CandPos = r;
	    WimeSetCandWin(wi->WimeCxn,WIME_POS_POINT,r.x,r.y);
	    LOG("candidate window (%d,%d)\n",r.x,r.y);
	}
    }
}

void imwime_set_client_window(GtkIMContext* context,GdkWindow* window)
{
    if(setjmp(WimeJmp) != 0)
	return;

    IMContextWime *wi = IMCONTEXT_WIME(context);
    wi->Client = window;
    LOG("cxn=%d gdkwin=%p xwin=0x%x\n",wi->WimeCxn,window,GDK_DRAWABLE_XID(window));
    WimeRegXWindow(wi->WimeCxn,GDK_DRAWABLE_XID(window));
}

void imwime_set_focus(GtkIMContext* context,gboolean state,const char* msg)
{
    if(setjmp(WimeJmp) != 0)
	return;

    IMContextWime *wi = IMCONTEXT_WIME(context);
    LOG("cxn=%d focus %s.\n",wi->WimeCxn,msg);
    WimeSetFocus(wi->WimeCxn,state);
}

void imwime_focus_in(GtkIMContext* context)
{
    imwime_set_focus(context,TRUE,"in");
}

void imwime_focus_out(GtkIMContext* context)
{
    imwime_set_focus(context,FALSE,"out");
}

void imwime_finalize(GObject* o)
{
    if(setjmp(WimeJmp) != 0)
	return;

    IMContextWime *wi = IMCONTEXT_WIME(o);
    LOG("finalize:cxn %d\n",wi->WimeCxn);
    WimeShowToolbar(wi->WimeCxn,FALSE,FALSE);
    WimeCloseContext(wi->WimeCxn);
    ArRemove(&Cxns,ArFind(&Cxns,0,&wi->WimeCxn));
    IMWIME_GET_CLASS(o)->FinalizeOrig(o);
}

/*
  wimeのコンテキストをつくってCxnsに記録する。

  TreeViewのインライン入力では,入力を完了させるたびにdisposeが呼ばれるみたい。
  class_initはアプリ立ち上げの１回しか呼ばれない。
*/
void imwime_init(GtkIMContext* cx)
{
    IMContextWime *wi = IMCONTEXT_WIME(cx);

    if(setjmp(WimeJmp) != 0)
	return;

    memset(&(wi->Parent)+1,0,sizeof(*wi)-sizeof(wi->Parent)); //IMContextWimeだけのメンバを0クリア
    wi->WimeCxn = WimeCreateContext();
    WimeShowToolbar(wi->WimeCxn,TRUE,FALSE);
    *(int*)ArExpand(&Cxns,1) = wi->WimeCxn;
    LOG("wime context %d\n",wi->WimeCxn);
}

////////////////////////////////////////////////////////

void imwime_class_init(GtkIMContextClass* cl)
{
    cl->filter_keypress =  imwime_filter_keypress;
    cl->get_preedit_string = imwime_get_preedit_str;
    cl->set_cursor_location = imwime_set_cursor_loc;
    cl->set_client_window = imwime_set_client_window;
    cl->focus_in = imwime_focus_in;
    cl->focus_out = imwime_focus_out;

    GObjectClass* o = G_OBJECT_CLASS(cl);
    IMCONTEXTWIMECLASS(cl)->FinalizeOrig = o->finalize;
    o->finalize = imwime_finalize;

    LOG("ok\n");
}

void imwime_class_fin(GtkIMContextClass* cl UNUSED)
{
    LOG("ok\n");
}

////////////////////////////////////////////////////////
//モジュールがexportする関数

const char ContextId[] = "wime";
const char ContextName[] = "Wime";
const char DomainName[] = "gtk20";
const char RegisterName[] = "IMContextWime";

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

void im_module_init(GTypeModule* module)
{
    GTypeInfo info = {
	sizeof(IMContextWimeClass),	//class_size
	NULL,				//GBaseInitFunc base_init
	NULL,				//GBaseFinalizeFunc base_finalize

	(GClassInitFunc)imwime_class_init,	//class_init
	(GClassFinalizeFunc)imwime_class_fin, 	//class_finalize
	NULL,           			//class_data

	sizeof(IMContextWime),		//instance_size
	0,				//n_preallocs
	(GtkObjectInitFunc)imwime_init,	//instance_init

	NULL			   	//value_table
    };

    Verbose = 1;
    RegisteredType = g_type_module_register_type(module,GTK_TYPE_IM_CONTEXT,RegisterName,&info,0);

    WimeInitialize(0,LOGMARK);
    InitDatabase(NULL,"gim");
    ToggleKeys = GetConvKeyFromResource(GDK_DISPLAY());
    ArNew(&Cxns,sizeof(int),NULL);

    LOG("ok\n");
}

void im_module_exit(void)
{
    LOG("ok\n"); //wimeとの接続を切る前に表示
    WimeFinalize();
    ArDelete(&Cxns);
}

void im_module_list(GtkIMContextInfo*** contexts,int* n_contexts)
{
    *contexts = ImcInfoList;
    *n_contexts = G_N_ELEMENTS(ImcInfoList);
}

GtkIMContext* im_module_create(const gchar* context_id)
{
    return strcmp(context_id,ContextId)==0 ? GTK_IM_CONTEXT(g_object_new(RegisteredType,NULL)) : NULL;
}
