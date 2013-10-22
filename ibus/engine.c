#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "engine.h"
#include "so/wimeapi.h"
#include "so/winkey.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"

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

static void create_wime_context(IBusWimeEngine* e)
{
    e->ServerLevel = RestartServerCount;
    e->WimeCxn = CannaCreateContext();
    WimeShowToolbar(e->WimeCxn,true,false);
}

static void replace_context(IBusWimeEngine* e)
{
    if(e->ServerLevel!=RestartServerCount || e->WimeCxn<0){
	//このコンテキストはサーバー再起動前のものと思われる。
	int old = e->WimeCxn;
	create_wime_context(e);
	LOG("replace wime context %d --> %d\n",old,e->WimeCxn);
    }
}

void destroy(IBusWimeEngine* e)
{
    LOG("\n");

    replace_context(e);
    WimeEnableIme(e->WimeCxn,FALSE);
    WimeShowToolbar(e->WimeCxn,false,false);
    CannaCloseContext(e->WimeCxn);
    WimeFinalize();

    ((IBusObjectClass*)ibus_wime_engine_parent_class)->destroy((IBusObject*)e);
}

#if !IBUS_CHECK_VERSION(1,4,0)
#define ibus_text_get_text(t) (t->text)
#endif

//分節文字列clが何番目の候補か。見つからなければ負数
//clはu8
int cand_index_cl(IBusWimeEngine* eng,const char* cl)
{
    int cs_pos,cs_pos0,cs_max;
    IBusText* bt;

    cs_pos=cs_pos0=ibus_lookup_table_get_cursor_pos(eng->CandTable)+1;
    cs_max=ibus_lookup_table_get_number_of_candidates(eng->CandTable);
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
//preeditはeucjp
int cand_index(IBusWimeEngine* eng,char* preedit,const WimeCompStrInfo* si)
{
    preedit = ForwardEj(preedit,si->TargetClause);
    char* endp = ForwardEj(preedit,si->TargetClLen);
    char save = *endp;
    *endp = 0;
    char* cl=EjToU8(NULL,preedit);
    *endp = save;
    int index=cand_index_cl(eng,cl);
    free(cl);
    return index;
}

//変換候補ウィンドウをつくる(表示しない)
void open_candidate(IBusWimeEngine* eng,char* preedit,const WimeCompStrInfo* si)
{
    replace_context(eng);
    if(eng->CandTable == NULL){
	LOG("create lookup table\n");
	int num;
	char** candlist = CannaGetCandidacyList(eng->WimeCxn,si->TargetNum,&num);
	eng->CandTable = g_object_ref_sink(ibus_lookup_table_new(10,0,TRUE,FALSE));
	for(char** c=candlist; *c!=NULL; ++c){
	    char* u8=EjToU8(NULL,*c);
	    ibus_lookup_table_append_candidate(eng->CandTable,ibus_text_new_from_string(u8));
	    free(u8);
	    free(*c);
	}
	free(candlist);
    }else{
	LOG("already create lookup table\n"); //あるんだろうか？
    }
}

/*
  変換中文字列を表示する。
  siにデータを返す。
  変換中文字列(eucjp)を返す。freeすること。変換中文字列がないか変換中でない場合NULLを返す。
*/
char* update_preedit(IBusWimeEngine* eng,WimeCompStrInfo* si)
{
    replace_context(eng);

    IBusText* ibt;
    char* ej = WimeGetCompStr(eng->WimeCxn,si);
    if(ej==NULL){ //bsなどで全部消した
	ibt = ibus_text_new_from_string("");
    }else{
	char* u8;
	ibt = ibus_text_new_from_string(u8 = EjToU8(NULL,ej));
	ibus_text_append_attribute(ibt,IBUS_ATTR_TYPE_UNDERLINE,IBUS_ATTR_UNDERLINE_SINGLE,0,si->Length);
	if(si->TargetClause != -1){
	    int attr[][2]={{IBUS_ATTR_TYPE_FOREGROUND,TARGETFG},
			   {IBUS_ATTR_TYPE_BACKGROUND,TARGETBG}};
	    for(int n=0; n<2; ++n)
		ibus_text_append_attribute(ibt,attr[n][0],attr[n][1],si->TargetClause,si->TargetClause+si->TargetClLen);
	}
	free(u8);
    }
    ibus_engine_update_preedit_text(IBUS_ENGINE(eng),ibt,si->CursorPos,TRUE);
    return ej;
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
    char* ej;
    int index;
    unsigned wk;
    KeySym xk;
    KeyCode xc;
    gboolean eaten=FALSE,search_cand;

    replace_context(eng);
    if((modifiers & IBUS_RELEASE_MASK) || !WimeEnableIme(eng->WimeCxn,IME_QUERY))
	return FALSE;

    LOG("val=%x code=%x mod=%x ime-enable=%d Cxn=%d\n",keyval,keycode,modifiers,eng->Flags,eng->WimeCxn);

    xc = XKeysymToKeycode(Disp,keyval);
    xk = XKEYCODETOKEYSYM(Disp,xc,0);
    wk = ConvToVk(xk,modifiers);
    LOG("sym 0x%x -> code 0x%x -> sym 0x%x -> vk 0x%x\n",keyval,xc,xk,wk);

    do{
	search_cand=FALSE;
	int st=WimeSendKey(eng->WimeCxn,wk,&ej);
	if(st==WIME_SENDKEY_ERROR || st==WIME_SENDKEY_NO_PROC){
	    eaten=FALSE;
	    break;
	}
	LOG("processed SendKey st=%d ej=%s\n",st,ej==NULL?"(null)":ej);

	if(ej==NULL){
	    //まだ未確定
	    WimeCompStrInfo si;
	    if((ej=update_preedit(eng,&si))==NULL){
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
		    open_candidate(eng,ej,&si);//今の候補文字列が候補リストにあるかどうかは次で調べる。*/
		case WIME_SENDKEY_CHGCAND:
		    index = cand_index(eng,ej,&si);
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
			LOG("retry convertion\n");
		    }
		    break;
		}
	    }
	}else{
	    //確定された
	    char* u8;
	    IBusText* ibt = ibus_text_new_from_string(u8 = EjToU8(NULL,ej));
	    ibus_engine_commit_text(IBUS_ENGINE(eng),ibt);
	    ibus_engine_hide_preedit_text(IBUS_ENGINE(eng));
	    free(u8);
	    release_cand_table(eng);
	    LOG("commit\n");
	}
	free(ej);
	eaten=TRUE;
    }while(search_cand);
    return eaten;
}

