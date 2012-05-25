#include "wimexim.h"
#include "so/wimeapi.h"
#include <stdlib.h>
#include <string.h>

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	size;
    uint16_t	unused;
    Attribute	attr[0];
}__attribute__((packed)) XimGetIcValuesReply;

int set_ic_values(void* base,Attribute* al,int sz,const CallbackParam*);
int set_rectangle(void* adr,Attribute* a,const CallbackParam*);
int set_u32(void* adr,Attribute* a,const CallbackParam*);
int set_u16_to_u32(void* adr,Attribute* a,const CallbackParam*);
int set_string_to_cstr(void* adr,Attribute* a,const CallbackParam*);
int set_xpoint(void* adr,Attribute* a,const CallbackParam*);
int set_nested_list(void* adr,Attribute* a,const CallbackParam*);
int set_strcnv(void* adr,Attribute* a,const CallbackParam*);
int set_input_style(void* adr,Attribute* a,const CallbackParam* icp);
int set_spot_loc(void* adr,Attribute* a,const CallbackParam* cp);
int get_u32(char* base,char** a,uint16_t* idlist,int idlen);
int get_rect(char* base,char** a,uint16_t* idlist,int idlen);
int get_nestedlist(char* base,char** a,uint16_t* idlist,int idlen);

Attrs_t IcAttrs[]={
    {ATTR_TYPE_DWORD,XNInputStyle,IC_INPUT_STYLE,offsetof(IcAttributes,InputStyle),get_u32,set_input_style},
    {ATTR_TYPE_WINDOW,XNClientWindow,IC_CLIENT_WINDOW,offsetof(IcAttributes,ClientWindow),get_u32,set_u32},
    {ATTR_TYPE_WINDOW,XNFocusWindow,IC_FOCUS_WINDOW,offsetof(IcAttributes,FocusWindow),get_u32,set_u32},
    {ATTR_TYPE_DWORD,XNFilterEvents,IC_FILTER_EVENTS,offsetof(IcAttributes,FilterEvents),get_u32,set_u32},
    {ATTR_TYPE_STRCONV,XNStringConversion,IC_STRING_CONV,offsetof(IcAttributes,StrConv),NULL,set_strcnv},
    {ATTR_TYPE_DWORD,XNResetState,IC_RESET_STATE,offsetof(IcAttributes,ResetState),get_u32,set_u32},
    {ATTR_TYPE_DWORD,XNHotKeyState,IC_HOTKEY_STATE,offsetof(IcAttributes,HotkeyState),get_u32,set_u32},
    {ATTR_TYPE_NESTEDLIST,XNPreeditAttributes,IC_PREEDIT_ATTR,offsetof(IcAttributes,Preedit),get_nestedlist,set_nested_list},
    {ATTR_TYPE_NESTEDLIST,XNStatusAttributes,IC_STATUS_ATTR,offsetof(IcAttributes,StatusArea),get_nestedlist,set_nested_list},

    {ATTR_TYPE_RECTANGLE,XNArea,IC_AREA,offsetof(CommonAttr,Area),get_rect,set_rectangle},
    {ATTR_TYPE_RECTANGLE,XNAreaNeeded,IC_AREA_NEEDED,offsetof(CommonAttr,AreaNeeded),get_rect,set_rectangle},
    {ATTR_TYPE_DWORD,XNColormap,IC_COLOR_MAP,offsetof(CommonAttr,ColorMap),get_u32,set_u32},
    {ATTR_TYPE_DWORD,XNForeground,IC_FG,offsetof(CommonAttr,Foreground),get_u32,set_u32},
    {ATTR_TYPE_DWORD,XNBackground,IC_BG,offsetof(CommonAttr,Background),get_u32,set_u32},
    {ATTR_TYPE_DWORD,XNBackgroundPixmap,IC_BG_PIXMAP,offsetof(CommonAttr,BgPixmap),get_u32,set_u32},
    {ATTR_TYPE_FONTSET,XNFontSet,IC_FONTSET,offsetof(CommonAttr,FontSet),NULL,set_string_to_cstr},
    {ATTR_TYPE_WORD,XNLineSpace,IC_LINE_SPACE,offsetof(CommonAttr,LineSpace),NULL,set_u16_to_u32},
    {ATTR_TYPE_DWORD,XNCursor,IC_CURSOR,offsetof(CommonAttr,CursorId),get_u32,set_u32},

    {ATTR_TYPE_POINT,XNSpotLocation,IC_SPOT_LOC,offsetof(PreeditAttr,SpotLocation),NULL,set_spot_loc},
    {ATTR_TYPE_DWORD,XNPreeditState,IC_STATE,offsetof(PreeditAttr,State),get_u32,set_u32},

    {ATTR_TYPE_SEP,XNSeparatorofNestedList,IC_SEP,0,NULL,NULL},

    {0,NULL,0,0,NULL,NULL}
};

