#define _GNU_SOURCE
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iconv.h>
#include <stdbool.h>
#include <libgen.h>
#include <sys/stat.h>
#include <errno.h>
#include "ut.h"

//e-a2a2 s-81a0 u-25a1 t-e296a1 '□'
#define TOFU_E	0xa2a2
#define TOFU_S	0xa081
#define TOFU_U2	0xa125
#define TOFU_U8	0xa196e2

int Verbose;
char LogMark; //メッセージ表示にも使う。

enum{ EU16_08,EU16_13,
      U16E_08,U16E_13,
      EU8_08,EU8_13,
      U16S,U16S_13,
      SU16,SU16_13,
      U16U8,CV_DUM0,
      CNVMAX};
iconv_t Icv[CNVMAX];

__attribute__((constructor))
void iconv_ini(void)
{
    const char UTF16[] = "utf-16le",UTF8[]="utf-8",
	EUCJP[]="euc-jp-ms",EUCX0213[]="euc-jisx0213",
	SJIS[]="shift_jis",SJIS13[]="shift_jisx0213";
    const char *cs[][2]={ /* to,from */
	{UTF16,EUCJP},		{UTF16,EUCX0213},
	{EUCJP,UTF16},		{EUCX0213,UTF16},
	{UTF8,EUCJP},		{UTF8,EUCX0213},
	{SJIS,UTF16},		{SJIS13,UTF16},
	{UTF16,SJIS},		{UTF16,SJIS13},
	{UTF8,UTF16},		{NULL,NULL},
    };
    for(unsigned n=0; n<ITEMS(Icv); ++n)
	if(cs[n][0]!=NULL)
	    Icv[n] = iconv_open(cs[n][0],cs[n][1]);
}

__attribute__((destructor))
void iconv_fin(void)
{
    for(unsigned n=0; n<ITEMS(Icv); ++n)
	if(Icv[n] != NULL){
	    iconv_close(Icv[n]);
	    Icv[n] = NULL;
	}
}

uint16_t* Swap2p(void* x,int dist)
{
    unsigned char *a,*b,c;
    a = (unsigned char*)x;
    b = a+dist;
    c = *a; *a = *b; *b = c;
    return (uint16_t*)x;
}

uint16_t Swap2(uint16_t x)
{
    return *Swap2p(&x,1);
}

uint16_t Swap2c(const void* x)
{
    return Swap2(*(const uint16_t*)x);
}

unsigned char* rev_copy(unsigned char* to,const unsigned char* from,int len)
{
    unsigned char* r = to;
    from += len;
    while(--len >= 0)
	*(to++) = *(--from);
    return r;
}

int* RevInt(int* x)
{
    int x0 = *x;
    return (int*)rev_copy((unsigned char*)x,(const unsigned char*)&x0,sizeof(int));
}

/*
  wchar_tの大きさはgccとwinegccで違う。
  別のディレクトリでgccを使う可能性もあるので、混乱しないようにwchar_t,wcs...()は
  使わずに自前の関数を作ることにする。
*/
int WcLen(const uint16_t* s)
{
    const uint16_t* s0 = s;

    if(s == NULL)
	return -1;

    while(*(s++) != 0)
	;
    return s-s0-1;
}
uint16_t* WcChr(uint16_t* s,uint16_t c)
{
    while(*s!=0 && *s!=c)
	++s;
    return (*s!=0 || c==0) ? s : NULL;
}
uint16_t* WcCpy(uint16_t* dst,const uint16_t* src)
{
    return memcpy(dst,src,(WcLen(src)+1)*2);
}

//文字列リストのn(>=0)番目の要素を返す
uint16_t* StrListNthWc(uint16_t* s,int nmax,int n)
{
    int x;
    for(x=0; x<n && x<nmax && *s!=0; ++x){
	s += WcLen(s)+1; //ヌル文字の次へ
    }
    return (x<nmax && *s!=0) ? s : NULL;
}

