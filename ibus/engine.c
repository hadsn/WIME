#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "engine.h"
#include "so/wimeapi.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "lib/log.h"
#include "lib/list.h"
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include "exe/at.h"

ATImeCol ImeColor[ATIMECOMPCOL_ITEMMAX];

extern int SocketNum;

G_DEFINE_TYPE(IBusWimeEngine,ibus_wime_engine,IBUS_TYPE_ENGINE);
/*
  G_DEFINE_TYPE(型名(CamelCase),型名(LowerCase),親のGType)
  上の例なら
  static void ibus_wime_engine_init(IBusWimeEngine* self)
  static void ibus_wime_engine_class_init(IBusWimeEngineClass* klass)
  をつくる必要がある。親クラスは
  static gpointer ibus_wime_engine_parent_class
  が定義される。
*/

void create_wime_context(IBusWimeEngine* eng)
{
    eng->ServerLevel = RestartServerCount;
    eng->WimeCxn = CannaCreateContext();
    WimeShowToolbar(eng->WimeCxn,true,false);
}

static void replace_context(IBusWimeEngine* eng)
{
    if(eng->ServerLevel!=RestartServerCount || eng->WimeCxn<0){
	//このコンテキストはサーバー再起動前のものと思われる。
	int old = eng->WimeCxn;
	create_wime_context(eng);
	DEBUGLOG(CH_IBUS,"replace wime context %d --> %d\n",old,eng->WimeCxn);
    }
}

void destroy(IBusWimeEngine* e)
{
    DEBUGLOG(CH_IBUS,"\n");

    replace_context(e);
    WimeEnableIme(e->WimeCxn,FALSE);
    WimeShowToolbar(e->WimeCxn,false,false);
    CannaCloseContext(e->WimeCxn);
    WimeFinalize();
    free(e->ToggleKeys);

    ((IBusObjectClass*)ibus_wime_engine_parent_class)->destroy((IBusObject*)e);
}

#if !IBUS_CHECK_VERSION(1,4,0)
#define ibus_text_get_text(t) (t->text)
#endif

//分節文字列clが何番目の候補か。見つからなければ負数
//clはu8
static int cand_index_cl(IBusWimeEngine* eng,const char* cl)
{
    int cs_pos,cs_pos0,cs_max;

    cs_pos = cs_pos0 = ibus_lookup_table_get_cursor_pos(eng->CandTable)+1;
    cs_max = ibus_lookup_table_get_number_of_candidates(eng->CandTable);
    if(cs_pos==cs_max-1)
	cs_pos=0; //最後までいったら先頭へ
    for(; cs_pos<cs_max; ++cs_pos){
	IBusText* text=ibus_lookup_table_get_candidate(eng->CandTable,cs_pos);
	if(strcmp(cl,ibus_text_get_text(text)) == 0){
	    break;
	}
    }
    if(cs_pos==cs_max){
	//見つからなければ上方向に探す
	for(cs_pos=cs_pos0-1; cs_pos>=0; --cs_pos){
	    IBusText* text=ibus_lookup_table_get_candidate(eng->CandTable,cs_pos);
	    if(strcmp(cl,ibus_text_get_text(text)) == 0){
		break;
	    }
	}
    }
    return cs_pos;
}

#if 0
//siで示す分節が何番目の候補か。見つからなければ負数
//preeditはutf8
static int cand_index(IBusWimeEngine* eng,const char* preedit,const WimeCompStrInfo* si)
{
    preedit = ForwardU8(preedit,si->TargetClause);
    unsigned len = ForwardU8(preedit,si->TargetClLen)-preedit;
    char* u8 = strndup(preedit,len);
    int index = cand_index_cl(eng,u8);
    DEBUGLOG(CH_IBUS,"%d:%d:%U->%d\n",si->TargetClause,si->TargetClLen,u8,index);
    free(u8);
    return index;
}
#endif

