// -*- coding:euc-jp -*-
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "array.h"

#define DEFAULT_WSPAGESIZE 128

Array* ArNew(Array* ws,int bs,ArNewCtr ct)
{
    if(ws == NULL)
	ws = malloc(sizeof(Array));
    if(ws){
	ws->adr = NULL;
	ws->blocksize = bs;
	ws->use = ws->reserve = 0;
	ws->pagesize = DEFAULT_WSPAGESIZE;
	ws->constructor = ct;
    }
    return ws;
}

Array* ArNewPs(Array* ws,int bs,ArNewCtr ct,int pagesize)
{
    if((ws = ArNew(ws,bs,ct)) != NULL)
	ws->pagesize = pagesize;
    return ws;
}

Array* ArDelete(Array* ws)
{
    if(ws){
	if(!ws->adr)
	    free(ws->adr);
	ws = ArNewPs(ws,ws->blocksize,ws->constructor,ws->pagesize);
    }
    return ws;
}

Array* ArClear(Array* ws)
{
    if(ws)
	ws->use = 0;
    return ws;
}

Array* ArReserve(Array* ws,int count)
{
    if(ws==NULL)
	return NULL;
    
    void* newadr = ws->adr;
    if(count > ws->reserve){
	//[180320]pagesize==1のときはいつも1ブロック余計に確保される。放っておくか？
	newadr = realloc(ws->adr,(ws->reserve=(count/ws->pagesize+1)*ws->pagesize)*ws->blocksize);
    }
    if(newadr == NULL)
	return NULL;

    ws->adr = newadr;
    if(ws->constructor != NULL){
	int n;
	char* p;
	for(n=ws->use,p=ArElemNc(ws,n); n<ws->reserve; ++n,p+=ws->blocksize)
	    ws->constructor(p);
    }
    return ws;
}

void* ArAlloc(Array* ws,int count)
{
    void* bufadr = NULL;
    if(ArReserve(ws,count) != NULL){
	bufadr = ws->adr;
	ws->use = count;
    }
    return bufadr;
}

void* ArExpand(Array* ws,int count)
{
    void* elemp = NULL;
    if(ws!=NULL && ArReserve(ws,ws->use+count) != NULL){
	elemp = ArElemNc(ws,ws->use);
	ws->use += count;
    }
    return elemp;
}

Array* ArRemove(Array* ws,int pos,int count)
{
    if(ws && pos>=0 && pos<ws->use){
	if(ws->use <= pos+count) //オーバーフロー→pos以降をすべて削除
	    ws->use = pos; //useをposまでのメンバにする。データの移動はしない。
	else{
	    char* ad = ArElemNc(ws,pos);
	    memmove(ad,ad+count*ws->blocksize,(ws->use-pos-count)*ws->blocksize);
	    ws->use -= count;
	}
    }
    return ws;
}

int ArForEach(Array* ws,ArForEachFunc func,void* arg)
{
    int pos=-1;
    if(ws){
	char* obj = ws->adr;
	for(pos=0; pos<ws->use; ++pos,obj+=ws->blocksize)
	    if(func(obj,arg))
		break;
    }
    return pos;
}

int ArFindIf(const Array* ws,int start,ArFindFunc eq,const void* val)
{
    int st=-1;
    if(ws){
	for(char* ep=ws->adr; start<ws->use; ++start,ep+=ws->blocksize)
	    if(eq(ep,val)){
		st = start;
		break;
	    }
    }
    return st;
}

int ArFind(const Array* ws,int pos,const void* val)
{
    int index = -1;
    if(ws){
	for(char* src=ArElemNc(ws,pos); pos<ws->use; src+=ws->blocksize,++pos){
	    if(memcmp(src,val,ws->blocksize) == 0)
		break;
	}
	index = pos<ws->use ? pos : -1;
    }
    return index;
}

