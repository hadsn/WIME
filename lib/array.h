#pragma once

#include <stdarg.h>

#ifdef __cplusplus
extern "C"{
#endif

    typedef struct{
	void* adr;
	int blocksize;
	int use;
	int reserve;
	int pagesize;
	void (*constructor)(void*);
    } Array;

    typedef void (*ArNewCtr)(void* elem);
    Array* ArNew(Array* ws,int bs,ArNewCtr ct);
    Array* ArNewPs(Array* ws,int bs,ArNewCtr ct,int pagesize);
    Array* ArDelete(Array* ws);
    Array* ArClear(Array* ws);
    Array* ArReserve(Array* ws,int count);
    void* ArExpand(Array* ws,int n);
    Array* ArAddN(Array* a,const void* p,int n);

    /* pから1個追加( *(type*)pを追加する)。 */
    static inline Array* ArAdd1(Array* a,const void* p){return ArAddN(a,p,1);}

    /* wsに1文字追加。wsはchar配列。 wsの要素サイズは1バイトにすること。*/
    static inline Array* ArAddChar(Array* ws,char c){ return ArAdd1(ws,&c);}
    
    Array* ArAddAr(Array* a,const Array* b);
    int ArFind(const Array* ws,int start,const void* p);
    Array* ArRemove(Array* ws,int pos);
    void* ArElem(const Array* ws,int n);
    typedef int (*ArFindFunc)(const void* elem,const void* val);
    int ArFindIf(const Array* a,int start,ArFindFunc eq,const void* val);
    void* ArFindElemIf(Array* a,int start,ArFindFunc eq,const void* val);
    void* ArAlloc(Array* a,int n);
    Array* ArCopy(Array* dst,const Array* src);
    Array* ArInsert(Array* a,int pos,int count,const void* p);
    int ArIndex(Array* a,const void* p);
    static inline void* ArAdr(Array* a){return a->adr;}
    static inline int ArUsing(const Array* a){return a->use;}
    static inline int ArUsingBytes(const Array* a){return a->use*a->blocksize;}
    static inline Array* ArSetUsing(Array* a,int u){a->use=u; return a;}
    static inline int ArBlockSize(const Array* a){return a->blocksize;}
    Array* ArDec(Array* a);
    Array* ArPrint(Array* a,const char* fmt,...)  __attribute__((format(printf,2,3)));
    Array* ArPrintV(Array* a,const char* fmt,va_list vl);
    Array* ArSwap(Array* a,Array *b);
    int ArEq(Array* a,Array* b);

    typedef int (*ArForEachFunc)(void* elem,void* arg);
    int ArForEach(Array* a,ArForEachFunc func,void* arg);

    typedef int (*ArBufFunc)(void* buf,int bufsize,void* data);
    Array* ArBuf(Array* a,ArBufFunc func,void* data);

#ifdef __cplusplus
}
#endif