/*
  canna eucjpのwchar文字列をchar文字列にする。
  戻り値はfreeすること。
  cannaなのでsrcの下８ビットが第１バイト、上８ビットが第２バイトになる。
  半角カナは0x8eがついていない(第１バイトは０)
  cannaはjisx0212を２バイトで扱うために第１バイトだけを変換するようだ。
*/
char* ToMb(const uint16_t* src)
{
    Array dst;
    
    ArNew(&dst,1,NULL);
    if(src != NULL){
	for(; *src!=0; ++src){
	    unsigned char uc,*dp;
	    if((*src & 0xff) != 0){
		if((*src>>8) & 0x80){
		    *(uint16_t*)ArExpand(&dst,2) = *src;
		}else{ //jisx0212
		    *(dp = ArExpand(&dst,3)) = 0x8f;
		    *(uint16_t*)(dp+1) = *src|0x8000;
		}
	    }else{
		uc = (unsigned char)(*src>>8);
		if(uc>=0xa1 && uc<=0xdf){ //半角カナ
		    dp = ArExpand(&dst,2);
		    *(dp++) = 0x8e;
		}else
		    dp = ArExpand(&dst,1);
		*dp = uc;
	    }
	}
	*(char*)ArExpand(&dst,1) = 0;
    }
    return ArAdr(&dst);
}

/*
  eucjpをcanna wcharにする。
  cannaなのでsrcの下８ビットが第１バイト、上８ビットが第２バイトになる。
  半角カナは0x8eがついていない(第１バイトは０)
  cannaはjisx0212を２バイトで扱うために第１バイトだけを変換するようだ。
*/
uint16_t* ToWc(uint16_t* dst,const char* src)
{
    uint16_t *dst0,wc;

    if(dst == NULL)
	dst = malloc((EjLen(src)+1)*2);
    dst0 = dst;

    while(*src != 0){
	switch((uint8_t)*src){
	case 0x8e: //半角カナ
	    wc = ((uint16_t)*(++src))<<8;
	    ++src;
	    break;
	case 0x8f: //0212
	    wc = *(uint16_t*)(++src) & 0x7fff;
	    src += 2;
	    break;
	case 0xa1 ... 0xff: //0208
	    wc = *(uint16_t*)src;
	    src += 2;
	    break;
	default: //ascii
	    wc = ((uint16_t)*(src++))<<8;
	}
	*(dst++) = wc;
    }
    *dst = 0;
    return dst0;
}

Array* Dump1(const char* fmt,const void* adr,int num,Array* a)
{
    const uint8_t *p = (const uint8_t *)adr;
    if(a == NULL)
	ArNew(a = malloc(sizeof(*a)),1,NULL);
    while(--num >= 0)
	ArPrint(a,fmt,*(p++));
    return a;
}
Array* Dump2(char* fmt,void* adr,int num,Array* a)
{
    uint16_t *p = (uint16_t*)adr;
    if(a == NULL)
	ArNew(a = malloc(sizeof(*a)),1,NULL);
    while(--num >= 0)
	ArPrint(a,fmt,*(p++));
    return a;
}
Array* Dump2le(char* fmt,void* adr,int num,Array* a)
{
    uint16_t *p = (uint16_t*)adr;
    if(a == NULL)
	ArNew(a = malloc(sizeof(*a)),1,NULL);
    for(; --num >= 0; ++p)
	ArPrint(a,fmt,((*p>>8)|(*p<<8))&0xffff);
    return a;
}
Array* Dump4(char* fmt,void* adr,int num,Array* a)
{
    uint32_t *p = (uint32_t*)adr;
    if(a == NULL)
	ArNew(a = malloc(sizeof(*a)),1,NULL);
    while(--num >= 0)
	ArPrint(a,fmt,*(p++));
    return a;
}

