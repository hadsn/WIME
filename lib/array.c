// -*- coding:euc-jp -*-
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "array.h"

#define DEFAULT_WSPAGESIZE 128

/*
  wsがNULLのときはmalloc()で確保する。
  エラーの時はNULLを返す。
*/
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

/*
  pagesizeを指定したいとき。
 */
Array* ArNewPs(Array* ws,int bs,ArNewCtr ct,int pagesize)
{
    if((ws = ArNew(ws,bs,ct)) != NULL)
	ws->pagesize = pagesize;
    return ws;
}

/*
  wsが使っているメモリを解放する。wsは再初期化される。
 */
Array* ArDelete(Array* ws)
{
    if(ws){
	if(!ws->adr)
	    free(ws->adr);
	ws = ArNewPs(ws,ws->blocksize,ws->constructor,ws->pagesize);
    }
    return ws;
}

/*
  要素の数を０にする。メモリは解放しない。
 */
Array* ArClear(Array* ws)
{
    if(ws)
	ws->use = 0;
    return ws;
}

/*
  最低count個の領域を確保する。
  メモリを確保するだけで、useは変更しない。
  追加確保したセルはconstructorが呼ばれる。
  エラーの時はNULLを返す。
*/
Array* ArReserve(Array* ws,int count)
{
    if(ws==NULL)
	return NULL;
    
    void* newadr = ws->adr;
    if(count > ws->reserve){
	newadr = realloc(ws->adr,(ws->reserve=(count/ws->pagesize+1)*ws->pagesize)*ws->blocksize);
    }
    if(newadr == NULL)
	return NULL;

    ws->adr = newadr;
    if(ws->constructor != NULL){
	int n;
	char* p;
	for(n=ws->use,p=ArElem(ws,n); n<ws->reserve; ++n,p+=ws->blocksize)
	    ws->constructor(p);
    }
    return ws;
}

/*
  n個確保する
  先頭を返す。エラーの時はNULLを返す。
*/
void* ArAlloc(Array* a,int n)
{
    void* r = NULL;
    if(ArReserve(a,n) != NULL){
	r = a->adr;
	a->use = n;
    }
    return r;
}

/*
  n個追加使用する
  確保分の先頭を返す。エラーの時はNULLを返す。
*/
void* ArExpand(Array* ws,int n)
{
    void* p = NULL;
    if(ArReserve(ws,ws->use+n) != NULL){
	p = ArElem(ws,ws->use);
	ws->use += n;
    }
    return p;
}

/*
  n個目の要素のアドレスを返す。
 */
void* ArElem(const Array* ws,int n)
{
    return (ws && ws->adr) ? (char*)(ws->adr)+n*ws->blocksize : NULL;
}

/*
  内容が*pと等しい要素のインデックスを返す
  n=検索開始インデックス
  見つからないorエラーの時-1を返す。
*/
int ArFind(const Array* ws,int n,const void* p)
{
    int index = -1;
    if(ws){
	for(char* src=ArElem(ws,n); n<ws->use; src+=ws->blocksize,++n){
	    if(memcmp(src,p,ws->blocksize) == 0)
		break;
	}
	index = n<ws->use ? n : -1;
    }
    return index;
}

/*
  pos個目の要素を削除する。
 */
Array* ArRemove(Array* ws,int pos)
{
    if(ws && pos>=0){
	char* ad = ArElem(ws,pos);
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
    if(a){
	for(char* ep=a->adr; start<a->use; ++start,ep+=a->blocksize)
	    if(eq(ep,val)){
		st = start;
		break;
	    }
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
  エラー(a==NULL)のとき-1を返す。
*/
int ArForEach(Array* a,ArForEachFunc func,void* arg)
{
    int n=-1;
    if(a){
	char* p = a->adr;
	for(n=0; n<a->use; ++n,p+=a->blocksize)
	    if(!func(p,arg))
		break;
    }
    return n;
}

/*
  pからn個追加。aを返す。
  エラーの時はNULLを返す。
*/
Array* ArAddN(Array* a,const void* p,int n)
{
    void* adr = ArExpand(a,n);
    if(!adr)
	return NULL;
    memcpy(adr,p,n*a->blocksize);
    return a;
}

/*
  a += b
*/
Array* ArAddAr(Array* a,const Array* b)
{
    void* adr = ArExpand(a,b->use);
    if(!adr)
	return NULL;
    memcpy(adr,b->adr,ArUsingBytes(b));
    return a;
}

/*
  dstを返す。
  エラーの時NULLを返す。元のdstは破壊されているだろう。
*/
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

/*
  a[pos]の位置にpからcount個を挿入する。
 */
Array* ArInsert(Array* a,int pos,int count,const void* p)
{
    if(!ArExpand(a,count))
	return NULL;
    memmove(ArElem(a,pos+count),ArElem(a,pos),(a->use-pos)*a->blocksize);
    memcpy(ArElem(a,pos),p,count*a->blocksize);
    return a;
}

//pの配列番号を返す
//エラー(a==NULL)の時-1を返す。
int ArIndex(Array* a,const void* p)
{
    return a ? (((char*)p - (char*)a->adr)/a->blocksize) : -1;
}

//useを１減らす
Array* ArDec(Array* a)
{
    if(a){
	if(-- a->use < 0)
	    a->use = 0;
    }
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
    if(a){
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
    }
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

/*
  a <--> b
 */
Array* ArSwap(Array* a,Array* b)
{
    if(a==NULL || b==NULL)
	return NULL;
    Array c = *a;
    *a = *b;
    *b = c;
    return a;
}

/*
  a == b
 */
int ArEq(Array* a,Array* b)
{
    if(a==NULL || b==NULL)
	return 0;
    return a->blocksize==b->blocksize && a->use==b->use && memcmp(a->adr,b->adr,a->blocksize*a->use)==0;
}

/*
  バッファアドレス、バッファサイズ(要素単位)、ユーザーデータを引数にしてfuncを実行する。
  funcは処理が終われば0を返す。バッファが足りなければ必要な要素数を返す。
  funcが0を返すまでaの領域を拡大して再度funcを実行することを繰り返す。
  useは設定されない。
 */
Array* ArBuf(Array* a,ArBufFunc func,void* data)
{
    if(a){
	int blk = 1;
	do{
	    if(ArReserve(a,blk) == NULL){
		a = NULL;
		break;
	    }
	}while((blk = func(ArAdr(a),a->reserve,data)) > 0);
    }
    return a;
}
