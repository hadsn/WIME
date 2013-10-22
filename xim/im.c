#include "wimexim.h"
#include <string.h>
#include <stdlib.h>
#include "so/xres.h"
#include <ctype.h>

extern Display* Disp;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	sz;
    Attribute attr[];
}__attribute__((packed)) XimGetImValuesReply;

int get_input_styles(char* base,char** a,uint16_t* idlist,int idlen);

Attrs_t ImAttrs[]={
    {ATTR_TYPE_STYLES,XNQueryInputStyle,IM_INPUT_STYLE,0,get_input_styles,NULL},
    {0,NULL,0,0,NULL,NULL}
};

void dbg_get_im_vals(XimGetImValues* pkt)
{
    Array a;
    ArNew(&a,1,NULL);
    for(int n=0; n<pkt->sz/2; ++n)
	ArPrint(&a,"[%hd]",pkt->id[n]);
    MSG("im-id=%hd id=%s\n",pkt->imid,ArAdr(&a));
    ArDelete(&a);
}

/*
  ic.cのコピー。ImAttrsとIcAttrsが違うだけなんだが,そのために更に引数を増やすか？
*/
int get_im_values(char* base,char** buf,uint16_t* idlist,int idlen)
{
    int used_all=0;
    while(idlen>0 && *idlist!=IC_SEP){
	int used = ImAttrs[*idlist].Getter(base,buf,idlist,idlen);
	idlist += used;
	idlen -= used;
	used_all += used;
    }
    return used_all;
}

int GetImValues(WxContext* cx,XimGetImValues* pkt)
{
    VERBOSE(dbg_get_im_vals(pkt));

    char *abuf;
    int bufsize,idlen = pkt->sz/2;

    //応答データの大きさを計算
    abuf = (char*)sizeof(XimGetImValuesReply);
    get_im_values(NULL,&abuf,pkt->id,idlen);
    bufsize = abuf-(char*)0;

    //実際にデータを作る
    char buf[bufsize];
    XimGetImValuesReply *r = (typeof(r))buf;
    abuf = (char*) r->attr;
    get_im_values(abuf/*NULL以外の値*/,&abuf,pkt->id,idlen);

    r->imid = pkt->imid;
    r->sz = bufsize - sizeof(*r);

    send_n(cx->Client,XIM_GET_IM_VALUES_REPLY,buf,bufsize);
    return 0;
}

//入力スタイルの一覧を返す
int get_input_styles(char* base,char** a,uint16_t* idlist,int idlen UNUSED)
{
    struct {
	int Type;
	const char* Name;
    } dis_sty_info[] = {
	{XIMPreeditPosition,"overthespot"},
	{XIMPreeditCallbacks,"onthespot"},
	{XIMPreeditArea,"offthespot"},
	{XIMPreeditNothing,"rootwindow"}
    };
    uint32_t styles[4/*エディットスタイル*/ * 3/*ステータス*/];
    uint32_t* stybufp = styles;

    /* 当面この機能は無効にする */
    const char* dis_sty_orig = NULL; //GetResource(Disp,XResDisableSty);
    char* dis_sty = strdup(dis_sty_orig!=NULL ? dis_sty_orig : "");
    char logstr[20*4]; //スタイル名の最大長(20くらいあれば十分だろう)*4

    //小文字に変換、'-','_'は削除
    char* p = dis_sty;
    while(*p!=0){
	if(*p=='-' || *p=='_'){
	    strcpy(p,p+1);
	    continue;
	}
	*p = tolower(*p);
	++p;
    }

    //dis_styにないスタイルを選ぶ
    logstr[0] = 0;
    for(int n=0; n<ITEMS(dis_sty_info); ++n){
	if(strstr(dis_sty,dis_sty_info[n].Name) == NULL){
	    *(stybufp++) = dis_sty_info[n].Type|XIMStatusNothing;
	    *(stybufp++) = dis_sty_info[n].Type|XIMStatusNone;
	    *(stybufp++) = dis_sty_info[n].Type|XIMStatusArea;
	    if(logstr[0] != 0)
		strcat(logstr,",");
	    strcat(logstr,dis_sty_info[n].Name);
	}
    }
    if(base!=NULL)
	LOG("%s\n",logstr);
    free(dis_sty);

    int styles_num = stybufp-styles;
    int styles_size = styles_num*sizeof(styles[0]);

    //padは必要ない
    if(base != NULL){
	Attribute *at = (Attribute*)*a;
	Styles *s = (Styles*)(at->value);
	at->id = *idlist;
	at->sz = sizeof(Styles)+styles_size;
	s->count = styles_num;
	memcpy(s->styles,styles,styles_size);
    }
    *a += sizeof(Attribute)+sizeof(Styles)+styles_size;
    return 1;
}

//(C) 2009 thomas