/*
  全角読み文字列をローマ字にする
*/
char* Zen2Roman(char* dest,const char* ej0)
{
    static const char* tab[]={
	"xa","a","xi","i","xu","u","xe","e","xo","o",
	"ka","ga","ki","gi","ku","gu","ke","ge","ko","go",
	"sa","za","si","zi","su","zu","se","ze","so","zo",
	"ta","da","ti","di","xtu","tu","du","te","de","to","do",
	"na","ni","nu","ne","no",
	"ha","ba","pa","hi","bi","pi","hu","bu","pu","he","be","pe","ho","bo","po",
	"ma","mi","mu","me","mo",
	"xya","ya","xyu","yu","xyo","yo",
	"ra","ri","ru","re","ro",
	"xwa","wa","wi","we","wo","nn"
    };
    static const char sym[]={ /* 0xa1xx */
	' ', 	',', 	'.', 	',', 	'.',	0,	':',	//a1
	';',	'?',	'!',	0,	0,	0,	'`',	0,	//a8
	'^',	'~',	'_',	0,	0,	0,	0,	0,	//b0
	0,	0,	0, 	0,	'-',	0,	0,	'/',	//b8
	'\\',	'-',	0,	'|',	0,	0,	0,	0,	//c0
	0,	0,	'(',	')',	0,	0,	'[',	']',	//c8
	'{',	'}',	'<',	'>',	0,	0,	'[',	']',	//d0
	0,	0,	0,	0,	'+',	'-',	0,	'*',	//d8
	'/',	'=',	0,	'<',	'>',	0,	0,	0,	//e0
	0,	0,	0,	0,	0,	0,	0,	'\\',	//e8
	'$',	0,	0,	'%',	'#',	'&',	'*',	'@'	//f0
    };
    const char* s;
    char* dest0=dest;
    unsigned char *ej=(unsigned char*)ej0;
    while(*ej != 0){
	if((*ej & 0x80) == 0){
	    //ascii
	    *(dest++) = *(ej++);
	}else{
	    switch(*(ej++)){
	    case 0xa1: //記号
		if(*ej>=0xa1 && *ej<=0xf7)
		    *(dest++) = sym[*ej-0xa1];
		++ej;
		break;
	    case 0xa3: //数字、アルファベット
		*(dest++) = *(ej++)-0xb0+0x30;
		break;
	    case 0xa4:
		s = tab[*(ej++)-0xa1];
		strcpy(dest,s);
		dest += strlen(s);
	    }
	}
    }
    *dest = 0;
    return dest0;
}

/*
  eucjpの半角カナ --> 全角ひらがな
  syn=濁点を合成する時true
  zen_asc=asciiを全角にするときtrue
  戻り値：次のdst
	use_src=使ったバイト数。濁点の合成で２文字使うこともある
  dstには最大３バイト必要
*/
char* EjHan2Zen(char* dst,const char* src0,int* use_src,bool syn,bool zen_asc)
{
    static const char as20[]=
	"　！”＃＄％＆’"	"（）＊＋，−．／"	/*20*/
	"０１２３４５６７"	"８９：；＜＝＞？"	/*30*/
	"＠ＡＢＣＤＥＦＧ"	"ＨＩＪＫＬＭＮＯ"	/*40*/
	"ＰＱＲＳＴＵＶＷ"	"ＸＹＺ［＼］＾＿"	/*50*/
	"‘ａｂｃｄｅｆｇ"	"ｈｉｊｋｌｍｎｏ"	/*60*/
	"ｐｑｒｓｔｕｖｗ"	"ｘｙｚ｛｜｝\xa1\xc1□"	/*70*/
	;
    static const char wk0[]=
	"。「」、・をぁ"		"ぃぅぇぉゃゅょっ"	//a1
	"ーあいうえおかき"	"くけこさしすせそ"	//b0
	"たちつてとなにぬ"	"ねのはひふへほま"	//c0
	"みむめもやゆよら"	"りるれろわん゛゜"	//d0
	;
    static const char wd0[]=
	"□□□□□□□"		"□□□□□□□□"	//a1
	"□□□□□□がぎ"	"ぐげござじずぜぞ"	//b0
	"だぢづでど□□□"	"□□ばびぶべぼ□"	//c0
	"□□□□□□□□"	"□□□□□□□□"	//d0
	;
    static const char wp0[]={
	"□□□□□□□"		"□□□□□□□□"	//a1
	"□□□□□□□□"	"□□□□□□□□"	//b0
	"□□□□□□□□"	"□□ぱぴぷぺぽ□"	//c0
	"□□□□□□□□"	"□□□□□□□□"	//d0
    };
    static const uint16_t *wk=(const uint16_t*)wk0;
    static const uint16_t *wdp[]={(const uint16_t*)wd0,(const uint16_t*)wp0};

    const unsigned char *src = (const unsigned char*)src0;
    int ofs;
    uint16_t z,tofu='□';
    unsigned char dp;

    switch(*src){
    case 0 ... 0x7f:
	if(zen_asc){
	    *(uint16_t*)dst = *(uint16_t*)(as20 + (*(src++)-0x20)*2);
	    dst += 2;
	}else
	    *(dst++) = *(src++);
	break;
    case 0x8e:  //半角カナのコード
	++src;
	ofs = *(src++)-0xa1;
	if(syn && src[0]==0x8e && (dp=src[1]-0xde,dp<2) && (z=wdp[dp][ofs])!=tofu){
	    *(uint16_t*)dst = z;
	    src += 2;
	}else{
	    *(uint16_t*)dst = wk[ofs];
	}
	dst += 2;
	break;
    case 0x8f:
	*(dst++) = *(src++);
    default:
	*(uint16_t*)dst = *(uint16_t*)src;
	dst += 2;
	src += 2;
    }
    *use_src = src - (const unsigned char*)src0;
    return dst;
}

