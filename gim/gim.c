// -*- coding:euc-jp -*-
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/keysym.h>
#include "gim.h"
#include "so/winkey.h"
#include "so/xres.h"
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

static void replace_context(GtkIMContext* context)
{
    IMContextWime* wi = IMCONTEXT_WIME(context);
    if(wi->ServerLevel!=RestartServerCount || wi->WimeCxn<0){
	//このコンテキストはサーバー再起動前のものと思われる。
	int old = wi->WimeCxn;
	wi->WimeCxn = CannaCreateContext();
	wi->ServerLevel = RestartServerCount;
	WimeShowToolbar(wi->WimeCxn,TRUE,FALSE);
	WimeShowCandidateWindow(wi->WimeCxn,TRUE);
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
    if((state & ~(ShiftMask|LockMask))==0 && !IsModifierKey(keyval) && isprint(ukey)){
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
    if((st = WimeSendKey(wi->WimeCxn,wk,res)) == WIME_SENDKEY_RECONV){
	//再変換キーだった
	gchar* sur;
	gint cursor;
	int pos;

	gtk_im_context_get_surrounding(GTK_IM_CONTEXT(wi),&sur,&cursor);
	cursor = g_utf8_strlen(sur,cursor); //バイトオフセット→文字単位
	DEBUGLOG(CH_GTK,"reconvert, cursor %d strlen %ld\n",cursor,g_utf8_strlen(sur,INT_MAX));
	int len = WimeReconvert(wi->WimeCxn,sur,cursor,&pos);
	*res = NULL;
	st = (len>0);
	if(st){
	    pos -= cursor; //元の文字列を消す（カーソルからの相対位置）
	    DEBUGLOG(CH_GTK,"delete pos %d,len %d\n",pos,len);
	    gtk_im_context_delete_surrounding(GTK_IM_CONTEXT(wi),pos,len);
	}
	g_free(sur);
    }
    return st!=0;
}

static gboolean aux_input(IMContextWime* wi)
{
    free(wi->PreeditStr);
    wi->PreeditStr = NULL;

    char* u8 = WimeGetResultStr(wi->WimeCxn);
    DEBUGLOG(CH_GTK,"aux input,utf8 string=[%*D]\n",strlen(u8),u8);
    free(commit(wi,u8));
    return TRUE;
}

/*
  keyをimが処理すればTRUEを返す
*/
gboolean imwime_filter_keypress(GtkIMContext* context,GdkEventKey* ev)
{
    gboolean st = FALSE;
    IMContextWime* wi = IMCONTEXT_WIME(context);
    //IMContextWimeClass* wc = IMWIME_GET_CLASS(wi);

    if(ev->type == GDK_KEY_RELEASE)
	return FALSE;
    if(ToggleKeys == NULL){
	ERRORLOG(CH_GTK,"not defined toggle key\n");
	return ascii_mode(wi,ev->keyval,ev->state);
    }

    replace_context(context);
    if(!WimeIsConnected())
	return ascii_mode(wi,ev->keyval,ev->state);

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
      [r96]mode_switchがあるとき(xmodmapでの2,3番目)はkeysymをそのまま渡す。
    */
    KeySym xk = ev->keyval;
    if((ev->state & MODESWITCHMASK) == 0){
	KeyCode xc = XKeysymToKeycode(XDISPLAY(),ev->keyval);
	xk = KeycodeToKeysym(XDISPLAY(),xc,ev->state,0); //XKEYCODETOKEYSYM(XDISPLAY(),xc,0);
	DEBUGLOG(CH_GTK,"keysym 0x%x(mod 0x%x) --> keycode 0x%x --> keysym 0x%lx\n",ev->keyval,ev->state,xc,xk);
    }else{
	DEBUGLOG(CH_GTK,"keysym 0x%lx + mode_switch\n",xk);
    }
    
    if(IsToggleKey(ToggleKeys,xk,ev->state)){
	static typeof(ev->time) prev_time;
	/* [3.4.3]firefoxでの入力で、入力候補がドロップダウンで表示されているときにimeをonにしようとすると
	   トグルキーが2回連続でくる。そのためon-->offとなってimeがonにならない。原因不明。
	   イベント発生時刻が同じなので、このキーイベントがトグルキーで、直前のイベントと同時刻だった場合
	   このキーは無視する。*/
	if(prev_time != ev->time){
	    if(!(wi->Flag & ENABLE_IME)){
		//漢字モード開始
		WimeEnableIme(wi->WimeCxn,IME_ON);
		wi->Flag |= ENABLE_IME;
	    }else{
		//漢字モード終了。commiしていなければ漢字モードを続ける。
		if(wi->PreeditStr == NULL){
		    WimeEnableIme(wi->WimeCxn,IME_OFF);
		    wi->Flag &= ~ENABLE_IME;
		}
	    }
	}
	prev_time = ev->time;
	return FALSE;
    }
    if(!(wi->Flag & ENABLE_IME)){
	//asciiモード
	return ascii_mode(wi,ev->keyval,ev->state);
    }

    //変換中
    char* res;
    unsigned wk = ConvToVk(xk,ev->state);
    DEBUGLOG(CH_GTK,"keysym 0x%lx --> windows vk 0x%x\n",xk,wk);
    if(!send_key(wi,wk,&res)){
	//imeに処理されなかった
	return ascii_mode(wi,ev->keyval,ev->state);
    }

    st = TRUE;
    free(wi->PreeditStr); //中身は使わないので先に解放 !!!このメンバは削除してibus-wimeみたいにしよう。
    if(res!=NULL){
	//全確定か注目文節までの部分確定
	ev->keyval=GDK_KEY_VoidSymbol; ev->hardware_keycode=0; //[r37]???問題ないか？
	commit(wi,res);
	DEBUGLOG(CH_GTK,"commit '%U'\n",res);
    }

    //入力途中、あるいは前半を部分確定して残りを変換中
    if(wi->PreeditStr == NULL)
	g_signal_emit_by_name(wi,SigPeStart);
    char* comp = WimeGetCompStr(wi->WimeCxn,&wi->StrInfo);
    if(comp!=NULL || wi->PreeditStr!=NULL){
	/*
	  前編集文字列が１文字の時にbsを押すとcompはNULLになるが
	  wi->PreeditStrにはまだ修正前の前編集文字列がある。
	  その後更にbsを押すとelse節へいく。
	*/
	wi->PreeditStr = comp; //一つ上のブロックで先に解放している。
	g_signal_emit_by_name(wi,SigPeChanged);
	DEBUGLOG(CH_GTK,"preedit string='%U'\n",comp);
    }else{
	/*
	  imeに処理されたが現在も直前も前編集文字列がない場合は
	  空の状態でエンターやbsを押した場合だろう。
	*/
	//確定文字列あり(and編集文字列なし)ならこのキーは何か処理されているので何もしない。
	if(res==NULL){
	    st = ascii_mode(wi,ev->keyval,ev->state);
	    DEBUGLOG(CH_GTK,"control char\n");
	}
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
    IMContextWime* wi = IMCONTEXT_WIME(context);
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
	PangoAttribute* at = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
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

    //カーソル位置はバイトではなく文字単位 [r18]入力中のカーソル移動ができなかった。
    *cursor_pos = wi->StrInfo.CursorPos;
}

//ウィンドウの移動／大きさ変更でもこれが呼ばれるようだ
void imwime_set_cursor_loc(GtkIMContext* context,GdkRectangle* area)
{
    gint d;
    GdkRectangle r;
    IMContextWime* wi = IMCONTEXT_WIME(context);

    replace_context(context);

    if(wi->Client != NULL){
	GDK_WINDOW_GET_GEOMETRY(wi->Client,&d,&d,&r.width,&r.height,&d);
	gdk_window_get_origin(wi->Client,&r.x,&r.y);
	if(memcmp(&wi->Geom,&r,sizeof(r)) != 0){
	    wi->Geom = r;
	    WimeMoveShadowWin(wi->WimeCxn,r.x,r.y,r.width,r.height);
	    DEBUGLOG(CH_GTK,"shadow window (%d,%d) %dx%d\n",r.x,r.y,r.width,r.height);
	    WimeSetCompWin(wi->WimeCxn,WIME_POS_POINT,area->x,area->y); //use_preedit()用
	}
	r = *area;
	r.y += r.height+3;
	if(wi->PreeditStr!=NULL && memcmp(&wi->CandPos,&r,sizeof(r))!=0){
	    wi->CandPos = r;
	    WimeSetCandWin(wi->WimeCxn,WIME_POS_POINT,r.x,r.y);
	    DEBUGLOG(CH_GTK,"candidate window (%d,%d)\n",r.x,r.y);
	}
    }
}

void imwime_set_client_window(GtkIMContext* context,GdkWindow* window)
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

    imwime_set_focus(context,TRUE,"in");
}

void imwime_focus_out(GtkIMContext* context)
{
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
    WimeShowCandidateWindow(wi->WimeCxn,TRUE);
    DEBUGLOG(CH_GTK,"wime context %d\n",wi->WimeCxn);
}

void imwime_reset(GtkIMContext* context)
{
    IMContextWime* wi = IMCONTEXT_WIME(context);
    char* comp = WimeGetCompStr(wi->WimeCxn,NULL);
    if(comp != NULL){
	DEBUGLOG(CH_GTK,"commit preedit string:%U\n",comp);
	free(wi->PreeditStr); //commit()でNULLにされてしまうので解放しておく。
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

void imwime_class_init(GtkIMContextClass* cl)
{
    cl->filter_keypress =  imwime_filter_keypress;
    cl->get_preedit_string = imwime_get_preedit_str;
    cl->set_cursor_location = imwime_set_cursor_loc;
    cl->set_client_window = imwime_set_client_window;
    cl->focus_in = imwime_focus_in;
    cl->focus_out = imwime_focus_out;
    cl->reset = imwime_reset;
#if 0
    cl->set_use_preedit = imwime_set_use_preedit;
#endif

    GObjectClass* o = G_OBJECT_CLASS(cl);
    IMCONTEXTWIMECLASS(cl)->FinalizeOrig = o->finalize;
    o->finalize = imwime_finalize;

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

    WimeInitialize(ParseEnv(CH_GLOBAL|CH_GTK),'g');
    InitDatabase(NULL,"gim");
    ToggleKeys = GetConvKeyFromResource(XDISPLAY());
    WimeRestartSignal(NULL);

    DEBUGLOG(CH_GTK,IMDOMAIN "\n");
}

void im_module_exit(void)
{
    DEBUGLOG(CH_GTK,IMDOMAIN "\n");
    WimeFinalize();
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

//(C) 2009 thomas
