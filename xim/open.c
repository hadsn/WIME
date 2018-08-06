// -*- coding:euc-jp -*-
#include "wimexim.h"
#include "lib/log.h"
#include <string.h>
#include <stdlib.h>

extern Attrs_t ImAttrs[];	//im.c
extern Attrs_t IcAttrs[];	//ic.c
extern Array ContextList;

int RegTriggerKeys(WxContext* cx);

/*
  Connectとごっちゃになっている。さらに,実際のcxの確保はPreconnectで行われている。
  整理しなければならない。
  ??? 複数のOpenはあり得るのか？ 今のところ１つの接続に１つのopenしか考えていない。
  im-idという数値があるんだから、複数オープンされると考えるべきか？
*/
int Open(WxContext* cx,XimOpen* pkt)
{
    DEBUGLOG(CH_XIM,"locale='%s'\n",pkt->str);

    Attrs_t* attrs[]={ImAttrs,IcAttrs};
    int attr_sz[2];

    for(int a=0; a<2; ++a){
	attr_sz[a] = 0;
	for(int n=0; attrs[a][n].Name!=NULL; ++n){
	    int nlen = strlen(attrs[a][n].Name);
	    attr_sz[a] += sizeof(XimAttr)+nlen+Pad(2+nlen);
	}
    }

    int totalsize = sizeof(XimHeader)+ 2+2+attr_sz[0]+2+2+attr_sz[1];
    XimHeader* h = calloc(totalsize,1);

    uint16_t* wptr = (uint16_t*)(h+1);
    *(wptr++) = ArIndex(&ContextList,cx)+1; //im-id(１以上にする)

    for(int a=0; a<2; ++a){
	*(wptr++) = attr_sz[a];
	wptr += a; //ic-attrのバイト数の次の２バイトは空き
	XimAttr* xa = (XimAttr*)wptr;

	for(int n=0; attrs[a][n].Name!=NULL; ++n){
	    xa->id = attrs[a][n].Number;
	    xa->type = attrs[a][n].Type;
	    xa->len = strlen(attrs[a][n].Name);
	    memcpy(xa->attr,attrs[a][n].Name,xa->len);
	    xa = (XimAttr*)((char*)xa + sizeof(XimAttr)+xa->len+Pad(2+xa->len));
	}
	wptr = (uint16_t*)xa;
    }

    SendN(cx->Client,XIM_OPEN_REPLY,h,totalsize);
    free(h);

    //RegTriggerKeys(cx);

    return 0;
}

/*
  ??? disconnectとはどう違うだろう？
*/
int Close(WxContext* cx,XimClose* pkt)
{
    DEBUGLOG(CH_XIM,"im-id=%hd\n",pkt->imid);
    cx->Flags |= IMF_CLOSE;
    SendW(cx->Client,XIM_CLOSE_REPLY,pkt->imid,0);
    return 0;
}

//(C) 2009 thomas