/*
  eucjpの半角カナ --> 全角ひらがな
  srclen=バイト数
  syn=濁点を合成する時true
  zen_asc=asciiを全角にするときtrue
  asciiも全角文字にするためdstがsrcより長くなる可能性がある。
*/
char* HanToZen(char* dst,const char* src,int srclen,bool syn,bool zen_asc)
{
    char *dst0;
    int use_src;

    if(dst == NULL)
	dst = malloc(srclen>=0 ? srclen : EjLen(src)*3+1);
    dst0 = dst;

    if(srclen < 0)
	srclen = INT_MAX;

    while(*src!=0 && srclen>0){
	dst = EjHan2Zen(dst,src,&use_src,syn,zen_asc);
	srclen -= use_src;
	src += use_src;
    }
    *dst = 0;
    return dst0;
}

//eucjp文字列の先頭からn文字移動する。
char* ForwardEj(char* ej,int n)
{
    while(--n >= 0 && *ej!=0){
	if(*ej == (char)0x8f)
	    ++ej;
	if(*(++ej) < 0)
	    ++ej;
    }
    return ej;
}

/*
  euc-jpの文字数を数える
*/
int EjLen(const char* ej)
{
    int len=0;
    if(ej != NULL){
	while(*ej != 0){
	    ej = ForwardEj((char*)ej,1);
	    ++len;
	}
    }
    return len;
}

/*
  numにはEU16_08,U16E_08を渡すこと。
  08でエラーが出たら13を使う。とりあえず文字コードの保存はできる。
  08を先に使うので'か'行１文字が変換できない現象は出ないはずだが、一応チェックしておく。
  ??? iconvは*inに書き込んだりするんだろうか？
*/
bool conv(int num,char** in,size_t* ileft,char** out,size_t* oleft)
{
    bool st;
    char *in0 = *in, *out0=*out;
    size_t ileft0 = *ileft;

    st = (iconv(Icv[num],in,ileft,out,oleft)!=(size_t)-1);
    if(st && *out==out0){
	*in = in0;
	*ileft = ileft0;
	st = false;
    }
    if(!st && Icv[num+1]!=NULL){
	st = (iconv(Icv[num+1],in,ileft,out,oleft)!=(size_t)-1);
    }
    return st;
}

//euc-jp --> ucs2
uint16_t* EjToU16(uint16_t* dst0,const char* src0)
{
    char *src,*src_orig;
    size_t ileft=strlen(src0),oleft=EjLen(src0)*2;
    uint16_t *dst;

    src = src_orig = strdup(src0);
    if(dst0 == NULL)
	dst0 = malloc(oleft+2);
    dst = dst0;
    while(!conv(EU16_08,&src,&ileft,(char**)&dst,&oleft)){
	*(dst++) = TOFU_U2;
	oleft -= 2;
	if(*src == (char)0x8f){
	    ++src;
	    --ileft;
	}
	src += 2;
	ileft -= 2;
    }
    *dst = 0;
    //iconv(Icv[E2U08],NULL,NULL,NULL,NULL);
    //iconv(Icv[E2U13],NULL,NULL,NULL,NULL);
    free(src_orig);
    return dst0;
}

