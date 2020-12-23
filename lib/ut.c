// -*- coding:euc-jp -*-
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
#include <ctype.h>


static char* cur_to_u8(char* out,const char* in,int inlen);
static char* no_conv(char* out,const char* in,int inlen);


//e-a2a2 s-81a0 u-25a1 t-e296a1 '□'
#define TOFU_E	0xa2a2
#define TOFU_S	0xa081
#define TOFU_U2	0xa125
#define TOFU_U8	0xa196e2

//(from,to) primary,secondary
enum{ EU16_08,EU16_13,
      U16E_08,U16E_13,
      EU8_08,EU8_13,
      U16S,U16S_13,
      SU16,SU16_13,
      U8E_08,U8E_13,
      U16U8,CV_DUM0,
      U8U16,CV_DUM1,
      CNVMAX};
iconv_t Icv[CNVMAX];

extern iconv_t CurrentToUtf8;

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
	{EUCJP,UTF8},		{EUCX0213,UTF8},
	{UTF8,UTF16},		{NULL,NULL},
	{UTF16,UTF8},		{NULL,NULL},
    };
    for(unsigned n=0; n<ITEMS(Icv); ++n){
	if(cs[n][0]!=NULL){
	    Icv[n] = iconv_open(cs[n][0],cs[n][1]);
#ifdef __FreeBSD__
	    if(Icv[n] == (iconv_t)-1){
		for(int ft=0; ft<2; ++ft){
		    if(cs[n][ft]==EUCX0213)
			cs[n][ft]=EUCJP;
		    if(cs[n][ft]==SJIS13)
			cs[n][ft]=SJIS;
		}
		Icv[n] = iconv_open(cs[n][0],cs[n][1]);
	    }
#endif
	}
    }

    //カレントロケール→utf8
    char* loc = getenv("LC_CTYPE");
    if(loc == NULL)
	loc = getenv("LANG");
    if(loc == NULL || (loc=strchr(loc,'.')) == NULL)
	loc = strdup("utf8");
    else{
	loc = strdup(loc+1);
	for(char* lp; (lp=strchr(loc,'-'))!=NULL; StrDel(lp,0,1))
	    ;
    }
    if(strcasecmp(loc,"utf8") == 0)
	CurToU8 = no_conv;
    else if(strcasecmp(loc,"eucjp") == 0)
	CurToU8 = EjToU8;
    else if(strcasecmp(loc,"sjis") == 0)
	CurToU8 = SjToU8;
    else{
	CurrentToUtf8 = iconv_open("utf8",loc);
	CurToU8 = CurrentToUtf8!=(iconv_t)-1 ? cur_to_u8 : no_conv;
    }
    free(loc);
}

