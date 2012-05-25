#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "array.h"

#define DEFAULT_WSPAGESIZE 128

Array* ArNew(Array* ws,int bs,ArNewCtr ct)
{
    ws->adr = NULL;
    ws->blocksize = bs;
    ws->use = ws->reserve = 0;
    ws->pagesize = DEFAULT_WSPAGESIZE;
    ws->constructor = ct;
    return ws;
}

Array* ArNewPs(Array* ws,int bs,ArNewCtr ct,int pagesize)
{
    ArNew(ws,bs,ct)->pagesize = pagesize;
    return ws;
}

Array* ArDelete(Array* ws)
{
    if(!ws->adr)
	free(ws->adr);
    return ArNew(ws,ws->blocksize,ws->constructor);
}

Array* ArClear(Array* ws)
{
    ws->use = 0;
    return ws;
}

/*
  最低n個確保する。
  メモリを確保するだけで、useは変更しない。
  追加確保したセルはconstructorが呼ばれる。
*/
Array* ArReserve(Array* ws,int count)
{
    if(count > ws->reserve){
	ws->adr = realloc(ws->adr,(ws->reserve=(count/ws->pagesize+1)*ws->pagesize)*ws->blocksize);
    }
    if(ws->constructor != NULL){
	int n;
	char *p;
	for(n=ws->use,p=ArElem(ws,n); n<ws->reserve; ++n,p+=ws->blocksize)
	    ws->constructor(p);
    }
    return ws;
}

//n個確保する
//先頭を返す
void* ArAlloc(Array* a,int n)
{
    ArReserve(a,n);
    a->use = n;
    return a->adr;
}

//n個追加使用する
//確保分の先頭を返す
void* ArExpand(Array* ws,int n)
{
    void *p;
    ArReserve(ws,ws->use+n);
    p = ArElem(ws,ws->use);
    ws->use += n;
    return p;
}

void* ArElem(const Array* ws,int n)
{
    return (char*)(ws->adr)+n*ws->blocksize;
}

//内容が*pと等しい要素のインデックスを返す
//n=検索開始インデックス
int ArFind(const Array* ws,int n,const void* p)
{
    char* src;

    for(src=ArElem(ws,n); n<ws->use; src+=ws->blocksize,++n){
	if(memcmp(src,p,ws->blocksize) == 0)
	    break;
    }
    return n<ws->use ? n : -1;
}

Array* ArRemove(Array* ws,int pos)
{
    if(pos >= 0){
	char *ad = ArElem(ws,pos);
	memcpy(ad,ad+ws->blocksize,(ws->use-pos-1)*ws->blocksize);
	-- ws->use;
    }
    return ws;
}

//eq(val,要素アドレス)が真になる要素の番号を返す
//見つからないとき-1を返す
int ArFindIf(const Array* a,int start,ArFindFunc eq,const void* val)
{
    int st=-1;
    for(char *ep=a->adr; start<a->use; ++start,ep+=a->blocksize)
	if(eq(ep,val)){
	    st = start;
	    break;
	}
    return st;
}

//eq(val,要素アドレス)が真になる要素のアドレスを返す
//見つからないときはArExpandを呼び出す
void* ArFindElemIf(Array* a,int start,ArFindFunc eq,const void* val)
{
    int n = ArFindIf(a,start,eq,val);
    return n<0 ? ArExpand(a,1) : ArElem(a,n);
}

/*
  funcが偽を返したら終了する。終了したときの要素番号を返す。
*/
int ArForEach(Array* a,ArForEachFunc func,void* arg)
{
    int n;
    char* p = a->adr;
    for(n=0; n<a->use; ++n,p+=a->blocksize)
	if(!func(p,arg))
	    break;
    return n;
}

//n文字追加
Array* ArAddN(Array* a,const void* p,int n)
{
    memcpy(ArExpand(a,n),p,n*a->blocksize);
    return a;
}

Array* ArAdd(Array* a,const void* p)
{
    return ArAddN(a,p,1);
}

Array* ArAdd1(Array* ws,char c)
{
    *(char*)ArExpand(ws,1) = c;
    return ws;
}

Array* ArAddAr(Array* a,const Array* b)
{
    memcpy(ArExpand(a,b->use),b->adr,ArUsingBytes(b));
    return a;
}

//dstを返す
Array* ArCopy(Array* dst,const Array* src)
{
    dst->reserve = (dst->reserve*dst->blocksize)/src->blocksize;
    dst->blocksize = src->blocksize;
    dst->pagesize = src->pagesize;
    dst->constructor = src->constructor;
    dst->use = 0;
    memcpy(ArAlloc(dst,src->use),src->adr,src->use*src->blocksize);
    return dst;
}

Array* ArInsert(Array* a,int pos,int count,const void* p)
{
    ArExpand(a,count);
    memmove(ArElem(a,pos+count),ArElem(a,pos),(a->use-pos)*a->blocksize);
    memcpy(ArElem(a,pos),p,count*a->blocksize);
    return a;
}

//pの配列番号を返す
int ArIndex(Array* a,const void* p)
{
    return ((char*)p - (char*)a->adr)/a->blocksize;
}

//useを１減らす
Array* ArDec(Array* a)
{
    if(-- a->use < 0)
	a->use = 0;
    return a;
}

/*
  aにprintfする。
  aはクリアされず、aの最後に追加出力される。
  aのブロックサイズは１にすること。
  useは文字数になる。ヌル文字は付けられる。
*/
Array* ArPrintV(Array* a,const char* fmt,va_list vl)
{
    int w,fz;
    va_list ovl;

    va_copy(ovl,vl);
    fz = a->reserve - a->use; //空いている大きさ
    if((w = vsnprintf(ArElem(a,a->use),fz,fmt,vl)) >= fz){
	w -= fz; //足りない大きさ
	w = (w & (-128)) + (((w & 0x7f)!=0)<<7); //128バイトで切り上げる
	ArReserve(a,a->reserve+w);
	w = vsnprintf(ArElem(a,a->use),fz+w,fmt,ovl);
    }
    a->use += w;
    va_end(ovl);
    return a;
}
Array* ArPrint(Array* a,const char* fmt,...)
{
    va_list vl;

    va_start(vl,fmt);
    a = ArPrintV(a,fmt,vl);
    va_end(vl);
    return a;
}

Array* ArSwap(Array* a,Array* b)
{
    Array c = *a;
    *a = *b;
    *b = c;
    return a;
}

int ArEq(Array* a,Array* b)
{
    return a->blocksize==b->blocksize && a->use==b->use && memcmp(a->adr,b->adr,a->blocksize*a->use)==0;
}