//canna ej --> ucs2
uint16_t* CejToU16(uint16_t* dst,const uint16_t* src)
{
    char *ej;

    dst = EjToU16(dst,ej = ToMb(src));
    free(ej);
    return dst;
}

//ucs2 --> ej or sj
char* u16_to_mb(int cv,int tofu,char* dst0,const uint16_t* src0,int src_len)
{
    uint16_t *src,*src_orig;
    char *dst;
    size_t ileft,oleft;

    if(src_len < 0)
	src_len = WcLen(src0);
    ileft = src_len*2;
    oleft = src_len*3; //ej用に多めに確保する
    src = src_orig = memcpy(malloc(ileft+2),src0,ileft+2);
    if(dst0 == NULL)
	dst0 = malloc(oleft+1);
    dst = dst0;
    while(!conv(cv,(char**)&src,&ileft,&dst,&oleft)){
	*(uint16_t*)dst = tofu;
	dst += 2;
	oleft -= 2;
	++src;
	ileft -= 2;
    }
    *dst = 0;
    free(src_orig);
    return dst0;
}

/* ucs2 -->euc-jp
   ??? euc-jisx0213で'か'行１文字だけの時、何も出力されずin-ptr,ileftだけ更新される。
   euc-jpのときは問題ない。わけがわからない。
   iconv(1)ではどうやってるんだろうか？
   shift-jisx0213でも同様のことが起こる。u16の'はく'をsj13に変換したらin-ptr,ileftは
   ２文字分更新されたがout-ptr,oleftは１文字分しか更新されなかった。
*/
char* U16ToEj(char* dst0,const uint16_t* src0,int src_len)
{
    return u16_to_mb(U16E_08,TOFU_E,dst0,src0,src_len);
}

//ucs2 --> canna ej
uint16_t* U16ToCej(uint16_t* dst,const uint16_t* src,int src_len)
{
    char *ej;

    ej = U16ToEj(NULL,src,src_len);
    dst = ToWc(dst,ej);
    free(ej);
    return dst;
}


//euc-jp --> utf8
char* EjToU8(char* dst,const char* src00)
{
    if(src00 == NULL)
	return NULL;

    size_t ileft=strlen(src00),oleft=EjLen(src00)*8;
    char *dst0,*src0,*src;

    src0 = src = strdup(src00);
    if(dst == NULL)
	dst = malloc(oleft+1);
    dst0 = dst;
    while(!conv(EU8_08,&src,&ileft,&dst,&oleft)){
	*(int32_t*)dst = TOFU_U8;
	dst += 3;
	oleft -= 3;
	if(*src == (char)0x8f){
	    ++src;
	    --ileft;
	}
	src += 2;
	ileft -= 2;
    }
    *dst = 0;
    free(src0);
    return dst0;
}

//ucs2 --> utf8
char* U16ToU8(char* dst0,const uint16_t* src0,int src_len)
{
    uint16_t *src,*src_orig;
    char *dst;
    size_t ileft,oleft;

    if(src0 == NULL)
	return NULL;
    if(src_len < 0)
	src_len = WcLen(src0);
    ileft = src_len*2;
    oleft = src_len*5;
    src = src_orig = memcpy(malloc(ileft+2),src0,ileft+2);
    if(dst0 == NULL)
	dst0 = malloc(oleft+1);
    dst = dst0;
    conv(U16U8,(char**)&src,&ileft,&dst,&oleft);
    *dst = 0;
    free(src_orig);
    return dst0;
}

//ucs2 --> shift-jisx0213
char* U16ToSj(char* out0,const uint16_t* in0,int in_len)
{
    return u16_to_mb(U16S,TOFU_S,out0,in0,in_len);
}

//ej --> sj
char* EjToSj(char* out,const char* in)
{
    uint16_t *u;
    
    u = EjToU16(NULL,in);
    out = U16ToSj(out,u,-1);
    free(u);
    return out;
}