iconv_t CurrentToUtf8;
char* (*CurToU8)(char* out,const char* in,int inlen);
//カレントロケール→utf8
static char* cur_to_u8(char* out,const char* in,int inlen)
{
    size_t ileft = inlen<0 ? strlen(in) : inlen;
    char* ind = strndup(in,ileft);
    char* ind0 = ind;
    size_t oleft = ileft*3+1;
    if(out == NULL)
	out = malloc(oleft);
    char* out0 = out;
    iconv(CurrentToUtf8,&ind,&ileft,&out,&oleft);
    *out = 0;
    free(ind0);
    return out0;
}
static char* no_conv(char* out,const char* in,int inlen)
{
    if(inlen < 0)
	inlen = strlen(in);
    if(out == NULL)
	out = strndup(in,inlen);
    else
	*(strncpy(out,in,inlen)+inlen) = 0;
    return out;
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
size_t WcLen(const uint16_t* s)
{
    const uint16_t* s0 = s;

    if(s == NULL)
	return 0;

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

//スペース区切りのみに対応。
uint16_t* U16Tok(uint16_t** src)
{
    const int spc = ' '; //wejのときは(' '<<8)
    while(**src == spc)
	++ *src;
    if(**src == 0)
	return NULL;
    uint16_t* start = *src;
    while(**src!=0 && **src!=spc)
	++ *src;
    if(**src != 0)
	*((*src)++) = 0;
    return start;
}
    
/*
  weucjpのwchar文字列をchar文字列にする。
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
	    unsigned char *dp;
	    if((*src & 0xff) != 0){
		if((*src>>8) & 0x80){
		    *(uint16_t*)ArExpand(&dst,2) = *src;
		}else{ //jisx0212
		    *(dp = ArExpand(&dst,3)) = 0x8f;
		    *(uint16_t*)(dp+1) = *src|0x8000;
		}
	    }else{
		unsigned char uc = (unsigned char)(*src>>8);
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
  半角カナは0x8eがついていない(第１バイトは０) [r140 ??? 0x8eがないと文字化けする。]
  cannaはjisx0212を２バイトで扱うために第１バイトだけを変換するようだ。
  src_len<0のときdst_lenにヌル文字を含む。src_lenを指定したときはその範囲で処理した文字数になる。
  src==NULLのときはエラーとしてNULLを返す。
*/
uint16_t* ToWc(uint16_t* dst,int* dst_len,const char* src,int src_len)
{
    uint16_t *dst0,wc;

    if(src == NULL)
	return NULL;
    if(dst == NULL)
	dst = calloc(src_len<0 ? EjLen(src)+1:src_len,2);
    dst0 = dst;

    while(src_len>0 || (src_len<0 && *src!=0)){
	switch((uint8_t)*src){
	case 0x8e: //半角カナ
	    wc = (((uint16_t)*(++src))<<8)|0x8e; //??? 0x8eがないと文字化けするようだ。[r140]
	    ++src;
	    src_len -= 3;
	    break;
	case 0x8f: //0212
	    wc = *(uint16_t*)(++src) & 0x7fff;
	    src += 2;
	    src_len -= 2;
	    break;
	case 0xa1 ... 0xff: //0208
	    wc = *(uint16_t*)src;
	    src += 2;
	    src_len -= 2;
	    break;
	default: //ascii
	    wc = ((uint16_t)*(src++))<<8;
	    -- src_len;
	}
	*(dst++) = wc;
    }
    if(src_len < 0) //ヌル文字までの時は出力にヌルを付ける。
	*(dst++) = 0;
    if(dst_len != NULL)
	*dst_len = dst-dst0;
    return dst0;
}

uint16_t* WcDup(const uint16_t* src)
{
    return WcCpy(calloc(WcLen(src)+1,2),src);
}

/*
  utf16の全角文字をローマ字にする。
  変換できなかったときはNULLが返る。
*/
const char* U16Zen2Romaji(uint16_t u16)
{
    static const char* hira[]={//0x3041
	"xa"/*ぁ*/,	"a"/*あ*/,	"xi"/*ぃ*/,	"i"/*い*/,	"xu"/*ぅ*/,	"u"/*う*/,
 	"xe"/*ぇ*/,	"e"/*え*/,	"xo"/*ぉ*/,	"o"/*お*/,	"ka"/*か*/,	"ga"/*が*/,
 	"ki"/*き*/,	"gi"/*ぎ*/,	"ku"/*く*/,	"gu"/*ぐ*/,	"ke"/*け*/,	"ge"/*げ*/,
 	"ko"/*こ*/,	"go"/*ご*/,	"sa"/*さ*/,	"za"/*ざ*/,	"si"/*し*/,	"ji"/*じ*/,
 	"su"/*す*/,	"zu"/*ず*/,	"se"/*せ*/,	"ze"/*ぜ*/,	"so"/*そ*/,	"zo"/*ぞ*/,
 	"ta"/*た*/,	"da"/*だ*/,	"ti"/*ち*/,	"di"/*ぢ*/,	"xtu"/*っ*/,	"tu"/*つ*/,
 	"du"/*づ*/,	"te"/*て*/,	"de"/*で*/,	"to"/*と*/,	"do"/*ど*/,	"na"/*な*/,
 	"ni"/*に*/,	"nu"/*ぬ*/,	"ne"/*ね*/,	"no"/*の*/,	"ha"/*は*/,	"ba"/*ば*/,
 	"pa"/*ぱ*/,	"hi"/*ひ*/,	"bi"/*び*/,	"pi"/*ぴ*/,	"fu"/*ふ*/,	"bu"/*ぶ*/,
 	"pu"/*ぷ*/,	"he"/*へ*/,	"be"/*べ*/,	"pe"/*ぺ*/,	"ho"/*ほ*/,	"bo"/*ぼ*/,
 	"po"/*ぽ*/,	"ma"/*ま*/,	"mi"/*み*/,	"mu"/*む*/,	"me"/*め*/,	"mo"/*も*/,
 	"xya"/*ゃ*/,	"ya"/*や*/,	"xyu"/*ゅ*/,	"yu"/*ゆ*/,	"xyo"/*ょ*/,	"yo"/*よ*/,
 	"ra"/*ら*/,	"ri"/*り*/,	"ru"/*る*/,	"re"/*れ*/,	"ro"/*ろ*/,	"xwa"/*ゎ*/,
 	"wa"/*わ*/,	"wi"/*ゐ*/,	"we"/*ゑ*/,	"wo"/*を*/,	"nn"/*ん*/,
	"vu"/*う゛*/, 	"xka"/*(か)*/,	"xke"/*(け)*/ //0x3096
    };
    static char ascii_buf[2]={0,0};

    const char* rmj;
    switch(u16){
    case 0 ... 0x7f: //ascii
	ascii_buf[0] = u16;
	rmj = ascii_buf;
	break;
    case 0x2018: //左シングルクォーテーション
	rmj = "`";
	break;
    case 0x2019: //右シングルクォーテーション
	rmj = "'";
	break;
    case 0x201d: //右ダブルクォーテーション
	rmj = "\"";
	break;
    case 0x3000: //全角スペース
	rmj = " ";
	break;
    case 0x3001: //、
	rmj = ",";
	break;
    case 0x3002: //。
	rmj = ".";
	break;
    case 0x300c: //「
	rmj = "[";
	break;
    case 0x300d: //」
	rmj = "]";
	break;
    case 0x30a1 ... 0x30f6: //カタカナ。ひらがなの処理に続く。
	u16 -= 0x60;
    case 0x3041 ... 0x3096: //ひらがな
	rmj = hira[u16-0x3041];
	break;
    case 0x30fc: //Katakana-Hiragana Prolonged Sound Mark
	rmj = "-";
	break;
    case 0xff01 ... 0xff5e: //全角の記号、数字、アルファベット
	ascii_buf[0] = u16-0xff01+0x21;
	rmj = ascii_buf;
	break;
    case 0xffe3:
	rmj = "~";
	break;
    default:
	rmj = NULL;
    }
    return rmj;
}

#if 0
/*
  eucjpの半角カナ --> 全角ひらがな
  syn=濁点を合成する時true
  zen_asc=asciiを全角にするときtrue
  戻り値：次のdst
	use_src=使ったバイト数。濁点の合成で２文字使うこともある
  dstには最大３バイト必要
*//*
char* EjHan2Zen(char* dst,const char* src0,int* use_src,bool syn,bool zen_asc)
{
    static const char as20[]=
	"　！”＃＄％＆’"	"（）＊＋，−．／"	//20
	"０１２３４５６７"	"８９：；＜＝＞？"	//30
	"＠ＡＢＣＤＥＦＧ"	"ＨＩＪＫＬＭＮＯ"	//40
	"ＰＱＲＳＴＵＶＷ"	"ＸＹＺ［＼］＾＿"	//50
	"‘ａｂｃｄｅｆｇ"	"ｈｉｊｋｌｍｎｏ"	//60
	"ｐｑｒｓｔｕｖｗ"	"ｘｙｚ｛｜｝\xa1\xc1□"	//70
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
  */
/*
  eucjpの半角カナ --> 全角ひらがな
  srclen=バイト数
  syn=濁点を合成する時true
  zen_asc=asciiを全角にするときtrue
  asciiも全角文字にするためdstがsrcより長くなる可能性がある。
*//*
char* HanToZen(char* dst,const char* src,int srclen,bool syn,bool zen_asc)
{
    char *dst0;
    int use_src;

    if(dst == NULL)
	dst = malloc((srclen>=0 ? srclen : EjLen(src)*3)+1);
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
*/
#endif

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

//euc-jp --> utf16
//dst0がNULLのときはmallocで確保する。
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

//weucjp --> utf16
//dst0がNULLのときはmallocで確保する。
uint16_t* WejToU16(uint16_t* dst,const uint16_t* src)
{
    char* ej = ToMb(src);
    dst = EjToU16(dst,ej);
    free(ej);
    return dst;
}

/*
  utf16 --> ej or sj
  dst_lenは出力した文字数。src_len<0のときはヌル文字の分も含む。
  src==NULLのときはエラーとしてNULLを返す。
*/
char* u16_to_mb(int cv,int tofu,char* dst0,int* dst_len,const uint16_t* src0,int src_len)
{
    uint16_t *src,*src_orig;
    char *dst;
    size_t ileft,oleft;

    if(src0 == NULL)
	return NULL;
    if(src_len < 0)
	src_len = WcLen(src0)+1; //ヌル文字を含める。
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
    if(dst_len != NULL)
	*dst_len = dst-dst0;
    free(src_orig);
    return dst0;
}

/* utf16 -->euc-jp
   ??? euc-jisx0213で'か'行１文字だけの時、何も出力されずin-ptr,ileftだけ更新される。
   euc-jpのときは問題ない。わけがわからない。
   iconv(1)ではどうやってるんだろうか？
   shift-jisx0213でも同様のことが起こる。u16の'はく'をsj13に変換したらin-ptr,ileftは
   ２文字分更新されたがout-ptr,oleftは１文字分しか更新されなかった。
*/
char* U16ToEj(char* dst,int* dst_len,const uint16_t* src,int src_len)
{
    return u16_to_mb(U16E_08,TOFU_E,dst,dst_len,src,src_len);
}

//utf16 --> weucjp
uint16_t* U16ToWej(uint16_t* dst,int* dst_len,const uint16_t* src,int src_len)
{
    int ejlen;
    
    char* ej = U16ToEj(NULL,&ejlen,src,src_len);
    dst = ToWc(dst,dst_len,ej,ejlen);
    free(ej);
    return dst;
}


//euc-jp --> utf8
char* EjToU8(char* dst,const char* src00,int ilen)
{
    if(src00 == NULL)
	return NULL;

    size_t ileft = ilen<0 ? strlen(src00) : ilen;
    size_t oleft=EjLen(src00)*8;
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

//utf16 --> utf8
//src_len<0のときdst_lenにはヌル文字も含む。
char* U16ToU8(char* dst0,int* dst_len,const uint16_t* src0,int src_len)
{
    uint16_t *src,*src_orig;

    if(src0 == NULL)
	return NULL;
    if(src_len < 0)
	src_len = WcLen(src0)+1; //ヌル文字を含める。
    size_t ileft = src_len*2;
    size_t oleft = src_len*5;
    src = src_orig = memcpy(malloc(ileft+2),src0,ileft+2);
    if(dst0 == NULL)
	dst0 = malloc(oleft+1);
    char* dst = dst0;
    conv(U16U8,(char**)&src,&ileft,&dst,&oleft);
    if(dst_len != NULL)
	*dst_len = dst-dst0;
    free(src_orig);
    return dst0;
}

#if 0
//utf16 --> shift-jisx0213
/*
char* U16ToSj(char* out0,const uint16_t* in0,int in_len)
{
    return u16_to_mb(U16S,TOFU_S,out0,NULL,in0,in_len);
}

//ej --> sj
char* EjToSj(char* out,const char* in)
{
    uint16_t* u = EjToU16(NULL,in);
    out = U16ToSj(out,u,-1);
    free(u);
    return out;
}
*/
#endif

//!!! EjToU16と一緒にするか？
//sjis --> utf16
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
    uint16_t* u = SjToU16(NULL,in,in_len);
    out = U16ToEj(out,NULL,u,-1);
    free(u);
    return out;
}

//sjis --> utf8
char* SjToU8(char* out,const char* in,int in_len)
{
    uint16_t* u = SjToU16(NULL,in,in_len);
    out = U16ToU8(out,NULL,u,-1);
    free(u);
    return out;
}

#if 0
/*
//sjis --> wej
uint16_t* SjToWej(uint16_t* out,const char* in,int in_len)
{
    char* ej = SjToEj(NULL,in,in_len);
    out = ToWc(out,NULL,ej,-1);
    free(ej);
    return out;
}
*/

/*
  eucjpの全角ひらがなを半角カナにする
  戻り値：出力した文字数(1 or 2)
  dstにヌル文字がつくので,最大5バイト必要。nullのときは出力しない。
*//*
int EjZen2Han(char* dst,const char* src)
{
    //全角ひらがな('ぁ'(0xa4a1)...'��'(0xa4f4))→半角カナ
    //1 1つ前と濁点
    //2 ２つ前と半濁点
    //3 半角のウと濁点
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
    char dum_dst[5];

    if(dst == NULL)
	dst = dum_dst;
    
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
  */
/*
  eucjpの全角ひらがな --> 半角カナ
*//*
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
  */

/*
  eucjpの全角ひらがな --> 全角カタカナ
  src_len=文字数
*//*
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
  */
#endif

int MkDir(const char* p0)
{
    if(p0[0]=='/' && p0[1]==0){
	return 1;
    }

    int r=0;
    char* p = strdup(p0);
    char* pp = strdup(p0);
    if(MkDir(dirname(pp))){
	r = (mkdir(p,0777)==0);
	if(r)
	    chmod(p,0777);
	else
	    if(errno==EEXIST)
		r=1;
    }
    free(p);
    free(pp);
    return r;
}

/*
  フラグを文字列化する。
  bitsの最後の要素のdescはNULLにすること。
  bufの初期化はしない。要素サイズは1にすること。 bufがNULLのときはmallocで確保する。
 */
Array* FlagStr(unsigned flags,const BitDesc* bits,Array* buf)
{
    if(buf == NULL)
	buf = ArNew(NULL,1,NULL);
    if(flags == 0){
	ArPrint(buf,"(none)");
    }else{
	const char* sep="";
	for(unsigned n=0; bits[n].desc!=NULL; ++n){
	    if((flags & bits[n].mask)){
		ArPrint(buf,"%s%s",sep,bits[n].desc);
		flags &= ~bits[n].mask;
		sep="|";
	    }
	}
	if(flags!=0){
	    ArPrint(buf,"%s0x%x",sep,flags);
	}
    }
    return buf;
}

/*
  u16半角カナ文字列で、src[1]に濁点／半濁点があってsrc[0]がそれと合成できるとき(カ゛→ガ)、
  濁点なら1、半濁点なら2を返す。それ以外の時は0を返す
  半角カナ/ascii以外、ヌルの時は-1を返す。
  u16なので'ヲ゛'も合成される。ejに変換すると豆腐文字になるので注意すること。
  kata_hira:ひらがなにする時1,カタカナにする時0
  combineがfalseのとき全部合成できないとする。つまり文字種の判定のみになる。
*/
int U16CombineHan(const uint16_t* src,int kata_hira,bool combine)
{
    #define D 1
    #define H 2
    const char hira_table[]={ //u-ff61  'ヲ’と'ワ'は０
	0/*｡*/,	0/*｢*/,	0/*｣*/,	0/*､*/,	0/*･*/,
	0/*ｦ*/,	0/*ｧ*/,	0/*ｨ*/,	0/*ｩ*/,	0/*ｪ*/,	0/*ｫ*/,	0/*ｬ*/,	0/*ｭ*/,	0/*ｮ*/,	0/*ｯ*/,	0/*ｰ*/,
 	0/*ｱ*/,	0/*ｲ*/,	D/*ｳ*/,	0/*ｴ*/,	0/*ｵ*/,	D/*ｶ*/,	D/*ｷ*/,	D/*ｸ*/,	D/*ｹ*/,	D/*ｺ*/,	D/*ｻ*/,
 	D/*ｼ*/,	D/*ｽ*/,	D/*ｾ*/,	D/*ｿ*/,	D/*ﾀ*/,	D/*ﾁ*/,	D/*ﾂ*/,	D/*ﾃ*/,	D/*ﾄ*/,	0/*ﾅ*/,	0/*ﾆ*/,
 	0/*ﾇ*/,	0/*ﾈ*/,	0/*ﾉ*/,	D|H/*ﾊ*/,	D|H/*ﾋ*/,	D|H/*ﾌ*/,	D|H/*ﾍ*/,
 	D|H/*ﾎ*/,	0/*ﾏ*/,	0/*ﾐ*/,	0/*ﾑ*/,	0/*ﾒ*/,	0/*ﾓ*/,	0/*ﾔ*/,	0/*ﾕ*/,	0/*ﾖ*/,	0/*ﾗ*/,
 	0/*ﾘ*/,	0/*ﾙ*/,	0/*ﾚ*/,	0/*ﾛ*/,	0/*ﾜ*/,	0/*ﾝ*/,	0/*ﾞ*/,	0/*ﾟ*/ /*u-ff9f*/
    };
    const char kata_table[]={ //u-ff61  'ヲ’と'ワ'はD
	0/*｡*/,	0/*｢*/,	0/*｣*/,	0/*､*/,	0/*･*/,
	D/*ｦ*/,	0/*ｧ*/,	0/*ｨ*/,	0/*ｩ*/,	0/*ｪ*/,	0/*ｫ*/,	0/*ｬ*/,	0/*ｭ*/,	0/*ｮ*/,	0/*ｯ*/,	0/*ｰ*/,
 	0/*ｱ*/,	0/*ｲ*/,	D/*ｳ*/,	0/*ｴ*/,	0/*ｵ*/,	D/*ｶ*/,	D/*ｷ*/,	D/*ｸ*/,	D/*ｹ*/,	D/*ｺ*/,	D/*ｻ*/,
 	D/*ｼ*/,	D/*ｽ*/,	D/*ｾ*/,	D/*ｿ*/,	D/*ﾀ*/,	D/*ﾁ*/,	D/*ﾂ*/,	D/*ﾃ*/,	D/*ﾄ*/,	0/*ﾅ*/,	0/*ﾆ*/,
 	0/*ﾇ*/,	0/*ﾈ*/,	0/*ﾉ*/,	D|H/*ﾊ*/,	D|H/*ﾋ*/,	D|H/*ﾌ*/,	D|H/*ﾍ*/,
 	D|H/*ﾎ*/,	0/*ﾏ*/,	0/*ﾐ*/,	0/*ﾑ*/,	0/*ﾒ*/,	0/*ﾓ*/,	0/*ﾔ*/,	0/*ﾕ*/,	0/*ﾖ*/,	0/*ﾗ*/,
 	0/*ﾘ*/,	0/*ﾙ*/,	0/*ﾚ*/,	0/*ﾛ*/,	D/*ﾜ*/,	0/*ﾝ*/,	0/*ﾞ*/,	0/*ﾟ*/ /*u-ff9f*/
    };
    const char* voiced_table[]={kata_table,hira_table};
    
    if(src[0]>0 && src[0]<0x80) //ascii
	return 0;
    if(src[0]<0xff61 || src[0]>0xff9f)
	return -1;
    char dh = voiced_table[kata_hira][src[0]-0xff61];
    if(combine && (dh & D) && src[1]==U16HAN_VOICEDSOUNDMARK)
	return 1;
    if(combine && (dh & H) && src[1]==U16HAN_SEMIVOICEDSOUNDMARK)
	return 2;
    return 0;
    #undef D
    #undef H
}

/*
  u16半角カナ→u16全角かな/カナ。combineがtrueなら濁点は合成する。
  srclenを指定したときはヌル文字は無視する。
  u16なので'ヲ゛'も合成される。ejに変換すると豆腐文字になるので注意すること。'う゛'もejにはない。
  kata_hira:ひらがなの時1,カタカナの時0
 */
uint16_t* U16HanToZen(uint16_t* dst,int* dstlen,const uint16_t* src,int srclen,int kata_hira,bool combine)
{
    if(src==NULL || srclen==0)
	return NULL;
    
    const uint16_t hk2zk[]={ //u-ff61 カタカナ
	0x3002/*｡*/,	0x300c/*｢*/,	0x300d/*｣*/,	0x3001/*､*/,	0x30fb/*･*/,
	0x30f2/*ｦ*/,	0x30a1/*ｧ*/,	0x30a3/*ｨ*/,	0x30a5/*ｩ*/,	0x30a7/*ｪ*/,	0x30a9/*ｫ*/,
	0x30e3/*ｬ*/,	0x30e5/*ｭ*/,	0x30e7/*ｮ*/,	0x30c3/*ｯ*/,	0x30fc/*ｰ*/, 	0x30a2/*ｱ*/,
	0x30a4/*ｲ*/,	0x30a6/*ｳ*/,	0x30a8/*ｴ*/,	0x30aa/*ｵ*/,	0x30ab/*ｶ*/,	0x30ad/*ｷ*/,
	0x30af/*ｸ*/,	0x30b1/*ｹ*/,	0x30b3/*ｺ*/,	0x30b5/*ｻ*/, 	0x30b7/*ｼ*/,	0x30b9/*ｽ*/,
	0x30bb/*ｾ*/,	0x30bd/*ｿ*/,	0x30bf/*ﾀ*/,	0x30c1/*ﾁ*/,	0x30c4/*ﾂ*/,	0x30c6/*ﾃ*/,
	0x30c8/*ﾄ*/,	0x30ca/*ﾅ*/,	0x30cb/*ﾆ*/, 	0x30cc/*ﾇ*/,	0x30cd/*ﾈ*/,	0x30ce/*ﾉ*/,
	0x30cf/*ﾊ*/,	0x30d2/*ﾋ*/,	0x30d5/*ﾌ*/,	0x30d8/*ﾍ*/, 	0x30db/*ﾎ*/,	0x30de/*ﾏ*/,
	0x30df/*ﾐ*/,	0x30e0/*ﾑ*/,	0x30e1/*ﾒ*/,	0x30e2/*ﾓ*/,	0x30e4/*ﾔ*/,	0x30e6/*ﾕ*/,
	0x30e8/*ﾖ*/,	0x30e9/*ﾗ*/, 	0x30ea/*ﾘ*/,	0x30eb/*ﾙ*/,	0x30ec/*ﾚ*/,	0x30ed/*ﾛ*/,
	0x30ef/*ﾜ*/,	0x30f3/*ﾝ*/, 	0x309b/*ﾞ*/, 	0x309c/*ﾟ*/ /*u-ff9f*/
    };
    const uint16_t hk2zh[]={ //u-ff61 ひらがな   長音、濁点、半濁点、記号はカタカナと同じ。
	0x3002/*｡*/,	0x300c/*｢*/,	0x300d/*｣*/,	0x3001/*､*/,	0x30fb/*･*/,
	0x3092/*ｦ*/,	0x3041/*ｧ*/,	0x3043/*ｨ*/,	0x3045/*ｩ*/,	0x3047/*ｪ*/,	0x3049/*ｫ*/,
	0x3083/*ｬ*/,	0x3085/*ｭ*/,	0x3087/*ｮ*/,	0x3063/*ｯ*/,	0x30fc/*ｰ*/, 	0x3042/*ｱ*/,
	0x3044/*ｲ*/,	0x3046/*ｳ*/,	0x3048/*ｴ*/,	0x304a/*ｵ*/,	0x304b/*ｶ*/,	0x304d/*ｷ*/,
	0x304f/*ｸ*/,	0x3051/*ｹ*/,	0x3053/*ｺ*/,	0x3055/*ｻ*/, 	0x3057/*ｼ*/,	0x3059/*ｽ*/,
	0x305b/*ｾ*/,	0x305d/*ｿ*/,	0x305f/*ﾀ*/,	0x3061/*ﾁ*/,	0x3064/*ﾂ*/,	0x3066/*ﾃ*/,
	0x3068/*ﾄ*/,	0x306a/*ﾅ*/,	0x306b/*ﾆ*/, 	0x306c/*ﾇ*/,	0x306d/*ﾈ*/,	0x306e/*ﾉ*/,
	0x306f/*ﾊ*/,	0x3072/*ﾋ*/,	0x3075/*ﾌ*/,	0x3078/*ﾍ*/, 	0x307b/*ﾎ*/,	0x307e/*ﾏ*/,
	0x307f/*ﾐ*/,	0x3080/*ﾑ*/,	0x3081/*ﾒ*/,	0x3082/*ﾓ*/,	0x3084/*ﾔ*/,	0x3086/*ﾕ*/,
	0x3088/*ﾖ*/,	0x3089/*ﾗ*/, 	0x308a/*ﾘ*/,	0x308b/*ﾙ*/,	0x308c/*ﾚ*/,	0x308d/*ﾛ*/,
	0x308f/*ﾜ*/,	0x3093/*ﾝ*/, 	0x309b/*ﾞ*/, 	0x309c/*ﾟ*/ /*u-ff9f*/
    };
    const uint16_t* hk2z[]={hk2zk,hk2zh};
    const uint16_t daku[]={ //u-ff61
	0/*｡*/,		0/*｢*/,		0/*｣*/,		0/*､*/,		0/*･*/,
	0x30fa/*ｦ*/,	0/*ｧ*/,		0/*ｨ*/,		0/*ｩ*/,		0/*ｪ*/,		0/*ｫ*/,
	0/*ｬ*/,		0/*ｭ*/,		0/*ｮ*/,		0/*ｯ*/,		0/*ｰ*/, 	0/*ｱ*/,
	0/*ｲ*/,		0x30f4/*ｳ*/,	0/*ｴ*/,		0/*ｵ*/,		0x30ac/*ｶ*/,	0x30ae/*ｷ*/,
	0x30b0/*ｸ*/,	0x30b2/*ｹ*/,	0x30b4/*ｺ*/,	0x30b6/*ｻ*/, 	0x30b8/*ｼ*/,	0x30ba/*ｽ*/,
	0x30bc/*ｾ*/,	0x30be/*ｿ*/,	0x30c0/*ﾀ*/,	0x30c2/*ﾁ*/,	0x30c5/*ﾂ*/,	0x30c7/*ﾃ*/,
	0x30c9/*ﾄ*/,	0/*ﾅ*/,		0/*ﾆ*/,		0/*ﾇ*/,		0/*ﾈ*/,		0/*ﾉ*/,
	0x30d0/*ﾊ*/,	0x30d3/*ﾋ*/,	0x30d6/*ﾌ*/,	0x30d9/*ﾍ*/, 	0x30dc/*ﾎ*/,	0/*ﾏ*/,
	0/*ﾐ*/,		0/*ﾑ*/,		0/*ﾒ*/,		0/*ﾓ*/,		0/*ﾔ*/,		0/*ﾕ*/,
	0/*ﾖ*/,		0/*ﾗ*/, 	0/*ﾘ*/,		0/*ﾙ*/,		0/*ﾚ*/,		0/*ﾛ*/,
	0x30f7/*ﾜ*/,	0/*ﾝ*/, 	0/*ﾞ*/, 	0/*ﾟ*/ /*u-ff9f*/
    };
    const uint16_t handaku[]={ //u-ff61
	0/*｡*/,		0/*｢*/,		0/*｣*/,		0/*､*/,		0/*･*/,
	0/*ｦ*/,		0/*ｧ*/,		0/*ｨ*/,		0/*ｩ*/,		0/*ｪ*/,		0/*ｫ*/,
	0/*ｬ*/,		0/*ｭ*/,		0/*ｮ*/,		0/*ｯ*/,		0/*ｰ*/, 	0/*ｱ*/,
	0/*ｲ*/,		0/*ｳ*/,		0/*ｴ*/,		0/*ｵ*/,		0/*ｶ*/,		0/*ｷ*/,
	0/*ｸ*/,		0/*ｹ*/,		0/*ｺ*/,		0/*ｻ*/, 	0/*ｼ*/,		0/*ｽ*/,
	0/*ｾ*/,		0/*ｿ*/,		0/*ﾀ*/,		0/*ﾁ*/,		0/*ﾂ*/,		0/*ﾃ*/,
	0/*ﾄ*/,		0/*ﾅ*/,		0/*ﾆ*/,		0/*ﾇ*/,		0/*ﾈ*/,		0/*ﾉ*/,
	0x30d1/*ﾊ*/,	0x30d4/*ﾋ*/,	0x30d7/*ﾌ*/,	0x30da/*ﾍ*/, 	0x30dd/*ﾎ*/,	0/*ﾏ*/,
	0/*ﾐ*/,		0/*ﾑ*/,		0/*ﾒ*/,		0/*ﾓ*/,		0/*ﾔ*/,		0/*ﾕ*/,
	0/*ﾖ*/,		0/*ﾗ*/, 	0/*ﾘ*/,		0/*ﾙ*/,		0/*ﾚ*/,		0/*ﾛ*/,
	0/*ﾜ*/,		0/*ﾝ*/, 	0/*ﾞ*/, 	0/*ﾟ*/ /*u-ff9f*/
    };

    if(dst == NULL)
	dst = calloc((srclen>0 ? srclen:WcLen(src))+1,2);
    uint16_t* dst0 = dst;

    const uint16_t begin=0xff61;
    uint16_t kata_to_hira = 0x60*kata_hira; //カナ→ひら へのオフセット
    uint16_t let;
    while(let=*src, srclen>0 || (srclen<0 && let!=0)){
	switch(U16CombineHan(src++,kata_hira,combine)){
	case 0:
	    if(let == 0x20) //space
		*dst = 0x3000;
	    else if(let < 0x80) //ascii !!!コントロール文字は考慮していない。
		*dst = let-0x20+0xff00;
	    else //半角カナ
		*dst = hk2z[kata_hira][let-begin];
	    break;
	case 1:
	    *dst = daku[let-begin] - kata_to_hira;
	    src++;
	    break;
	case 2:
	    *dst = handaku[let-begin] - kata_to_hira;
	    src++;
	    break;
	default:
	    *dst = let;
	}
	++dst;
	--srclen;
    }
    *dst = 0; //合成で短くなる可能性があるのでヌル文字は入れておく。
    if(dstlen != NULL)
	*dstlen = dst-dst0;
    return dst0;
}

/*
  u16の全角ひらがな/カタカナ→半角カナ
  srclen<0のときdstlenにヌル文字を含む。srclenを指定したときはその範囲で処理した文字数になる。
*/
uint16_t* U16ZenToHan(uint16_t* dst,int* dstlen,const uint16_t* src,int srclen)
{
    if(src == NULL)
	return NULL;
    
    const int errlet=0xfffd;
    const uint32_t zenkana_to_han[]={ //濁点／半濁点があれば上１６ビットにコードを入れる。
	0xff67/*ぁ 0x3041*/,
	0xff71/*あ*/,	0xff68/*ぃ*/,	0xff72/*い*/,	0xff69/*ぅ*/,	0xff73/*う*/,
 	0xff6a/*ぇ*/,	0xff74/*え*/,	0xff6b/*ぉ*/,	0xff75/*お*/, 	0xff76/*か*/,
	0xff76|(U16HAN_VOICEDSOUNDMARK<<16)/*が*/,	0xff77/*き*/,
 	0xff77|(U16HAN_VOICEDSOUNDMARK<<16)/*ぎ*/,	0xff78/*く*/,
	0xff78|(U16HAN_VOICEDSOUNDMARK<<16)/*ぐ*/, 	0xff79/*け*/,
 	0xff79|(U16HAN_VOICEDSOUNDMARK<<16)/*げ*/,	0xff7a/*こ*/,
 	0xff7a|(U16HAN_VOICEDSOUNDMARK<<16)/*ご*/,	0xff7b/*さ*/,
 	0xff7b|(U16HAN_VOICEDSOUNDMARK<<16)/*ざ*/,	0xff7c/*し*/,
 	0xff7c|(U16HAN_VOICEDSOUNDMARK<<16)/*じ*/,	0xff7d/*す*/,
 	0xff7d|(U16HAN_VOICEDSOUNDMARK<<16)/*ず*/,	0xff7e/*せ*/,
 	0xff7e|(U16HAN_VOICEDSOUNDMARK<<16)/*ぜ*/,	0xff7f/*そ*/,
 	0xff7f|(U16HAN_VOICEDSOUNDMARK<<16)/*ぞ*/,	0xff80/*た*/,
 	0xff80|(U16HAN_VOICEDSOUNDMARK<<16)/*だ*/,	0xff81/*ち*/,
 	0xff81|(U16HAN_VOICEDSOUNDMARK<<16)/*ぢ*/,	0xff6f/*っ*/,
 	0xff82/*つ*/,	0xff82|(U16HAN_VOICEDSOUNDMARK<<16)/*づ*/,	0xff83/*て*/,
 	0xff83|(U16HAN_VOICEDSOUNDMARK<<16)/*で*/,	0xff84/*と*/,
 	0xff84|(U16HAN_VOICEDSOUNDMARK<<16)/*ど*/,	0xff85/*な*/,	0xff86/*に*/,
 	0xff87/*ぬ*/,	0xff88/*ね*/,	0xff89/*の*/,	0xff8a/*は*/,
 	0xff8a|(U16HAN_VOICEDSOUNDMARK<<16)/*ば*/,
	0xff8a|(U16HAN_SEMIVOICEDSOUNDMARK<<16)/*ぱ*/,	0xff8b/*ひ*/,
 	0xff8b|(U16HAN_VOICEDSOUNDMARK<<16)/*び*/,
 	0xff8b|(U16HAN_SEMIVOICEDSOUNDMARK<<16)/*ぴ*/,	0xff8c/*ふ*/,
 	0xff8c|(U16HAN_VOICEDSOUNDMARK<<16)/*ぶ*/,
 	0xff8c|(U16HAN_SEMIVOICEDSOUNDMARK<<16)/*ぷ*/,	0xff8d/*へ*/,
 	0xff8d|(U16HAN_VOICEDSOUNDMARK<<16)/*べ*/,
 	0xff8d|(U16HAN_SEMIVOICEDSOUNDMARK<<16)/*ぺ*/,	0xff8e/*ほ*/,
 	0xff8e|(U16HAN_VOICEDSOUNDMARK<<16)/*ぼ*/,
 	0xff8e|(U16HAN_SEMIVOICEDSOUNDMARK<<16)/*ぽ*/,	0xff8f/*ま*/,	0xff90/*み*/,
	0xff91/*む*/,	0xff92/*め*/,	0xff93/*も*/,	0xff6c/*ゃ*/,	0xff94/*や*/,
 	0xff6d/*ゅ*/,	0xff95/*ゆ*/,	0xff6e/*ょ*/,	0xff96/*よ*/,	0xff97/*ら*/,
 	0xff98/*り*/,	0xff99/*る*/,	0xff9a/*れ*/,	0xff9b/*ろ*/, 	errlet/*ゎ*/,
 	0xff9c/*わ*/, 	errlet/*ゐ→<?>*/, 		errlet/*ゑ→<?>*/,
	0xff66/*を*/, 	0xff9d/*ん*/,
 	0xff73|(U16HAN_VOICEDSOUNDMARK<<16)/*う゛ 0x3094*/,
    };

    if(dst == NULL)
	dst = calloc((srclen>0 ? srclen:WcLen(src))*2+1,2); //濁点のため２倍確保しておく。
    uint16_t* dst0 = dst;
    uint16_t let;
    while(let=*(src++), srclen>0 || (srclen<0 && let!=0)){
	switch(let){
	case 0x30a1 ... 0x30f4:/*ァ..ヴ*/
	    let -= 0x60;
	case 0x3041 ... 0x3094:/*ぁ..う゛*/{
	    uint32_t z = zenkana_to_han[let-0x3041];
	    *dst = (z & 0xffff);
	    if((z & 0xffff0000) != 0)
		*(++dst) = (z>>16);
	    break;
	}
	case 0x3001:/*、*/
	    *dst = 0xff64;
	    break;
	case 0x3002:/*。*/
	    *dst = 0xff61;
	    break;
	case 0x300c:/*「*/
	    *dst = 0xff62;
	    break;
	case 0x300d:/*」*/
	    *dst = 0xff63;
	    break;
	case 0x30fb: /*・*/
	    *dst = 0xff65;
	    break;
	case 0x309b:/* ゛ */
	    *dst = U16HAN_VOICEDSOUNDMARK;
	    break;
	case 0x309c:/*  ゜ */
	    *dst = U16HAN_SEMIVOICEDSOUNDMARK;
	    break;
	case 0x30fc:/*ー*/
	    *dst = 0xff70;
	    break;
	case 0x3000:/*　*/
	    *dst = 0x20;
	    break;
	case 0xff01 ... 0xff5e:/*！ .. ~*/
	    *dst = (let&0xff)+0x20;
	    break;
	default:
	    *dst = errlet/*<?>*/;
	}
	--srclen;
	++dst;
    }
    if(srclen < 0)
	*(dst++) = 0;
    if(dstlen != NULL)
	*dstlen = dst-dst0;
    return dst0;
}

//utf8 --> euc-jp
char* U8ToEj(char* dst,const char* src)
{
    if(src == NULL)
	return NULL;

    size_t ileft=strlen(src),oleft=ileft+1;
    char *dst0,*src0,*srcp;

    src0 = srcp = strdup(src);
    if(dst == NULL)
	dst = malloc(oleft+1);
    dst0 = dst;
    while(!conv(U8E_08,&srcp,&ileft,&dst,&oleft)){
	*(dst++) = (char)(TOFU_E & 0xff);
	*(dst++) = (char)(TOFU_E >> 8);
	oleft -= 2;
	char* srcerr = srcp;
	srcp = ForwardU8(srcp,1);
	ileft -= srcp-srcerr;
    }
    *dst = 0;
    free(src0);
    return dst0;
}

//utf8文字列の先頭からn文字移動する。
char* ForwardU8(const char* str,int n)
{
    while(--n >= 0 && *str!=0){
	unsigned char ch = *(str++);
	if((ch & 0x80)!=0){
	    while(((ch <<= 1) & 0x80) != 0)
		++str;
	}
    }
    return (char*)str;
}

uint16_t* U8ToU16(uint16_t* out,const char* in)
{
    if(in == NULL)
	return NULL;
    
    size_t ileft = strlen(in);
    size_t oleft = ileft*2;
    if(out == NULL)
	out = calloc(ileft+1,2);
    uint16_t* out0 = out;
    char* inwk = strdup(in);
    char* inwk0 = inwk;
    conv(U8U16,&inwk,&ileft,(char**)&out,&oleft);
    *out = 0;
    free(inwk0);
    return out0;
}

char* StrDel(char* str,int pos,int len)
{
    memmove(str+pos,str+pos+len,strlen(str+pos+len)+1);
    return str;
}

//(C) 2008 thomas
