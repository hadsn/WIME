#ifndef WIME_LIB_UT
#define WIME_LIB_UT

#include <stdint.h>
#include <stdbool.h>
#include "array.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ITEMS(v) (sizeof(v)/sizeof(v[0]))
#define MEMBERSIZE(s,m) sizeof(((s*)0)->m)
//#define OFFSETOF(type,member) ((int)&(((type*)0)->member))
#define UNUSED __attribute__((unused))

#if !(__GNUC__>=4 && __GNUC_MINOR__>=3)
    static inline int32_t __builtin_bswap32(int32_t x){
	uint8_t s,*p = (uint8_t*)&x;
	s=p[0]; p[0]=p[3]; p[3]=s;
	s=p[1]; p[1]=p[2]; p[2]=s;
	return x;
    }
#endif

int WcLen(const uint16_t* s);
uint16_t* WcChr(uint16_t* s,uint16_t c);
uint16_t* WcCpy(uint16_t* dst,const uint16_t* src);
uint16_t* StrListNthWc(uint16_t* s,int nmax,int n);
char* ToMb(const uint16_t* src);
uint16_t* ToWc(uint16_t* dst,const char* src);

uint16_t* Swap2p(void* x,int dist);
uint16_t Swap2(uint16_t x);
uint16_t Swap2c(const void* x);
#define Swap4 __builtin_bswap32
static inline int32_t Swap4c(const void* x){return Swap4(*(int32_t*)x);}
int* RevInt(int* x);

Array* Dump1(const char* fmt,const void* adr,int num,Array* a);
Array* Dump2(char* fmt,void* adr,int num,Array* a);
Array* Dump2le(char* fmt,void* adr,int num,Array* a);
Array* Dump4(char* fmt,void* adr,int num,Array* a);

char* ForwardEj(char* ej,int n);
int EjLen(const char* ej);
int EjZen2Han(char* dst,const char* src);
char* Zen2Roman(char* dest,const char* ej);
    char* HanToZen(char* dst,const char* src,int srclen,bool syn,bool zen_asc);
    char* ZenToHan(char* dst,const char* src);
    char* HiraToKata(char* dst,const char* src,int src_len);

    uint16_t* EjToU16(uint16_t* dst,const char* src);
    uint16_t* CejToU16(uint16_t* dst,const uint16_t* src);
    char* U16ToEj(char* dst,const uint16_t* src,int src_len);
    uint16_t* U16ToCej(uint16_t* dst,const uint16_t* src,int src_len);
    char* EjToU8(char* dst,const char* src0);
    char* U16ToU8(char* dst,const uint16_t* src,int src_len);
    char* EjToSj(char* out,const char* in);
    uint16_t* SjToU16(uint16_t* out0,const char* in0,size_t ileft);
    char* SjToEj(char* out,const char* in,int in_len);

#ifdef __cplusplus
}
#endif

#endif