//!!! EjToU16と一緒にするか？
//sjis --> ucs2
uint16_t* SjToU16(uint16_t* out0,const char* in0,size_t ileft)
{
    char *in,*in_orig;
    uint16_t *out;
    size_t oleft;

    in = in_orig = strdup(in0);
    if(ileft == (size_t)-1)
	ileft = strlen(in);
    oleft=ileft*2; //inが半角カナの場合outは２倍になる。ちゃんと文字数数える？

    if(out0 == NULL)
	out0 = malloc(oleft+2);
    out = out0;

    while(!conv(SU16,&in,&ileft,(char**)&out,&oleft)){
	*(out++) = TOFU_U2;
	oleft -= 2;
	in += 2;
	ileft -= 2;
    }
    *out = 0;
    free(in_orig);
    return out0;
}

//sjis --> ej
char* SjToEj(char* out,const char* in,int in_len)
{
    uint16_t *u = SjToU16(NULL,in,in_len);
    out = U16ToEj(out,u,-1);
    free(u);
    return out;
}

/*
  eucjpの全角ひらがなを半角カナにする
  戻り値：出力した文字数(1 or 2)
  dstにヌル文字がつくので,最大5バイト必要。
*/
int EjZen2Han(char* dst,const char* src)
{
    /*
      全角ひらがな('ぁ'(0xa4a1)...'��'(0xa4f4))→半角カナ
      1 1つ前と濁点
      2 ２つ前と半濁点
      3 半角のウと濁点
    */
    static uint16_t hira2hkana[]={
	'ｧ','ｱ','ｨ','ｲ','ｩ','ｳ','ｪ','ｴ','ｫ','ｵ','ｶ',1,'ｷ',1,'ｸ',1,'ｹ',1,'ｺ',1,
	'ｻ',1,'ｼ',1,'ｽ',1,'ｾ',1,'ｿ',1,'ﾀ',1,'ﾁ',1,'ｯ','ﾂ',1,'ﾃ',1,'ﾄ',1,'ﾅ',
	'ﾆ','ﾇ','ﾈ','ﾉ','ﾊ',1,2,'ﾋ',1,2,'ﾌ',1,2,'ﾍ',1,2,'ﾎ',1,2,'ﾏ','ﾐ','ﾑ',
	'ﾒ','ﾓ','ｬ','ﾔ','ｭ','ﾕ','ｮ','ﾖ','ﾗ','ﾘ','ﾙ','ﾚ','ﾛ','ゎ','ﾜ','ゐ','ゑ',
	'ｦ','ﾝ',3
    };

    union {
	uint8_t u1[4];
	uint16_t u2[2];
	uint32_t u4;
    } wc;
    int len=1,moji=1,idx;

    wc.u1[1] = *(src++);
    if((wc.u1[1] & 0x80) == 0){
	//ascii
	*(dst++) = wc.u1[1];
	*dst = 0;
	return 1;
    }

    wc.u1[0] = *(src++);
    switch(wc.u2[0]){
    case '　': wc.u1[1]=' '; break;
    case '！': wc.u1[1]='!'; break;
    case '”': wc.u1[1]='"'; break;
    case '＃': wc.u1[1]='#'; break;
    case '＄': wc.u1[1]='$'; break;
    case '％': wc.u1[1]='%'; break;
    case '＆': wc.u1[1]='&'; break;
    case '’': wc.u1[1]='\''; break;
    case '（': wc.u1[1]='('; break;
    case '）': wc.u1[1]=')'; break;
    case '＊': wc.u1[1]='*'; break;
    case '＋': wc.u1[1]='+'; break;
    case '，': wc.u1[1]=','; break;
    case '―': wc.u1[1]='-'; break;
    case '．': wc.u1[1]='.'; break;
    case '／': wc.u1[1]='/'; break;
    case '：': wc.u1[1]=':'; break;
    case '；': wc.u1[1]=';'; break;
    case '＜': wc.u1[1]='<'; break;
    case '＝': wc.u1[1]='='; break;
    case '＞': wc.u1[1]='>'; break;
    case '？': wc.u1[1]='?'; break;
    case '＠': wc.u1[1]='@'; break;
    case '［': wc.u1[1]='['; break;
    case '＼': wc.u1[1]='\\'; break;
    case '］': wc.u1[1]=']'; break;
    case '＾': wc.u1[1]='^'; break;
    case '＿': wc.u1[1]='_'; break;
    case '｀': wc.u1[1]='`'; break;
    case '｛': wc.u1[1]='{'; break;
    case '｜': wc.u1[1]='|'; break;
    case '｝': wc.u1[1]='}'; break;
    case '￣': wc.u1[1]='~'; break;

    case '。': wc.u2[0]='｡'; ++len; break;
    case '「': wc.u2[0]='｢'; ++len; break;
    case '」': wc.u2[0]='｣'; ++len; break;
    case '、': wc.u2[0]='､'; ++len; break;
    case '・': wc.u2[0]='･'; ++len; break;
    case '゛': wc.u2[0]='ﾞ'; ++len; break;
    case '゜': wc.u2[0]='ﾟ'; ++len; break;
    case 'ー': wc.u2[0]='ｰ'; ++len; break;
    case '０' ... 'ｚ':
	wc.u1[1]=(uint8_t)(wc.u2[0]-'０'+'0'); break;
    case 'ぁ' ... '��':
	++len;
	wc.u2[0] = hira2hkana[idx=wc.u2[0]-'ぁ'];
        switch(wc.u2[0]){
	case 1:
	    wc.u2[0] = hira2hkana[idx-1];
	    wc.u2[1] = 'ﾞ';
	    len += 2;
	    ++moji;
	    break;
	case 2:
	    wc.u2[0] = hira2hkana[idx-2];
	    wc.u2[1] = 'ﾟ';
	    len += 2;
	    ++moji;
	    break;
	case 3:
	    wc.u2[0] = 'ｳ';
	    wc.u2[1] = 'ﾞ';
	    len += 2;
	    ++moji;
	}
	break;
    default:
	fprintf(stderr,"%s:illegal code 0x%04x\n",__FUNCTION__,wc.u2[0]);
    }
    for(idx=0; idx<4; idx+=2){
	dst[idx] = wc.u1[idx+1];
	dst[idx+1] = wc.u1[idx];
    }
    dst[len] = 0;
    return moji;
}

