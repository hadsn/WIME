#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "pkt.h"
#include "lib/ut.h"
#include "corr.h"

bool Snd0(int fd,const char* ver,const char* user)
{
    int infosz = strlen(ver)+1+strlen(user)+1;
    char buf[sizeof(Req0_t)+infosz];
    Req0_t *r = (Req0_t*)buf;

    r->init = Swap4(1);
    r->len = Swap4(infosz);
    sprintf(r->info,"%s:%s",ver,user);
    return write(fd,buf,sizeof(buf))==(int)sizeof(buf);
}

bool Snd1(int fd,int prn)
{
    Req1_t r = {prn&0xff,prn>>8,0};
    return write(fd,&r,sizeof(r))==sizeof(r);
}

bool Snd2(int fd,int prn,int16_t p1)
{
    Req2_t r = {{prn&0xff,prn>>8,Swap2(2)},Swap2(p1)};
    return write(fd,&r,sizeof(r))==sizeof(r);
}

bool Snd3(int fd,int prn,int16_t p1,uint16_t p2)
{
    Req3_t r = {{prn&0xff,prn>>8,Swap2(4)},Swap2(p1),Swap2(p2)};
    return write(fd,&r,sizeof(r))==sizeof(r);
}

//str_lenは文字数。str_len<0のときはヌル文字を探す
static bool snd_s16(int fd,const void* base,int base_size,const uint16_t* str,int str_len)
{
    char* buf;
    int bufsize;
    bool st;

    if(str == NULL)
	str_len = 0;
    if(str_len < 0)
	str_len = WcLen(str)+1;
    str_len *= 2; //バイト数にする
    buf = malloc(bufsize = base_size+str_len);
    memcpy(buf,base,base_size);
    memcpy(buf+base_size,str,str_len);
    ((CanHeader*)buf)->Length = Swap2(bufsize-sizeof(CanHeader));
    st = (write(fd,buf,bufsize)==bufsize);
    free(buf);
    return st;
}

//p5lenは文字数。p5len<0のときはヌル文字を探す
bool Snd4(int fd,int prn,int16_t p1,uint16_t p2,uint16_t p3,uint16_t p4,uint16_t* p5,int p5len)
{
    Req4_t r = {{prn&0xff,prn>>8,0},Swap2(p1),Swap2(p2),Swap2(p3),Swap2(p4)};
    return snd_s16(fd,&r,sizeof(r),p5,p5len);
}

bool Snd5(int fd,int prn,int16_t p1,uint16_t p2,int32_t p3)
{
    Req5_t r = {{prn&0xff,prn>>8,Swap2(8)},Swap2(p1),Swap2(p2),Swap4(p3)};
    return write(fd,&r,sizeof(r))==sizeof(r);
}    

bool Snd6(int fd,int prn,int16_t p1,int16_t p2,uint16_t p3)
{
    return Snd7(fd,prn,p1,p2,(int16_t)p3);
}

bool Snd7(int fd,int prn,int16_t p1,int16_t p2,int16_t p3)
{
    Req7_t r = {{prn&0xff,prn>>8,Swap2(6)},Swap2(p1),Swap2(p2),Swap2(p3)};
    return write(fd,&r,sizeof(r))==sizeof(r);
}    

bool Snd9(int fd,int prn,int16_t p1,int16_t p2,int16_t p3,int16_t p4)
{
    Req9_t r = {{prn&0xff,prn>>8,Swap2(8)},{Swap2(p1),Swap2(p2),Swap2(p3),Swap2(p4)}};
    return write(fd,&r,sizeof(r))==sizeof(r);
}    

//p4len=p4の個数
bool Snd10(int fd,int prn,int16_t p1,int16_t p2,int32_t p3,int16_t* p4,int p4len)
{
    int datasize = sizeof(Req10_t)-sizeof(CanHeader) + sizeof(*p4)*p4len;
    int totalsize = sizeof(CanHeader)+datasize;
    Req10_t* r = malloc(totalsize);
    Req10_t r0 = {{prn&0xff,prn>>8,Swap2(datasize)},Swap2(p1),Swap2(p2),Swap4(p3)};
    memcpy(r,&r0,sizeof(r0));
    for(int n=0; n<p4len; ++n)
	r->p4[n] = Swap2(*(p4++));
    bool st = (write(fd,r,totalsize)==totalsize);
    free(r);
    return st;
}