IcData* create_ic(WxContext* cx,XimCreateIc* pkt)
{
    int icn;
    IcData *icp;

    for(icn=0,icp=ArAdr(&cx->Ic); icn<ArUsing(&cx->Ic) && (icp->Flags&ICF_INVALID)==0; ++icn,++icp)
	;
    if(icn == ArUsing(&cx->Ic)) //空きなし
	icp = ArExpand(&cx->Ic,1);
    ++icn; //icidは1から
    memset(icp,0,sizeof(*icp));

    CallbackParam cp={icp,0,NULL};
    icp->CompFontHeight = -1;
    icp->Attrs.Defined |= set_ic_values(&icp->Attrs,pkt->attrs,pkt->sz,&cp);
    send_ww(cx->Client,XIM_CREATE_IC_REPLY,pkt->imid,icn);
    LOG("im-id=%hd sz=%hd --> ic-id %d\n",pkt->imid,pkt->sz,icn);

    XimSetEventMask hm={{0,0,0},
			pkt->imid,icn,	//imid,icid
			KeyPressMask/*|KeyReleaseMask*/, 	//forward
			KeyPressMask/*|KeyReleaseMask*/};	//sync
    send_n(cx->Client,XIM_SET_EVENT_MASK,&hm,sizeof(hm));
    return icp;
}

int CreateIc(WxContext* cx,XimCreateIc* pkt)
{
    int cxn = CannaCreateContext(); //先にlongjumpを起こさせる
    WimeShowToolbar(cxn,true,true);
    IcData *icp = create_ic(cx,pkt);
    icp->WimeCxn = cxn;
    LOG("wime-cxn %d\n",cxn);
    WimeRegXWindow(cxn,icp->Attrs.FocusWindow ?: icp->Attrs.ClientWindow);
    return 0;
}

int CreateIc_nwm(WxContext* cx,XimCreateIc* pkt)
{
    create_ic(cx,pkt);
    return 0;
}

void DestroyIcIf(WxContext* cx,XimImIc* pkt,bool send_reply,bool enable_wime)
{
    IcData *icp = ArElem(&cx->Ic,pkt->icid-1);
    icp->Flags |= ICF_INVALID;
    free(icp->Attrs.Preedit.Cmn.FontSet);
    free(icp->Attrs.StatusArea.Cmn.FontSet);
    LOG("im-id=%hd ic-id=%hd wimecxn=%d\n",pkt->imid,pkt->icid,icp->WimeCxn);
    if(send_reply)
	send_ww(cx->Client,XIM_DESTROY_IC_REPLY,pkt->imid,pkt->icid);

    CallbackParam cp={icp,cx->Client,pkt};
    icp->ConvFunc->Cleanup(&cp);

    if(enable_wime){
	int cxn = icp->WimeCxn;
	WimeShowToolbar(cxn,false,false);
	CannaCloseContext(cxn);
    }
}

int DestroyIc(WxContext* cx,XimImIc* pkt)
{
    DestroyIcIf(cx,pkt,true,true);
    return 0;
}

int DestroyIc_nwm(WxContext* cx,XimImIc* pkt)
{
    DestroyIcIf(cx,pkt,true,false);
    return 0;
}

