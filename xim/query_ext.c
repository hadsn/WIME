// -*- coding:euc-jp -*-
#include "wimexim.h"
#include "lib/log.h"
#include "lib/ut.h"
#include <stdlib.h>
#include <string.h>

void dbg_query_ext(const XimQueryExtension* pkt)
{
    Array a;
    ArNew(&a,1,NULL);
    const Str *s=pkt->ext, *e=(Str*)((char*)s+pkt->sz);
    while(s < e){
	ArPrint(&a," %.*s",(int)(s->len),s->str);
	s = IncStr(s);
    }
    MESG("im-id=%hd n=%hd ext=%s\n",pkt->imid,pkt->sz,(char*)ArAdr(&a));
    ArDelete(&a);
}

int QueryExtension(WxContext* cx,XimQueryExtension* pkt)
{
    struct ext{
	unsigned major;
	char *name;
    } ext[]={
	{XIM_EXT_SET_EVENT_MASK,"XIM_EXT_SET_EVENT_MASK"},
	{XIM_EXT_FORWARD_KEYEVENT,"XIM_EXT_FORWARD_KEYEVENT"},
	{XIM_EXT_MOVE,"XIM_EXT_MOVE"},
    };

    DEBUGDO(CH_XIM,dbg_query_ext(pkt));

    Array ind; //extの番号の配列
    ArNew(&ind,sizeof(int),NULL);
    if(pkt->sz == 0){
	//全部送る
	int *p = ArAlloc(&ind,ITEMS(ext));
	for(unsigned items=0; items<ITEMS(ext); ++items)
	    *(p++) = items;
    }else{
	//指定されたものがあればそれだけ送る
	Array buf;
	ArNew(&buf,1,NULL);
	Str *s=pkt->ext, *e=(Str*)((char*)s+pkt->sz);
	while(s < e){
	    ArAddChar(ArAddN(&buf,s->str,s->len),0);
	    for(unsigned num=0; num<ITEMS(ext); ++num){
		if(strcmp(ArAdr(&buf),ext[num].name) == 0){
		    ArAdd1(&ind,&num);
		}
	    }
	    ArClear(&buf);
	    s = IncStr(s);
	}
	ArDelete(&buf);
    }

    //送るデータの大きさを計算
    int totalsize = sizeof(XimQueryExtensionReply);
    for(int n=0; n<ArUsing(&ind); ++n){
	int namelen = strlen(ext[ARVAL(int,&ind,n)].name);
	totalsize += sizeof(Ext)+namelen+Pad(namelen);
    }

    //データを作る
    XimQueryExtensionReply* d = calloc(totalsize,1);
    d->imid = pkt->imid;
    d->len = totalsize-sizeof(XimQueryExtensionReply);
    Ext* el = d->ext;
    for(int n=0; n<ArUsing(&ind); ++n){
	struct ext* src = ext+ARVAL(int,&ind,n);
	el->major = (src->major & 0xff);
	el->minor = (src->major >> 8);
	el->len = strlen(src->name);
	memcpy(el->name,src->name,el->len);
	DEBUGLOG(CH_XIM,"major=%hhu minor=%hhu name=%s\n",el->major,el->minor,src->name);
	el = (Ext*)((char*)el + sizeof(Ext)+el->len+Pad(el->len));
    }

    SendN(cx->Client,XIM_QUERY_EXTENSION_REPLY,d,totalsize);
    ArDelete(&ind);
    free(d);
    return 0;
}

//(C) 2009 thomas
