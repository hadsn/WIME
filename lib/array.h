#pragma once

#include <stdarg.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*ArNewCtr)(void* elem);
typedef struct{
    void* adr;
    int blocksize;
    int use;
    int reserve;
    int pagesize;
    ArNewCtr constructor;
} Array;

Array* ArNew(Array* ws,int bs,ArNewCtr ct);
/* wsがNULLのときはmalloc()で確保する。エラーの時はNULLを返す。 */

Array* ArNewPs(Array* ws,int bs,ArNewCtr ct,int pagesize);
/* pagesizeを指定したいとき。 */

Array* ArDelete(Array* ws);
/* wsが使っているメモリを解放する。wsは再初期化される。 */

Array* ArClear(Array* ws);
/* 要素の数を０にする。メモリは解放しない。 */

Array* ArReserve(Array* ws,int count);
/* *最低*count個の領域を確保する。メモリを確保するだけで、useは変更しない。
   追加確保したセルはconstructorが呼ばれる。エラーの時はNULLを返す。 */

void* ArAlloc(Array* a,int count);
/* count個確保する。useはcountになる。
   バッファアドレスを返す。エラーの時はNULLを返す。*/

void* ArExpand(Array* ws,int count);
/* count個追加使用する。 *確保分の先頭*を返す。エラーの時はNULLを返す。*/

Array* ArAddN(Array* ws,const void* valptr,size_t count);
/* valptrからcount個追加。wsを返す。 エラーの時はNULLを返す。*/

static inline Array* ArAdd1(Array* a,const void* p){return ArAddN(a,p,1);}
/* pから1個追加( *(type*)pを追加する)。 */

static inline Array* ArAddChar(Array* ws,char c){ return ArAdd1(ws,&c);}
/* wsに1文字追加。wsはchar配列。 wsの要素サイズは1バイトにすること。*/

static inline Array* ArAddStr(Array* ws,const char* str){ return ArAddN(ws,str,strlen(str)+1);}
/* wsに文字列を追加。ヌル文字も使用数に含む。wsの要素サイズは1バイトにすること。*/
    
Array* ArAddAr(Array* a,const Array* b);
/* a += b */

Array* ArRemove(Array* ws,int pos,int count);
/* pos番目からcount個の要素を削除する。指定が多すぎるときはpos以降を未使用とする。*/

static inline void* ArElemNc(const Array* ws,int pos){
    return (ws && ws->adr) ? (char*)(ws->adr)+pos*ws->blocksize : NULL; }
/* pos(>=0)番目の要素のアドレスを返す。境界チェックなし*/

static inline void* ArElem(const Array* ws,int pos){
    return (ws && pos>=0 && pos<ws->use) ? ArElemNc(ws,pos) : NULL; }
/* pos(>=0)番目の要素のアドレスを返す。チェックあり*/
    
Array* ArInsert(Array* ws,int pos,int count,const void* valptr);
/* ws[pos]の位置にvalptrからcount個を挿入する。 */

int ArIndex(Array* ws,const void* ptr);
/* ptrの配列番号を返す。ptrはもちろんws内のアドレスであること。
   エラー(a==NULL)の時-1を返す。*/

int ArFind(const Array* ws,int pos,const void* val);
/* 内容が*valと等しい要素のインデックスを返す。 pos=検索開始インデックス
   見つからないorエラーの時-1を返す。*/

typedef int (*ArFindFunc)(const void* elem,const void* val);
int ArFindIf(const Array* ws,int start,ArFindFunc eq,const void* val);
/* eq(要素アドレス,val)が真になる要素の番号を返す。 見つからないとき-1を返す */

void* ArFindElemIf(Array* ws,int start,ArFindFunc eq,const void* val);
/*eq(val,要素アドレス)が真になる要素のアドレスを返す。見つからないときはArExpandを呼び出す。*/

Array* ArCopy(Array* dst,const Array* src);
/* dst=src  dstを返す。エラーの時NULLを返す。元のdstは破壊されているだろう。*/

static inline void* ArAdr(Array* a){return a!=NULL ? a->adr : NULL;}
//バッファアドレスを返す。

static inline const void* ArAdrC(const Array* a){return a!=NULL ? a->adr : NULL;}
//const版

static inline int ArBlockSize(const Array* a){return a!=NULL ? a->blocksize : 0;}
//要素の大きさを返す。
    
static inline int ArUsing(const Array* a){return a!=NULL ? a->use : 0;}
//使用している要素数を返す。

static inline int ArUsingBytes(const Array* a){return a!=NULL ? a->use*a->blocksize : 0;}
//使用しているバイト数を返す。

static inline Array* ArSetUsing(Array* a,int u){if(a!=NULL) a->use=u; return a;}
//使用数を変更する。
    
Array* ArDec(Array* ws);
//使用数を１減らす。0以下にはならない。

Array* ArPrint(Array* ws,const char* fmt,...)  __attribute__((format(printf,2,3)));
Array* ArPrintV(Array* ws,const char* fmt,va_list vl);
/* wsにprintfする。wsはクリアされず、wsの最後にヌル文字があれば消して追加出力される。
   wsのブロックサイズは１にすること。
   useはヌルを含めた文字数になる。 */

Array* ArSwap(Array* a,Array *b);
// a <--> b

int ArEq(Array* a,Array* b);
// a == b

typedef int (*ArForEachFunc)(void* elem,void* arg);
int ArForEach(Array* ws,ArForEachFunc func,void* arg);
/* funcが真を返したら終了する。終了したときの要素番号を返す。すべての要素を処理したときはuse+1になる。
   エラー(ws==NULL)のとき-1を返す。*/

typedef int (*ArBufFunc)(void* buf,int bufsize,void* data);
Array* ArBuf(Array* ws,ArBufFunc func,void* data);
/* バッファアドレス、バッファサイズ(要素単位)、ユーザーデータを引数にしてfuncを実行する。
   funcは処理が終われば0を返す。バッファが足りなければ必要な要素数を返す。
   funcが0を返すまでaの領域を拡大して再度funcを実行することを繰り返す。
   useは設定されない。 */

#ifdef __cplusplus
}
#endif

#define ARELEM(type,ws,n) (type*)ArElem(ws,n)
#define ARVAL(type,ws,n) *ARELEM(type,ws,n)

//(C) 2008 thomas