//変換候補ウィンドウをつくる(表示しない)
static void open_candidate(IBusWimeEngine* eng,const WimeCompStrInfo* si)
{
    replace_context(eng);
    if(eng->CandTable == NULL){
	DEBUGLOG(CH_IBUS,"create lookup table\n");
	Array* candlist = CannaGetCandidacyList(eng->WimeCxn,si->TargetNum);
	ListRemove(candlist,ListCount(candlist)-1); //読みはいらない
	DEBUGLOG(CH_IBUS,"%*D\n",ArUsing(candlist),ArAdr(candlist));
	eng->CandTable = g_object_ref_sink(ibus_lookup_table_new(10,0,true,true));
	char* cstr;
	for(int index=0; (cstr = ListInc(candlist,index))!=NULL; ++index){
	    ibus_lookup_table_append_candidate(eng->CandTable,ibus_text_new_from_string(cstr));
	}
	free(ArDelete(candlist));
    }else{
	DEBUGLOG(CH_IBUS,"already create lookup table\n"); //あるんだろうか？
    }
    eng->ImeIndex = 0;
}

#define IBUSRGB(colref) (GETR(colref)<<16)|(GETG(colref)<<8)|GETB(colref)

void set_color(IBusText* text,int colindex,int start,int end)
{
    if(ImeColor[colindex].UnderLine)
	ibus_text_append_attribute(text,IBUS_ATTR_TYPE_UNDERLINE,IBUS_ATTR_UNDERLINE_SINGLE,start,end);
    ibus_text_append_attribute(text,IBUS_ATTR_TYPE_FOREGROUND,IBUSRGB(ImeColor[colindex].Text),start,end);
    ibus_text_append_attribute(text,IBUS_ATTR_TYPE_BACKGROUND,IBUSRGB(ImeColor[colindex].Back),start,end);
}

void release_cand_table(IBusWimeEngine* eng)
{
    replace_context(eng);
    if(eng->CandTable!=NULL){
	ibus_engine_hide_lookup_table(IBUS_ENGINE(eng));
	g_object_unref(eng->CandTable);
	eng->CandTable = NULL;
	WimeCloseCandWin(eng->WimeCxn);
    }
}

void wime_preedit(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    DEBUGLOG(CH_IBUS,"%U\n",u8);
    IBusText* text = ibus_text_new_from_string(u8);
    if(*u8==0){
	release_cand_table(arg); //編集文字列がなくなった、キャンセルした
    }else{
	set_color(text,ATCOLINDEX_INPUT,0,si->Length);
	if(si->TargetClause != -1){
	    set_color(text,ATCOLINDEX_TARGETCONVERT,si->TargetClause,si->TargetClause+si->TargetClLen);
	}
    }
    ibus_engine_update_preedit_text(IBUS_ENGINE(arg),text,si->CursorPos,TRUE);
    g_object_unref(text);
}

/*
  変換中文字列を表示する。
*/
void update_preedit(IBusWimeEngine* eng)
{
    WimeCompStrInfo si;
    replace_context(eng);
    char* u8 = WimeGetCompStr(eng->WimeCxn,&si);
    wime_preedit(u8!=NULL ? u8 : "",&si,eng);
    free(u8);
}

void wime_convert(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    DEBUGLOG(CH_IBUS,"%U\n",u8);
    IBusWimeEngine* eng = arg;
    if(eng->TargetNum!=si->TargetNum){
	//文節が移動したらすでに表示している候補ウィンドウを閉じる。
	release_cand_table(eng);
	eng->TargetNum=si->TargetNum;
    }
    wime_preedit(u8,si,arg);
}

