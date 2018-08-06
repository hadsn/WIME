#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "engine.h"
#include "so/wimeapi.h"
#include "so/winkey.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "lib/log.h"
#include "lib/list.h"

//注目文節の色
#define TARGETFG	0xff0000
#define TARGETBG 	0xffffff

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

void enable(IBusWimeEngine* eng)
{
    DEBUGLOG(CH_IBUS,"\n");
    replace_context(eng);

    if(!WimeEnableIme(eng->WimeCxn,IME_QUERY)){
	//漢字モード開始
	WimeEnableIme(eng->WimeCxn,IME_ON);
    }
}

void disable(IBusWimeEngine* eng)
{
    DEBUGLOG(CH_IBUS,"\n");
    replace_context(eng);

    if(WimeEnableIme(eng->WimeCxn,IME_QUERY)){
	//漢字モード終了。
	char* u8;
	WimeCompStrInfo si;
	if((u8=WimeGetCompStr(eng->WimeCxn,&si))!=NULL){
	    //変換中だったときは消す。
	    free(u8);
	    IBusText* ibt = ibus_text_new_from_string("");
	    ibus_engine_update_preedit_text(IBUS_ENGINE(eng),ibt,si.CursorPos,TRUE);
	}
	WimeEnableIme(eng->WimeCxn,IME_OFF);
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
    IBusText* bt;

    cs_pos = cs_pos0 = ibus_lookup_table_get_cursor_pos(eng->CandTable)+1;
    cs_max = ibus_lookup_table_get_number_of_candidates(eng->CandTable);
    if(cs_pos==cs_max-1)
	cs_pos=0; //最後までいったら先頭へ
    for(; cs_pos<cs_max; ++cs_pos){
	bt=ibus_lookup_table_get_candidate(eng->CandTable,cs_pos);
	if(strcmp(cl,ibus_text_get_text(bt)) == 0){
	    break;
	}
    }
    if(cs_pos==cs_max){
	//見つからなければ上方向に探す
	for(cs_pos=cs_pos0-1; cs_pos>=0; --cs_pos){
	    bt=ibus_lookup_table_get_candidate(eng->CandTable,cs_pos);
	    if(strcmp(cl,ibus_text_get_text(bt)) == 0){
		break;
	    }
	}
    }
    return cs_pos;
}

//siで示す分節が何番目の候補か。見つからなければ負数
//preeditはutf8
static int cand_index(IBusWimeEngine* eng,char* preedit,const WimeCompStrInfo* si)
{
    preedit = ForwardU8(preedit,si->TargetClause);
    char* endp = ForwardU8(preedit,si->TargetClLen);
    char save = *endp;
    *endp = 0;
    int index=cand_index_cl(eng,preedit);
    *endp = save;
    return index;
}

//変換候補ウィンドウをつくる(表示しない)
static void open_candidate(IBusWimeEngine* eng,const WimeCompStrInfo* si)
{
    replace_context(eng);
    if(eng->CandTable == NULL){
	DEBUGLOG(CH_IBUS,"create lookup table\n");
	Array* candlist = CannaGetCandidacyList(eng->WimeCxn,si->TargetNum);
	eng->CandTable = g_object_ref_sink(ibus_lookup_table_new(10,0,TRUE,FALSE));
	char* cstr;
	for(int index=0; (cstr = ListInc(candlist,index))!=NULL; ++index){
	    ibus_lookup_table_append_candidate(eng->CandTable,ibus_text_new_from_string(cstr));
	}
	free(ArDelete(candlist));
    }else{
	DEBUGLOG(CH_IBUS,"already create lookup table\n"); //あるんだろうか？
    }
}

/*
  変換中文字列を表示する。
  siにデータを返す。
  変換中文字列(utf8)を返す。freeすること。変換中文字列がないか変換中でない場合NULLを返す。
*/
char* update_preedit(IBusWimeEngine* eng,WimeCompStrInfo* si)
{
    replace_context(eng);

    IBusText* ibt;
    char* u8 = WimeGetCompStr(eng->WimeCxn,si);
    if(u8==NULL){ //bsなどで全部消した
	ibt = ibus_text_new_from_string("");
    }else{
	ibt = ibus_text_new_from_string(u8);
	ibus_text_append_attribute(ibt,IBUS_ATTR_TYPE_UNDERLINE,IBUS_ATTR_UNDERLINE_SINGLE,0,si->Length);
	if(si->TargetClause != -1){
	    int attr[][2]={{IBUS_ATTR_TYPE_FOREGROUND,TARGETFG},
			   {IBUS_ATTR_TYPE_BACKGROUND,TARGETBG}};
	    for(int n=0; n<2; ++n)
		ibus_text_append_attribute(ibt,attr[n][0],attr[n][1],si->TargetClause,si->TargetClause+si->TargetClLen);
	}
    }
    ibus_engine_update_preedit_text(IBUS_ENGINE(eng),ibt,si->CursorPos,TRUE);
    return u8;
}

void release_cand_table(IBusWimeEngine* eng)
{
    replace_context(eng);
    if(eng->CandTable!=NULL){
	ibus_engine_hide_lookup_table(IBUS_ENGINE(eng));
	g_object_unref(eng->CandTable);
	eng->CandTable=NULL;
	WimeCloseCandidateWindow(eng->WimeCxn);
    }
}

//TRUE for successfully process the key
gboolean process_key(IBusWimeEngine* eng,guint keyval,guint keycode,guint modifiers)
{
    gboolean eaten=FALSE, search_cand;

    replace_context(eng);
    if((modifiers & IBUS_RELEASE_MASK))// || !WimeEnableIme(eng->WimeCxn,IME_QUERY))
	return FALSE;

    DEBUGLOG(CH_IBUS,"val=%x code=%x mod=%x ime-enable=%d Cxn=%d\n",keyval,keycode,modifiers,eng->Flags,eng->WimeCxn);

    KeyCode xc = XKeysymToKeycode(Disp,keyval);
    KeySym xk = KeycodeToKeysym(Disp,xc,modifiers,0);
    unsigned wk = ConvToVk(xk,modifiers);
    DEBUGLOG(CH_IBUS,"sym 0x%x -> code 0x%x -> sym 0x%lx -> vk 0x%x\n",keyval,xc,xk,wk);

    if(IsToggleKey(eng->ToggleKeys,xk,modifiers)){
	DEBUGLOG(CH_IBUS,"sym 0x%lx is ime toggle key\n",xk);
	if(WimeEnableIme(eng->WimeCxn,IME_QUERY))
	    disable(eng);
	else
	    enable(eng);
	return TRUE;
    }
    
    do{
	search_cand=FALSE;
	char* u8;
	int st = WimeSendKey(eng->WimeCxn,wk,&u8);
	if(st==WIME_SENDKEY_ERROR || st==WIME_SENDKEY_NO_PROC){
	    eaten=FALSE;
	    break;
	}
	DEBUGLOG(CH_IBUS,"processed SendKey st=%d str=%U\n",st,u8);

	if(u8==NULL){
	    //まだ未確定
	    WimeCompStrInfo si;
	    if((u8 = update_preedit(eng,&si)) == NULL){
		//escによるキャンセルの可能性があるので候補ウィンドウを消しておく。
		release_cand_table(eng);
	    }else{
		if(eng->TargetNum!=si.TargetNum){
		    //文節が移動したらすでに表示している候補ウィンドウを閉じる。
		    release_cand_table(eng);
		    eng->TargetNum=si.TargetNum;
		}
		switch(st){
		case WIME_SENDKEY_OPENCAND:
		    open_candidate(eng,&si);//今の候補文字列が候補リストにあるかどうかは次で調べる。*/
		case WIME_SENDKEY_CHGCAND:
		{
		    int index = cand_index(eng,u8,&si);
		    if(index >= 0){
			ibus_lookup_table_set_cursor_pos(eng->CandTable,index);
			ibus_engine_update_lookup_table(IBUS_ENGINE(eng),eng->CandTable,TRUE);
		    }else{
			/* ???
			   変換キーで1回ずつ変換する場合、ImmGetCandidateList()で得られる文字列以外の
			   候補が出てくる場合がある。2周目で別の候補が出てくる場合もある。(たぶんatokの
			   せいだろう)
			   今の注目文節が変換候補にない場合見つかるまでWimeSendKeyを繰り返す。
			   変換キーで候補を順番に出すときとImmGetCandidateList()で候補の順番が違う。
			   どうしたもんか?
			*/
			search_cand=TRUE;
			DEBUGLOG(CH_IBUS,"retry convertion\n");
		    }
		    break;
		}
		}
	    }
	}else{
	    //確定された
	    IBusText* ibt = ibus_text_new_from_string(u8);
	    ibus_engine_commit_text(IBUS_ENGINE(eng),ibt);
	    ibus_engine_hide_preedit_text(IBUS_ENGINE(eng));
	    release_cand_table(eng);
	    DEBUGLOG(CH_IBUS,"commit\n");
	}
	free(u8);
	eaten=TRUE;
    }while(search_cand);
    return eaten;
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
	WimeShowCandidateWindow(eng->WimeCxn,false);

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
	WimeCompStrInfo si;
	ibus_engine_update_lookup_table(IBUS_ENGINE(eng),eng->CandTable,TRUE);
	WimeSelectCandidate(eng->WimeCxn,ibus_lookup_table_get_cursor_pos(eng->CandTable));
	update_preedit(eng,&si);
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
    WimeCompStrInfo si;
    replace_context(eng);
    DEBUGLOG(CH_IBUS,"index=%d button=%d state=%d\n",index,button,state);
    IBusText* t=ibus_lookup_table_get_candidate(eng->CandTable,index);
    WimeSelectCandidate(eng->WimeCxn,cand_index_cl(eng,ibus_text_get_text(t)));
    free(update_preedit(eng,&si));
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

static void ibus_wime_engine_class_init(IBusWimeEngineClass* klass)
{
    IBusObjectClass* obj_cl = IBUS_OBJECT_CLASS(klass);
    IBusEngineClass* eng_cl = IBUS_ENGINE_CLASS(klass);
	
    obj_cl->destroy = (IBusObjectDestroyFunc)destroy;

    eng_cl->process_key_event = (typeof(eng_cl->process_key_event))process_key;
    eng_cl->candidate_clicked = (typeof(eng_cl->candidate_clicked))candidate_clicked;
    eng_cl->cursor_down = (typeof(eng_cl->cursor_down))cursor_down;
    eng_cl->cursor_up = (typeof(eng_cl->cursor_up))cursor_up;
    eng_cl->disable = (typeof(eng_cl->disable))disable;
    eng_cl->enable = (typeof(eng_cl->enable))enable;
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
    WimeRestartSignal(NULL);
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