void set_focus(IBusWimeEngine* e,gboolean state)
{
    replace_context(e);
    LOG("cxn=%d focus %s.\n",e->WimeCxn,(state?"in":"out"));
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

void enable(IBusWimeEngine* e)
{
    LOG("\n");
    replace_context(e);

    //漢字モード開始
    WimeEnableIme(e->WimeCxn,IME_ON);
}

void disable(IBusWimeEngine* eng)
{
    LOG("\n");
    replace_context(eng);

    if(WimeEnableIme(eng->WimeCxn,IME_QUERY)){
	//漢字モード終了。
	char* ej;
	WimeCompStrInfo si;
	if((ej=WimeGetCompStr(eng->WimeCxn,&si))!=NULL){
	    //変換中だったときは消す。
	    free(ej);
	    IBusText* ibt = ibus_text_new_from_string("");
	    ibus_engine_update_preedit_text(IBUS_ENGINE(eng),ibt,si.CursorPos,TRUE);
	}
	WimeEnableIme(eng->WimeCxn,IME_OFF);
    }
}

void set_cursor_location(IBusWimeEngine* eng,gint x,gint y,gint w,gint h)
{
    LOG("x=%d y=%d w=%d h=%d\n",x,y,w,h);
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

    int f[]={IBUS_CAP_PREEDIT_TEXT,IBUS_CAP_AUXILIARY_TEXT,IBUS_CAP_LOOKUP_TABLE,IBUS_CAP_FOCUS,IBUS_CAP_PROPERTY,IBUS_CAP_SURROUNDING_TEXT};
    char* t[]={"IBUS_CAP_PREEDIT_TEXT","IBUS_CAP_AUXILIARY_TEXT","IBUS_CAP_LOOKUP_TABLE","IBUS_CAP_FOCUS","IBUS_CAP_PROPERTY","IBUS_CAP_SURROUNDING_TEXT"};
    char buf[200];

    strcpy(buf,"[");
    for(int n=0; n<sizeof(f)/sizeof(int); ++n){
	if(caps & f[n]){
	    if(buf[1]!=0)
		strcat(buf,"|");
	    strcat(buf,t[n]);
	    caps &= ~f[n];
	}
    }
    if(caps!=0){
	char buf2[20];
	sprintf(buf2,"0x%x",caps);
	strcat(strcat(buf,"|"),buf2);
    }
    strcat(buf,"]");
    LOG("caps=%s\n",buf);
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
    LOG("\n");
}

void page_down(IBusWimeEngine* eng)
{
    page_updown(eng,ibus_lookup_table_page_down);
    LOG("\n");
}

void cursor_up(IBusWimeEngine* eng)
{
    page_updown(eng,ibus_lookup_table_cursor_up);
    LOG("\n");
}

void cursor_down(IBusWimeEngine* eng)
{
    page_updown(eng,ibus_lookup_table_cursor_down);
    LOG("\n");
}

void candidate_clicked(IBusWimeEngine* eng,guint index,guint button,guint state)
{
    WimeCompStrInfo si;
    replace_context(eng);
    LOG("index=%d button=%d state=%d\n",index,button,state);
    IBusText* t=ibus_lookup_table_get_candidate(eng->CandTable,index);
    WimeSelectCandidate(eng->WimeCxn,cand_index_cl(eng,ibus_text_get_text(t)));
    free(update_preedit(eng,&si));
    release_cand_table(eng);
}

void reset(IBusEngine* engine)
{
    LOG("\n");
}
void property_activate(IBusEngine* engine,const gchar* prop_name,guint prop_state)
{
    LOG("name=%s state=0x%x\n",prop_name,prop_state);
}
void property_show(IBusEngine* engine,const gchar* prop_name)
{
    LOG("name=%s\n",prop_name);
}
void property_hide(IBusEngine* engine,const gchar* prop_name)
{
    LOG("name=%s\n",prop_name);
}
void set_surrounding_text(IBusEngine* engine,IBusText* text,guint cursor_index,guint anchor_pos)
{
    LOG("index=%d pos=%d\n",cursor_index,anchor_pos);
}
void process_hand_writing_event(IBusEngine* engine,const gdouble* coordinates,guint coordinates_len)
{
    LOG("\n");
}
void cancel_hand_writing(IBusEngine* engine,guint n_strokes)
{
    LOG("\n");
}

static void ibus_wime_engine_class_init(IBusWimeEngineClass* klass)
{
    LOG("\n");

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

    Verbose=1;
    WimeInitialize(SocketNum,LOGMARK);
    WimeRestartSignal(NULL,SocketNum);
}

static void ibus_wime_engine_init(IBusWimeEngine* eng)
{
    LOG("\n");

    eng->CandTable = NULL;
    eng->Flags = eng->TargetNum = 0;
    create_wime_context(eng);
}

//(C) 2012 thomas