bool wime_change_cand(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    IBusWimeEngine* eng = arg;
    int ime_index = WimeCandIndex(eng->WimeCxn);

    int skip = ime_index-eng->ImeIndex;
    DEBUGLOG(CH_IBUS,"str=%U index=%d %d %d\n",u8,ime_index,eng->ImeIndex,skip);
    if(skip != 0){
	if(abs(skip) > 9)
	    skip %= 2; //10以上離れたら最後の候補から先頭(あるいはその逆)に戻ったことにする。
	if(abs(skip) > 5){
	    if(skip >= 0)
		ibus_lookup_table_page_down(eng->CandTable);
	    else
		ibus_lookup_table_page_up(eng->CandTable);
	}else{
	    if(skip >= 0)
		ibus_lookup_table_cursor_down(eng->CandTable);
	    else
		ibus_lookup_table_cursor_up(eng->CandTable);
	}
    }
    /* 変換キーで1回ずつ変換する場合、ImmGetCandidateList()で得られる文字列以外の候補が出てくる場合がある。
       2周目で別の候補が出てくる場合もある。(たぶんatokのせいだろう)
       変換キーで候補を順番に出すときとImmGetCandidateList()で候補の順番が違う。
       →候補の変更を受け取ったときはibusの候補と同じになるまで注目文節を変換する。
    */
    bool another_cand=false;
    int index = ibus_lookup_table_get_cursor_pos(eng->CandTable);
    IBusText* text = ibus_lookup_table_get_candidate(eng->CandTable,index);
    DEBUGLOG(CH_IBUS,"ibus cand=%d,%U\n",index,ibus_text_get_text(text));
    if(strcmp(u8,ibus_text_get_text(text)) != 0){
	bool match;
	do{
	    char* cand=CannaStoreRange(eng->WimeCxn,-1,NULL);
	    match = (strcmp(cand,ibus_text_get_text(text))==0);
	    DEBUGLOG(CH_IBUS,"storerange %U -> %d\n",cand,match);
	    free(cand);
	}while(!match);
	another_cand=true; //WimeSendKeyに戻ったらu8,siを再取得する。
    }
    eng->ImeIndex = WimeCandIndex(eng->WimeCxn);
    ibus_lookup_table_set_cursor_pos(eng->CandTable,index);
    ibus_engine_update_lookup_table(IBUS_ENGINE(eng),eng->CandTable,TRUE);
    return another_cand;
}

bool wime_open_cand(const char* u8,const WimeCompStrInfo* si,void* arg)
{
    DEBUGLOG(CH_IBUS,"%U\n",u8);
    open_candidate(arg,si);
    return wime_change_cand(u8,si,arg);//今の候補文字列が候補リストにあるかどうか調べる。*/
}

void wime_commit(const char* u8,const char* composition,const WimeCompStrInfo* si,void* arg)
{
    DEBUGLOG(CH_IBUS,"commit:%U\n",u8);
    IBusText* text = ibus_text_new_from_string(u8);
    ibus_engine_commit_text(IBUS_ENGINE(arg),text);
    ibus_engine_hide_preedit_text(IBUS_ENGINE(arg));
    if(composition != NULL)
	wime_preedit(composition,si,arg);
    release_cand_table(arg);
    g_object_unref(text);
}

__attribute__((constructor))
static void init_cb()
{
    WimePreedit = wime_preedit;
    WimeConvert = wime_convert;
    WimeCommit = wime_commit;
    WimeOpenCandidate = wime_open_cand;
    WimeChangeCandidate = wime_change_cand;
}

//TRUE for successfully process the key
gboolean process_key(IBusWimeEngine* eng,guint keyval,guint keycode,guint modifiers)
{
    replace_context(eng);
    if((modifiers & IBUS_RELEASE_MASK))
	return FALSE;

    DEBUGLOG(CH_IBUS,"val=0x%x code=0x%x mod=0x%x flags=0x%x Cxn=%d\n",keyval,keycode,modifiers,eng->Flags,eng->WimeCxn);

    return WimeFilterKey(eng->WimeCxn,eng->ToggleKeys,Disp,keycode,keyval,modifiers,eng);
}

void set_focus(IBusWimeEngine* e,gboolean state)
{
    replace_context(e);
    DEBUGLOG(CH_IBUS,"cxn=%d focus %s.\n",e->WimeCxn,(state?"in":"out"));
    WimeSetFocus(e->WimeCxn,state);
}

