#ifndef WIME_LIB_UT
#define WIME_LIB_UT

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> //size_t
#include "array.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ITEMS(v) (sizeof(v)/sizeof(v[0]))
#define MEMBERSIZE(s,m) sizeof(((s*)0)->m)
//#define OFFSETOF(type,member) ((int)&(((type*)0)->member))
#define UNUSED __attribute__((unused))

#if ! (defined(__clang__) || ((__GNUC__*100+__GNUC_MINOR__)>=403))
    static inline int32_t __builtin_bswap32(int32_t x){
	uint8_t s,*p = (uint8_t*)&x;
	s=p[0]; p[0]=p[3]; p[3]=s;
	s=p[1]; p[1]=p[2]; p[2]=s;
	return x;
    }
#endif

size_t WcLen(const uint16_t* s);
uint16_t* WcChr(uint16_t* s,uint16_t c);
uint16_t* WcCpy(uint16_t* dst,const uint16_t* src);
char* ToMb(const uint16_t* src);
uint16_t* ToWc(uint16_t* dst,int* dst_len,const char* src,int src_len);
uint16_t* WcDup(const uint16_t* src);
uint16_t* U16Tok(uint16_t** src);

uint16_t* Swap2p(void* x,int dist);
uint16_t Swap2(uint16_t x);
uint16_t Swap2c(const void* x);
#define Swap4 __builtin_bswap32
static inline uint32_t Swap4c(const void* x){return Swap4(*(uint32_t*)x);}
int* RevInt(int* x);

char* ForwardEj(char* ej,int n);
int EjLen(const char* ej);
//int EjZen2Han(char* dst,const char* src);
//char* HanToZen(char* dst,const char* src,int srclen,bool syn,bool zen_asc);
//char* ZenToHan(char* dst,const char* src);
//char* HiraToKata(char* dst,const char* src,int src_len);

#define U16HAN_VOICEDSOUNDMARK		0xff9e
#define U16HAN_SEMIVOICEDSOUNDMARK	0xff9f
int U16CombineHan(const uint16_t* src,int kata_hira,bool combine);
static inline int U16CombineHanHira(const uint16_t* src){
    return U16CombineHan(src,1,true);
}
uint16_t* U16HanToZen(uint16_t* dst,int* dstlen,const uint16_t* src,int srclen,int kata_hira,bool combine);
static inline uint16_t* U16HanToZenKata(uint16_t* dst,int* dstlen,const uint16_t* src,int srclen){
    return U16HanToZen(dst,dstlen,src,srclen,0,true);
}
static inline uint16_t* U16HanToZenHira(uint16_t* dst,int* dstlen,const uint16_t* src,int srclen){
    return U16HanToZen(dst,dstlen,src,srclen,1,true);
}

uint16_t* U16ZenToHan(uint16_t* dst,int* dstlen,const uint16_t* src,int srclen);
const char* U16Zen2Romaji(uint16_t u16);

uint16_t* EjToU16(uint16_t* dst,const char* src);
char* EjToU8(char* dst,const char* src0,int ileft);
//char* EjToSj(char* out,const char* in);
uint16_t* WejToU16(uint16_t* dst,const uint16_t* src);
char* U16ToEj(char* dst,int* dst_len,const uint16_t* src,int src_len);
uint16_t* U16ToWej(uint16_t* dst,int* dst_len,const uint16_t* src,int src_len);
char* U16ToU8(char* dst,int* dst_len,const uint16_t* src,int src_len);
//char* U16ToSj(char* out0,const uint16_t* in0,int in_len)
uint16_t* SjToU16(uint16_t* out0,const char* in0,size_t ileft);
char* SjToEj(char* out,const char* in,int in_len);
char* SjToU8(char* out,const char* in,int in_len);
//uint16_t* SjToWej(uint16_t* out,const char* in,int in_len);
char* U8ToEj(char* dst,const char* src);
uint16_t* U8ToU16(uint16_t* out,const char* in);
extern char* (*CurToU8)(char* out,const char* in,int inlen);

char* ForwardU8(const char* str,int n);

int MkDir(const char* p);

char* StrDel(char* str,int pos,int len);

typedef struct{
    int mask;
    const char* desc;
} BitDesc;
#define BITDESC(x) {x,#x}
Array* FlagStr(unsigned flags,const BitDesc* bits,Array* buf);

#ifdef __cplusplus
}
#endif

#endif

//(C) 2008 thomas