int SetIcValues(WxContext* cx,XimSetIcValues* pkt)
{
    LOG("im-id=%hd ic-id=%hd\n",pkt->imid,pkt->icid);
    IcData *icp = ArElem(&cx->Ic,pkt->icid-1);
    CallbackParam cp={icp,0,(const XimImIc*)pkt};
    icp->Attrs.Defined |= set_ic_values(&icp->Attrs,pkt->attr,pkt->sz,&cp);
    send_ww(cx->Client,XIM_SET_IC_VALUES_REPLY,pkt->imid,pkt->icid);
    return 0;
}

/*
  処理したidの数を返す。
  途中にIC_SEPがあればそこで止まる。IC_SEPは戻り値に含まれない。
*/
int get_ic_values(char* base,char** buf,uint16_t* idlist,int idlen)
{
    int used,used_all=0;
    while(idlen>0 && *idlist!=IC_SEP){
	used = IcAttrs[*idlist].Getter(base,buf,idlist,idlen);
	idlist += used;
	idlen -= used;
	used_all += used;
    }
    return used_all;
}

int GetIcValues(WxContext* cx,XimGetIcValues* pkt)
{
    int idlen,bufsize;
    char *atbuf;

    LOG("im-id=%hd ic-id=%hd\n",pkt->imid,pkt->icid);
    IcData *icp = ArElem(&cx->Ic,pkt->icid-1);

    //属性リストの全体の大きさを求める
    idlen = pkt->sz/2;
    atbuf = (char*)sizeof(XimGetIcValuesReply);
    get_ic_values(NULL,&atbuf,pkt->atid,idlen);
    bufsize = (atbuf-(char*)0);

    //あらためて属性を取得する
    char buf[bufsize];
    XimGetIcValuesReply *r = (typeof(r))buf;
    atbuf = (char*) r->attr;
    get_ic_values((char*)&icp->Attrs,&atbuf,pkt->atid,idlen);

    r->imid = pkt->imid;
    r->icid = pkt->icid;
    r->size = bufsize - sizeof(*r);

    send_n(cx->Client,XIM_GET_IC_VALUES_REPLY,buf,bufsize);
    return 0;
}

int SetIcFocus(WxContext* cx,XimImIc* pkt)
{
    IcData *icp = ArElem(&cx->Ic,pkt->icid-1);
    LOG("im-id=%hd ic-id=%hd cxn=%d\n",pkt->imid,pkt->icid,icp->WimeCxn);
    WimeSetFocus(icp->WimeCxn,true);
    return 0;
}

int SetIcFocus_nwm(WxContext* cx UNUSED,XimImIc* pkt UNUSED)
{
    return 0;
}

int UnsetIcFocus(WxContext* cx,XimImIc* pkt)
{
    IcData *icp = ArElem(&cx->Ic,pkt->icid-1);
    LOG("im-id=%hd ic-id=%hd cxn=%d\n",pkt->imid,pkt->icid,icp->WimeCxn);
    WimeSetFocus(icp->WimeCxn,false);
    return 0;
}

int UnsetIcFocus_nwm(WxContext* cx UNUSED,XimImIc* pkt UNUSED)
{
    return 0;
}

//属性配列alからデータを取得して base+al->offset に保存する
//使われた属性のビットマスクを返す
int set_ic_values(void* base,Attribute* al,int sz,const CallbackParam* cp)
{
    int attr_sz,def=0;

    while(sz > 0){
	if(al->id >= ITEMS(IcAttrs)-1){
	    MSG("\tinvalid ic-id,%hd(0x%hx)\n",al->id,al->id);
	    break;
	}
	def |= IcAttrs[al->id].Setter((char*)base+IcAttrs[al->id].Offset,al,cp);
	attr_sz = sizeof(*al) + al->sz + Pad(al->sz);
	al = (Attribute*)((char*)al + attr_sz);
	sz -= attr_sz;
    }
    return def;
}