void focus_in(IBusWimeEngine* e)
{
    set_focus(e,TRUE);
}

void focus_out(IBusWimeEngine* e)
{
    set_focus(e,FALSE);
}

void set_cursor_location(IBusWimeEngine* eng,gint x,gint y,gint w,gint h)
{
    DEBUGLOG(CH_IBUS,"x=%d y=%d w=%d h=%d\n",x,y,w,h);
    replace_context(eng);
    WimeMoveShadowWin(eng->WimeCxn,0,0,1,1);
    WimeSetCandWin(eng->WimeCxn,WIME_POS_POINT,x,y+h+3); //+3は適当な数字
}

/*
  IBUS_CAP_PREEDIT_TEXT		UI is capable to show pre-edit text.
  IBUS_CAP_AUXILIARY_TEXT	UI is capable to show auxiliary text.
  IBUS_CAP_LOOKUP_TABLE		UI is capable to show the lookup table.
  IBUS_CAP_FOCUS		UI is capable to get focus.
  IBUS_CAP_PROPERTY		UI is capable to have property.
  IBUS_CAP_SURROUNDING_TEXT	Client can provide surround text, or IME can handle surround text. 
*/
void set_capabilities(IBusWimeEngine* eng,guint caps)
{
    replace_context(eng);

    //{on|over)-the-spotでなければ候補ウィンドウはibusのものを使う。
    if((caps & IBUS_CAP_PREEDIT_TEXT)==0 || (Flags & USE_IBUS_CANDIDATE_WINDOW)!=0)
	WimeShowCandWin(eng->WimeCxn,false);

    BitDesc bits[]={
	BITDESC(IBUS_CAP_PREEDIT_TEXT),
	BITDESC(IBUS_CAP_AUXILIARY_TEXT),
	BITDESC(IBUS_CAP_LOOKUP_TABLE),
	BITDESC(IBUS_CAP_FOCUS),
	BITDESC(IBUS_CAP_PROPERTY),
	BITDESC(IBUS_CAP_SURROUNDING_TEXT),
	{0,NULL}};
    Array buf;
    ArNew(&buf,1,NULL);
    ArAddChar(&buf,'[');
    FlagStr(caps,bits,&buf);
    ArAddChar(&buf,']');
    DEBUGLOG(CH_IBUS,"caps=%s\n",(char*)ArAdr(&buf));
    ArDelete(&buf);
}

void page_updown(IBusWimeEngine* eng,gboolean (*updown)(IBusLookupTable*))
{
    replace_context(eng);
    if((*updown)(eng->CandTable)){
	ibus_engine_update_lookup_table(IBUS_ENGINE(eng),eng->CandTable,TRUE);
	WimeSelectCand(eng->WimeCxn,ibus_lookup_table_get_cursor_pos(eng->CandTable));
	update_preedit(eng);
    }
}

void page_up(IBusWimeEngine* eng)
{
    page_updown(eng,ibus_lookup_table_page_up);
    DEBUGLOG(CH_IBUS,"\n");
}

void page_down(IBusWimeEngine* eng)
{
    page_updown(eng,ibus_lookup_table_page_down);
    DEBUGLOG(CH_IBUS,"\n");
}

void cursor_up(IBusWimeEngine* eng)
{
    page_updown(eng,ibus_lookup_table_cursor_up);
    DEBUGLOG(CH_IBUS,"\n");
}

void cursor_down(IBusWimeEngine* eng)
{
    page_updown(eng,ibus_lookup_table_cursor_down);
    DEBUGLOG(CH_IBUS,"\n");
}

void candidate_clicked(IBusWimeEngine* eng,guint index,guint button,guint state)
{
    replace_context(eng);
    DEBUGLOG(CH_IBUS,"index=%d button=%d state=%d\n",index,button,state);
    IBusText* text=ibus_lookup_table_get_candidate(eng->CandTable,index);
    int pos = cand_index_cl(eng,ibus_text_get_text(text));
    if(pos > 0)
	WimeSelectCand(eng->WimeCxn,(unsigned)pos);
    update_preedit(eng);
    release_cand_table(eng);
}