//p3の個数をlenで指定する。len<0のときはヌル文字を探す。
//p3はバイトの入れ換えをせずそのまま渡す
bool Snd11(int fd,int prn,int16_t p1,int16_t p2,const uint16_t* p3,int len)
{
    Req11_t r = {{prn&0xff,prn>>8,0},Swap2(p1),Swap2(p2)};
    return snd_s16(fd,&r,sizeof(r),p3,len);
}

bool Snd14(int fd,int prn,int32_t p1,int16_t p2,const uint16_t* p3)
{
    int bufsize = sizeof(Req14_t)+(WcLen(p3)+1)*2;
    Req14_t r = {{prn&0xff,prn>>8,Swap2(bufsize-sizeof(CanHeader))},Swap4(p1),Swap2(p2)};
    Req14_t* buf = malloc(bufsize);
    memcpy(buf,&r,sizeof(r));
    if(p3!=NULL)
	WcCpy(buf->p3,p3);
    bool st=(write(fd,buf,bufsize)==bufsize);
    free(buf);
    return st;
}

bool Snd15(int fd,int prn,int32_t p1,int16_t p2,const char* p3)
{
    int bufsize = sizeof(Req15_t);
    if(p3!=NULL)
	bufsize += strlen(p3)+1;
    char buf[bufsize];
    Req15_t *r = (Req15_t*)buf;
    r->h.Major = prn & 0xff;
    r->h.Minor = prn >> 8;
    r->h.Length = Swap2(bufsize-sizeof(CanHeader));
    r->p1 = Swap4(p1);
    r->p2 = Swap2(p2);
    if(p3!=NULL)
	strcpy(r->p3,p3);
    return write(fd,buf,bufsize)==bufsize;
}

//sはNULLで終わる配列
bool Snd17a(int fd,int prn,const char** s)
{
    int sz = 1,len;
    for(const char** p=s; *p!=NULL; ++p)
	sz += strlen(*p)+1;
    char buf[sizeof(Req17_t)+sz];
    Req17_t *r = (Req17_t*)buf;

    r->h.Major = prn & 0xff;
    r->h.Minor = prn >> 8;
    r->h.Length = Swap2(sz);

    char *p = r->p1;
    while(*s != NULL){
	len = strlen(*s)+1;
	memcpy(p,*s,len);
	p += len;
	++s;
    }
    *(p++) = 0;
    return write(fd,buf,sizeof(buf))==(int)sizeof(buf);
}

bool SndN(int fd,int prn,const void* r,unsigned size)
{
    CanHeader h;
    h.Major = prn & 0xff;
    h.Minor = prn >> 8;
    h.Length = Swap2(size);
    return write(fd,&h,sizeof(h))==sizeof(h) && write(fd,r,size)==size;
}

/*
  buf0==NULL,あるいはbufsizeに収まらない時はmallocで確保する。
  buf0あるいは確保したアドレスを返す
*/
void* RcvN(int fd,CanHeader* buf0,int bufsize)
{
    int left,rsz;
    char *bp;
    CanHeader *buf;

    if(buf0 == NULL)
	buf = malloc(bufsize = sizeof(CanHeader));
    else
	buf = buf0;

    //まずヘッダを読み込む
    left = sizeof(CanHeader);
    bp = (char*)buf;
    do{
	rsz = read(fd,bp,left);
	bp += rsz;
    }while(rsz>0 && (left-=rsz)>0);
    if(rsz <= 0)
	return NULL;

    if((left = buf->Length = Swap2(buf->Length)) > 0){
	//追加データがある
	int need = sizeof(CanHeader)+buf->Length;
	if(bufsize < need){
	    //足りなければmallocでバッファを作る
	    if(buf0 == NULL)
		buf = realloc(buf,need);
	    else
		buf = memcpy(malloc(need),buf,sizeof(CanHeader));
	    bp = (char*)(buf+1);
	}
	do{
	    rsz = read(fd,bp,left);
	    bp += rsz;
	}while(rsz>0 && (left-=rsz)>0);
	if(rsz <= 0){
	    if(bufsize < need)
		free(buf);
	    buf = NULL;
	}
    }
    return buf;
}

//コンテキスト番号を返す。エラーの時-1
int Rcv0(int fd,int* ver)
{
    Rply0_t r;
    int cxn = -1;

    //通常のパケットとは構造が違うのでRcvNは使えない
    if(read(fd,&r,sizeof(r)) == sizeof(r)){
	*ver = Swap2(r.minor);
	cxn = Swap2(r.cxn);
    }
    return cxn;
}

