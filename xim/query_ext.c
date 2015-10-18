// -*- coding:euc-jp -*-
#include "wimexim.h"
#include <stdlib.h>
#include <string.h>

void dbg_query_ext(XimQueryExtension* pkt)
{
    Array a;
    Str *s=pkt->ext,*e=(Str*)((char*)s+pkt->sz);
    char *buf=NULL;
    ArNew(&a,1,NULL);
    while(s < e){
	buf = memcpy(realloc(buf,s->len+1),s->str,s->len);
	buf[s->len] = 0;
	ArPrint(&a," %s",buf);
	s = IncStr(s);
    }
    MSG("im-id=%hd n=%hd ext=%s\n",pkt->imid,pkt->sz,ArAdr(&a));
    ArDelete(&a);
    free(buf);
}

int QueryExtension(WxContext* cx,XimQueryExtension* pkt)
{
    struct{
	unsigned major;
	char *name;
    } ext[]={
	{XIM_EXT_SET_EVENT_MASK,"XIM_EXT_SET_EVENT_MASK"}
    };

    VERBOSE(dbg_query_ext(pkt));

    Array ind; //extの番号の配列
    ArNew(&ind,sizeof(int),NULL);
    if(pkt->sz == 0){
	//全部送る
	int *p = ArAlloc(&ind,ITEMS(ext));
	for(unsigned items=0; items<ITEMS(ext); ++items)
	    *(p++) = items;
    }else{
	//指定されたものがあればそれだけ送る
	Str *s=pkt->ext,*e=(Str*)((char*)s+pkt->sz);
	Array buf;
	ArNew(&buf,1,NULL);
	while(s < e){
	    char* bp = memcpy(ArAlloc(&buf,s->len+1),s->str,s->len);
	    bp[s->len] = 0;
	    for(unsigned num=0; num<ITEMS(ext); ++num){
		if(strcmp(bp,ext[num].name) == 0){
		    *(int*)ArExpand(&ind,1) = num;
		    break;
		}
	    }
	    s = IncStr(s);
	}
	ArDelete(&buf);
    }

    //送るデータの大きさを計算
    int totalsize = sizeof(XimQueryExtensionReply);
    for(int *ip=ArAdr(&ind),n=0; n<ArUsing(&ind); ++ip,++n){
	int namelen = strlen(ext[*ip].name);
	totalsize += sizeof(Ext)+namelen+Pad(namelen);
    }

    //データを作る
    XimQueryExtensionReply* d = memset(malloc(totalsize),0,totalsize);
    d->imid = pkt->imid;
    d->len = totalsize-sizeof(XimQueryExtensionReply);
    Ext *el = d->ext;
    for(int *ip=ArAdr(&ind),n=0; n<ArUsing(&ind); ++n,++ip){
	el->major = (ext[*ip].major & 0xff);
	el->minor = (ext[*ip].major >> 8);
	el->len = strlen(ext[*ip].name);
	memcpy(el->name,ext[*ip].name,el->len);
	LOG("major=%hhu minor=%hhu name=%s\n",el->major,el->minor,el->name);
	el = (Ext*)((char*)el + sizeof(Ext)+el->len+Pad(el->len));
    }

    send_n(cx->Client,XIM_QUERY_EXTENSION_REPLY,d,totalsize);
    ArDelete(&ind);
    free(d);
    return 0;
}

//(C) 2009 thomas