void reset(IBusEngine* engine)
{
    DEBUGLOG(CH_IBUS,"\n");
}
void property_activate(IBusEngine* engine,const gchar* prop_name,guint prop_state)
{
    DEBUGLOG(CH_IBUS,"name=%s state=0x%x\n",prop_name,prop_state);
}
void property_show(IBusEngine* engine,const gchar* prop_name)
{
    DEBUGLOG(CH_IBUS,"name=%s\n",prop_name);
}
void property_hide(IBusEngine* engine,const gchar* prop_name)
{
    DEBUGLOG(CH_IBUS,"name=%s\n",prop_name);
}
void set_surrounding_text(IBusEngine* engine,IBusText* text,guint cursor_index,guint anchor_pos)
{
    DEBUGLOG(CH_IBUS,"index=%d pos=%d\n",cursor_index,anchor_pos);
}
void process_hand_writing_event(IBusEngine* engine,const gdouble* coordinates,guint coordinates_len)
{
    DEBUGLOG(CH_IBUS,"\n");
}
void cancel_hand_writing(IBusEngine* engine,guint n_strokes)
{
    DEBUGLOG(CH_IBUS,"\n");
}

static void catch_restart_signal(void)
{
    ++RestartServerCount;
    DEBUGLOG(CH_IBUS,"count %d\n",RestartServerCount);
    WimeGetColor(0,ImeColor);
}

static void ibus_wime_engine_class_init(IBusWimeEngineClass* klass)
{
    IBusObjectClass* obj_cl = IBUS_OBJECT_CLASS(klass);
    IBusEngineClass* eng_cl = IBUS_ENGINE_CLASS(klass);
	
    obj_cl->destroy = (IBusObjectDestroyFunc)destroy;

    eng_cl->process_key_event = (typeof(eng_cl->process_key_event))process_key;
    eng_cl->candidate_clicked = (typeof(eng_cl->candidate_clicked))candidate_clicked;
    eng_cl->cursor_down = (typeof(eng_cl->cursor_down))cursor_down;
    eng_cl->cursor_up = (typeof(eng_cl->cursor_up))cursor_up;
    //eng_cl->disable = (typeof(eng_cl->disable))disable;
    //eng_cl->enable = (typeof(eng_cl->enable))enable;
    eng_cl->focus_in = (typeof(eng_cl->focus_in))focus_in;
    eng_cl->focus_out = (typeof(eng_cl->focus_out))focus_out;
    eng_cl->page_down = (typeof(eng_cl->page_down))page_down;
    eng_cl->page_up = (typeof(eng_cl->page_up))page_up;
    eng_cl->property_activate =     property_activate;
    eng_cl->property_hide =     property_hide;
    eng_cl->property_show =     property_show;
    eng_cl->reset =     reset;
    eng_cl->set_capabilities = (typeof(eng_cl->set_capabilities))set_capabilities;
    eng_cl->set_cursor_location = (typeof(eng_cl->set_cursor_location))set_cursor_location;
#if IBUS_CHECK_VERSION(1,4,0)
    eng_cl->cancel_hand_writing =     cancel_hand_writing;
    eng_cl->set_surrounding_text =     set_surrounding_text;
    eng_cl->process_hand_writing_event =     process_hand_writing_event;
#endif

    WimeInitialize(SocketNum,'i');
    WimeRestartSignal(catch_restart_signal);
    WimeGetColor(0,ImeColor);
    DEBUGLOG(CH_IBUS,"\n");
}

static void ibus_wime_engine_init(IBusWimeEngine* eng)
{
    DEBUGLOG(CH_IBUS,"\n");

    eng->CandTable = NULL;
    eng->Flags = eng->TargetNum = 0;
    eng->ToggleKeys = GetConvKeyFromResource(Disp);
    create_wime_context(eng);
}

//(C) 2012 thomas