void* ArFindElemIf(Array* ws,int start,ArFindFunc eq,const void* val)
{
    int pos = ArFindIf(ws,start,eq,val);
    return pos<0 ? ArExpand(ws,1) : ArElemNc(ws,pos);
}

Array* ArAddN(Array* ws,const void* valptr,size_t count)
{
    void* adr = ArExpand(ws,count);
    if(!adr)
	return NULL;
    memcpy(adr,valptr,count*ws->blocksize);
    return ws;
}

Array* ArAddAr(Array* wa,const Array* wb)
{
    void* adr = ArExpand(wa,wb->use);
    if(!adr)
	return NULL;
    memcpy(adr,wb->adr,ArUsingBytes(wb));
    return wa;
}

Array* ArCopy(Array* dst,const Array* src)
{
    if(dst==NULL || src==NULL)
	return NULL;
    
    dst->reserve = (dst->reserve*dst->blocksize)/src->blocksize;
    dst->blocksize = src->blocksize;
    dst->pagesize = src->pagesize;
    dst->constructor = src->constructor;
    dst->use = 0;
    void* adr = ArAlloc(dst,src->use);
    if(!adr)
	return NULL;
    memcpy(adr,src->adr,src->use*src->blocksize);
    return dst;
}

Array* ArInsert(Array* ws,int pos,int count,const void* valptr)
{
    if(!ArExpand(ws,count))
	return NULL;
    memmove(ArElemNc(ws,pos+count),ArElemNc(ws,pos),(ws->use-pos)*ws->blocksize);
    memcpy(ArElemNc(ws,pos),valptr,count*ws->blocksize);
    return ws;
}

int ArIndex(Array* ws,const void* ptr)
{
    return ws ? (((char*)ptr - (char*)ws->adr)/ws->blocksize) : -1;
}

Array* ArDec(Array* ws)
{
    if(ws && ws->use>0)
	-- ws->use;
    return ws;
}

Array* ArPrintV(Array* ws,const char* fmt,va_list vl)
{
    if(ws){
	int wrote,fz;
	va_list ovl;

	//ヌル文字があれば削る。
	if(ArUsing(ws)>0 && *(char*)ArElemNc(ws,ArUsing(ws)-1)==0)
	    ArDec(ws);
	
	va_copy(ovl,vl);
	fz = ws->reserve - ws->use; //空いている大きさ
	if((wrote = vsnprintf(ArElemNc(ws,ws->use),fz,fmt,vl)) >= fz){
	    ArReserve(ws,ws->reserve+ wrote-fz +1/*足りない大きさ(ヌル文字分を含める)*/);
	    fz = ws->reserve - ws->use; //空いている大きさ
	    wrote = vsnprintf(ArElemNc(ws,ws->use),fz,fmt,ovl);
	}
	ws->use += wrote+1;
	va_end(ovl);
    }
    return ws;
}
Array* ArPrint(Array* ws,const char* fmt,...)
{
    va_list vl;

    va_start(vl,fmt);
    ws = ArPrintV(ws,fmt,vl);
    va_end(vl);
    return ws;
}

Array* ArSwap(Array* a,Array* b)
{
    if(a==NULL || b==NULL)
	return NULL;
    Array c = *a;
    *a = *b;
    *b = c;
    return a;
}

int ArEq(Array* wa,Array* wb)
{
    if(wa==NULL || wb==NULL)
	return 0;
    return wa->blocksize == wb->blocksize &&
	wa->use == wb->use &&
	memcmp(wa->adr,wb->adr,wa->blocksize*wa->use) == 0;
}

Array* ArBuf(Array* ws,ArBufFunc func,void* data)
{
    if(ws){
	int blk = 1;
	do{
	    if(ArReserve(ws,blk) == NULL){
		ws = NULL;
		break;
	    }
	}while((blk = func(ArAdr(ws),ws->reserve,data)) > 0);
    }
    return ws;
}

//(C) 2008 thomas
