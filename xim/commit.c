#include <string.h>
#include <iconv.h>
#include <stdlib.h>
#include "wimexim.h"

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	icid;
    uint16_t	flags;
}__attribute__((packed)) XimCommit;

typedef struct{
    uint16_t	len;
    char	str[];
    //char	pad[];
}__attribute__((packed)) XimCommitChar;

#define COMMIT_SYNC	1
#define COMMIT_CHAR	2
#define COMMIT_SYM	4

void dbg_commit(uint16_t imid,uint16_t icid,const char* ej,const char* ct)
{
    Array a;
    ArNew(&a,1,NULL);
    MSG("im=%hd ic=%hd\n",imid,icid);
    MSG("raw:%s\n",ArAdr(Dump1(" %02x",ej,strlen(ej),&a)));
    ArClear(&a);
    MSG("ctext:%s\n",ArAdr(Dump1(" %02x",ct,strlen(ct),&a)));
    ArDelete(&a);
}

void CommitChar(Window client,uint16_t imid,uint16_t icid,const char* ch)
{
    XimCommit *base;
    XimCommitChar *cm;
    char *ct = EucjpToCtext(ch);
    int ctlen = strlen(ct);

    VERBOSE(dbg_commit(imid,icid,ch,ct));
    int bufsize = sizeof(XimCommit)+sizeof(XimCommitChar)+ctlen+Pad(ctlen);
    char pktbuf[bufsize];

    memset(pktbuf,0,bufsize);
    base = (XimCommit*)pktbuf;
    cm = (XimCommitChar*)(base+1);

    base->imid = imid;
    base->icid = icid;
    base->flags = COMMIT_CHAR;
    memcpy(cm->str,ct,cm->len=ctlen);
    send_n(client,XIM_COMMIT,pktbuf,bufsize);

    free(ct);
}

void append_sq(Array* a,const char** mode,const char* s)
{
    if(*mode != s){
	unsigned sz = strlen(s);
	memcpy(ArExpand(a,sz),s,sz);
    }
    *mode = s;
}

/*
  euc-jp --> compound_text
  mallocで確保した領域を返す
*/
char* EucjpToCtext(const char* ej)
{
    const char ISO8859_L[]	= "\x1b\x28\x42";
    //const char ISO8859_R[]	= "\x1b\x2d\x41";
    const char JX0201[]		= "\x1b\x29\x49";	//right half
    const char JX0208[]		= "\x1b\x24\x29\x42";
    const char JX0212[]		= "\x1b\x24\x29\x44";
    const char *mode_l,*mode_r;
    unsigned char ejc;
    Array ct;

    if(ej == NULL)
	return NULL;

    ArNew(&ct,1,NULL);
    ArReserve(&ct,strlen(ej)+20); //制御コード分いくらか多めに確保する

    mode_l = mode_r = ISO8859_L;
    while((ejc = *(ej++)) != 0){
	switch(ejc){
	case 0 ... 0x7f: //ascii
	    append_sq(&ct,&mode_l,ISO8859_L);
	    *(char*)ArExpand(&ct,1) = ejc;
	    break;
	case 0x8e: //半角カナ
	    append_sq(&ct,&mode_r,JX0201);
	    *(char*)ArExpand(&ct,1) = *(ej++);
	    break;
	case 0x8f: //補助漢字
	    append_sq(&ct,&mode_r,JX0212);
	    *(uint16_t*)ArExpand(&ct,2) = *(uint16_t*)ej;
	    ej += 2;
	    break;
	default: //漢字
	    append_sq(&ct,&mode_r,JX0208);
	    *(uint16_t*)ArExpand(&ct,2) = *(uint16_t*)(ej-1);
	    ++ej;
	}
    }

    //append_sq(&ct,&mode_l,ISO8859_L);
    //append_sq(&ct,&mode_r,ISO8859_R);

    *(char*)ArExpand(&ct,1) = 0;
    return ArAdr(&ct); //Arrayのバッファを解放せずにそのまま渡す
}

//(C) 2009 thomas