int set_rectangle(void* adr,Attribute* a,const CallbackParam* cp UNUSED)
{
    *(XRectangle*)adr = *(XRectangle*)(a->value);

    LOG("\t%s:value=(%hd,%hd)-%hdx%hd\n",IcAttrs[a->id].Name,((XRectangle*)adr)->x,((XRectangle*)adr)->y,((XRectangle*)adr)->width,((XRectangle*)adr)->height);
    return 1<<IcAttrs[a->id].Number;
}

int set_u32(void* adr,Attribute* a,const CallbackParam* cp UNUSED)
{
    *(uint32_t*)adr = *(uint32_t*)(a->value);
    LOG("\t%s:value=%d(0x%x)\n",IcAttrs[a->id].Name,*(uint32_t*)adr,*(uint32_t*)adr);
    return 1<<IcAttrs[a->id].Number;
}

int set_u16_to_u32(void* adr,Attribute* a,const CallbackParam* cp UNUSED)
{
    *(uint32_t*)adr = *(uint16_t*)(a->value);
    LOG("\t%s:value=%d(0x%x)\n",IcAttrs[a->id].Name,*(uint32_t*)adr,*(uint32_t*)adr);
    return 1<<IcAttrs[a->id].Number;
}

int set_string_to_cstr(void* adr,Attribute* a,const CallbackParam* cp UNUSED)
{
    String *src = (String*)(a->value);
    *((char*)memcpy(*(void**)adr=malloc(src->sz+1),src->str,src->sz) + src->sz) = 0;
    LOG("\t%s:value=%s\n",IcAttrs[a->id].Name,*(char**)adr);
    return 1<<IcAttrs[a->id].Number;
}

int set_xpoint(void* adr,Attribute* a,const CallbackParam* cp UNUSED)
{
    *(XPoint*)adr = *(XPoint*)(a->value);
    LOG("\t%s:value=(%hd,%hd)\n",IcAttrs[a->id].Name,((XPoint*)adr)->x,((XPoint*)adr)->y);
    return 1<<IcAttrs[a->id].Number;
}

int set_nested_list(void* adr,Attribute* a,const CallbackParam* p)
{
    LOG("\t%s:nested list...\n",IcAttrs[a->id].Name);
    int r = (1<<IcAttrs[a->id].Number)|set_ic_values(adr,(Attribute*)(a->value),a->sz,p);
    LOG("\tend\n");
    return r;
}

int set_strcnv(void* adr,Attribute* a,const CallbackParam* cp UNUSED)
{
    XimStrConvText *s = (XimStrConvText*)(a->value);
    XIMStringConversionText *d = (XIMStringConversionText*)adr;

    free(d->string.mbs);
    d->length = s->Size;
    *((char*)memcpy(d->string.mbs = malloc(d->length+1),s->Str,d->length) + d->length) = 0;
    //feedbackは今のところ無視する

    LOG("\t%s:value=%s\n",IcAttrs[a->id].Name,d->string.mbs);
    return 1<<IcAttrs[a->id].Number;
}

/*
  base=IcData.Attrsの先頭位置
  a=バッファポインタのポインタ。必要な大きさ分加算される
  idlist=渡されたidの配列。先頭がこの関数のidとなる。
  idlen=idlistの数
  戻り値：使用したidlistの数。普通１だが、nestedlistのときは１以上になる。
  base==NULLのときは値を書き込まない
*/
int get_u32(char* base,char** a,uint16_t* idlist,int idlen UNUSED)
{
    if(base != NULL){
	Attribute *at = (Attribute*)*a;
	at->id = *idlist;
	at->sz = 4;
	*(uint32_t*)(at->value) = *(uint32_t*)(base+IcAttrs[*idlist].Offset);
	LOG("\t%s:value=%d(0x%x)\n",IcAttrs[*idlist].Name,*(uint32_t*)(at->value),*(uint32_t*)(at->value));
    }
    *a += sizeof(Attribute) + 4 + Pad(4);
    return 1;
}