bool Rcv2(int fd,char* p1)
{
    Rply2_t r,*p;
    bool st=false;
    if((p=RcvN(fd,(CanHeader*)&r,sizeof(r))) && p==&r){
	*p1 = p->p1;
	st=true;
    }
    if(p!=NULL && p!=&r)
	free(p);
    return st;
}

//p2はfreeすること
bool Rcv3(int fd,char* p1,uint16_t** p2)
{
    int str_sz;
    bool st = false;
    Rply3_t *p = RcvN(fd,NULL,0);
    if(p != NULL){
	*p1 = p->p1;
	if((str_sz = (p->h.Length - (sizeof(*p)-sizeof(p->h)))) > 0)
	    memcpy(p,p->p2,str_sz);
	else{
	    free(p);
	    p = NULL;
	}
	*p2 = (uint16_t*)p;
	st = true;
    }
    return st;
}

//p2の個数を返す。受信エラーの時は-1を返す。p2はmallocで確保される(個数0の時はnull)。
int Rcv4v(int fd,char* p1,int32_t** p2)
{
    Rply4_t *p;
    int n=-1;
    if((p = RcvN(fd,NULL,0)) != NULL){
	*p1 = p->p1;
	if((n = (p->h.Length-1)/4) == 0){
	    free(p);
	    *p2 = NULL;
	}else{
	    //pの先頭からp->p2を書き込む
	    int32_t *i = (int32_t*)p;
	    for(int x=0; x<n; ++x)
		*(i++) = Swap4(p->p2[x]);
	    *p2 = (int32_t*)p;
	}
    }
    return n;
}

bool Rcv4(int fd,char* p1,int32_t* p2)
{
    int n;
    int32_t *p2buf;
    bool st=false;
    if((n = Rcv4v(fd,p1,&p2buf)) >= 0){
	memcpy(p2,p2buf,n*4);
	free(p2buf);
	st=true;
    }
    return st;
}

bool Rcv5(int fd,int16_t* p1)
{
    Rply5_t r,*p;
    bool st=false;
    if((p=RcvN(fd,(CanHeader*)&r,sizeof(r))) && p==&r){
	*p1 = Swap2(p->p1);
	st=true;
    }
    if(p!=NULL && p!=&r)
	free(p);
    return st;
}

//p2はmalloc()で返す。なければNULLになるので初期化の必要なし。
bool Rcv6(int fd,int16_t* p1,char** p2)
{
    bool st = false;
    Rply6_t *p = RcvN(fd,NULL,0);
    if(p != NULL){
	*p1 = Swap2(p->p1);
	if(p->h.Length > 2)
	    strcpy((char*)p,p->p2);
	else{
	    free(p);
	    p = NULL;
	}
	*p2 = (char*)p;
	st = true;
    }
    return st;
}

//p2はmalloc()で返す。なければNULLがセットされる。
bool Rcv7(int fd,int16_t* p1,uint16_t** p2)
{
    bool st = false;
    Rply7_t* p = RcvN(fd,NULL,0);
    if(p != NULL){
	*p1 = Swap2(p->p1);
	if(p->h.Length > 2)
	    memcpy(p,p->p2,p->h.Length-2);
	else{
	    free(p);
	    p = NULL;
	}
	*p2 = (uint16_t*)p;
	st = true;
    }
    return st;
}

/*
  p2,p3はmallocを使う。p4には必要な大きさを与えること。
*/
bool Rcv10(int fd,char* p1,char** p2,char** p3,int32_t* p4)
{
    Rply10_t *r;
    int p2size,p3size,p4len;
    char *p3pos;
    int32_t *p4pos;

    if((r = RcvN(fd,NULL,0)) != NULL){
	*p1 = r->p1;
	p2size = strlen(r->p2)+1;
	p3size = strlen(*p3 = strdup(p3pos = r->p2 + p2size))+1;
	p4pos = (int32_t*)(p3pos + p3size);
	p4len = (r->h.Length - sizeof(*p1) - p2size - p3size)/4;
	while(--p4len >= 0)
	    *(p4++) = Swap4c(p4pos++);
	*p2 = memcpy(r,r->p2,p2size); //p2はrを上書き
    }
    return r!=NULL;
}