/*
  eucjpの全角ひらがな --> 半角カナ
*/
char* ZenToHan(char* dst,const char* src)
{
    char *dst0;

    if(dst == NULL)
	dst = malloc(strlen(src)*4+1);
    dst0 = dst;

    while(*src != 0){
	dst += EjZen2Han(dst,src)*2;
	src += 2;
    }
    *dst = 0;
    return dst0;
}

/*
  eucjpの全角ひらがな --> 全角カタカナ
  src_len=文字数
*/
char* HiraToKata(char* dst,const char* src,int src_len)
{
    char *dst0;

    if(src_len < 0)
	src_len = INT_MAX;

    if(dst == NULL)
	dst = malloc(src_len<INT_MAX ? src_len*3+1 : strlen(src)+1);
    dst0 = dst;

    while(*src!=0 && src_len>0){
	switch((uint8_t)*src){
	case 0 ... 0x7f:
	    *(dst++) = *(src++);
	    break;
	case 0x8f:
	    memcpy(dst,src,3);
	    src += 3;
	    dst += 3;
	    break;
	case 0xa4: //ひらがな
	    *(dst++) = 0xa5;
	    ++src;
	    *(dst++) = *(src++);
	    break;
	default:
	    *(int16_t*)dst = *(int16_t*)src;
	    src += 2;
	    dst += 2;
	}
	--src_len;
    }
    *dst = 0;
    return dst0;
}

int MkDir(const char* p)
{
    char *pp;
    int r=0;

    if(p[0]=='/' && p[1]==0)
	return 1;

    pp = strdup(p);
    if(MkDir(dirname(pp))){
	r = (mkdir(p,0777)==0);
	if(r)
	    chmod(p,0777);
	else
	    if(errno==EEXIST)
		r=1;
    }
    free(pp);
    return r;
}

//(C) 2008 thomas