int get_rect(char* base,char** a,uint16_t* idlist,int idlen UNUSED)
{
    if(base != NULL){
	Attribute *at = (Attribute*)*a;
	at->id = *idlist;
	at->sz = sizeof(XRectangle);
	XRectangle *r = (XRectangle*)(at->value);
	*r = *(XRectangle*)(base+IcAttrs[*idlist].Offset);
	LOG("\t%s:value=(%hd,%hd)-%hdx%hd\n",IcAttrs[*idlist].Name,r->x,r->y,r->width,r->height);
    }
    *a += sizeof(Attribute)+sizeof(XRectangle)+Pad(sizeof(XRectangle));
    return 1;
}

int get_nestedlist(char* base,char** a,uint16_t* idlist,int idlen)
{
    Attribute *a_top;
    int used,valsz,pad;

    if(base!=NULL){
	LOG("\t%s\n",IcAttrs[*idlist].Name);
	base += IcAttrs[*idlist].Offset;
    }
    a_top = (Attribute*)*a;
    *a += sizeof(Attribute);
    used = get_ic_values(base,a,idlist+1,idlen-1) +1/*自分の分*/;
    valsz = *a - (char*)(a_top+1);
    pad = Pad(valsz);
    valsz += pad;
    *a += pad;
    if(base != NULL){
	a_top->id = *idlist;
	a_top->sz = valsz;
    }
    if(idlen!=used && idlist[used]==IC_SEP)
	++used;
    return used;
}

int set_input_style(void* adr,Attribute* a,const CallbackParam* p)
{
    extern ConvCallbackFuncs ConvFuncOverTheSpot,ConvFuncOnTheSpot,
	ConvFuncOffTheSpot,ConvFuncRootInput;
    int r = set_u32(adr,a,p);

    switch(*(uint32_t*)adr & 0xff){
    case XIMPreeditPosition:
	p->Ic->ConvFunc = &ConvFuncOverTheSpot;
	LOG("select over-the-spot\n");
	break;
    case XIMPreeditCallbacks:
	p->Ic->ConvFunc = &ConvFuncOnTheSpot;
	LOG("select on-the-spot\n");
	break;
    case XIMPreeditArea:
	p->Ic->ConvFunc = &ConvFuncOffTheSpot;
	LOG("select off-the-spot\n");
	break;
    case XIMPreeditNothing:
	p->Ic->ConvFunc = &ConvFuncRootInput;
	LOG("select root-input\n");
	break;
    default:
	MSG("unsupported preedit style %x\n",*(uint32_t*)adr & 0xff);
    }

    return r;
}

//overthespot.c参照
int set_spot_loc(void* adr,Attribute* a,const CallbackParam* cp)
{
    int r = set_xpoint(adr,a,cp);

    /*
      ConvFunc->SetSpotLoc()でwimeを呼びだすためlongjmpしないように接続を確認する。
      !!! SetIcValuesも未接続用の関数を用意するか？
    */
    if(cp->Ic->ConvFunc!=NULL && WimeIsConnected())
	cp->Ic->ConvFunc->SetSpotLoc(cp,adr);
    return r;
}

/*
  on-the-spotで入力途中にフォーカスを変えると飛んでくる。
*/
int ResetIc(WxContext* cx,XimImIc* pkt)
{
    char *ps,*ct,*buf;
    XimResetIcReply *r;
    int rsize,ctlen;
    WimeCompStrInfo si;
    IcData *icp = ArElem(&cx->Ic,pkt->icid-1);

    LOG("im-id=%hd ic-id=%hd cxn=%d\n",pkt->imid,pkt->icid,icp->WimeCxn);
    ct = EucjpToCtext(ps = WimeGetCompStr(icp->WimeCxn,&si));
    ctlen = ct!=NULL ? strlen(ct) : 0;
    r = (XimResetIcReply*)(buf = malloc(rsize = sizeof(XimResetIcReply)+ctlen+Pad(ctlen+2)));
    r->imid = pkt->imid;
    r->icid = pkt->icid;
    r->len = ctlen;
    memcpy(r->str,ct,ctlen);
    send_n(cx->Client,XIM_RESET_IC_REPLY,r,rsize);
    LOG("\tpreedit string:'%s'\n",ps);

    free(ps);
    free(ct);
    free(buf);
    return 0;
}
