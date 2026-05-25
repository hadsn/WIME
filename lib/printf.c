#include <stdio.h>
#include <printf.h>
#include <stdlib.h>
#include <string.h>
#include "ut.h"

/*
  wej	S
  u8	U
  u16	W
  dump  <len>[.<bs>]D  #フラグ=10進数  bs=1,2,4  bsなし=1バイト
*/

/*
  wchar_tのメンバを32bitに固定したもの。
  winegccでは-fshort-wcharがデフォルトになるが、printf_infoのwchar_tのメンバ(specなど)の大きさが変わり、
  printf_info自体の大きさも変わる。そのため各フラグの値の読み出しがおかしくなってしまう。
  対策がとられるまではwchar_tをint32_tに書き換えたprintf_infoにキャストして使うことにする。
*/
struct w32_printf_info
{
    int prec;			/* Precision.  */
    int width;			/* Width.  */
    int32_t spec;			/* Format letter.  */
    unsigned int is_long_double : 1;/* L flag.  */
    unsigned int is_short : 1;	/* h flag.  */
    unsigned int is_long : 1;	/* l flag.  */
    unsigned int alt : 1;		/* # flag.  */
    unsigned int space : 1;		/* Space flag.  */
    unsigned int left : 1;		/* - flag.  */
    unsigned int showsign : 1;	/* + flag.  */
    unsigned int group : 1;		/* ' flag.  */
    unsigned int extra : 1;		/* For special use.  */
    unsigned int is_char : 1;	/* hh flag.  */
    unsigned int wide : 1;		/* Nonzero for wide character streams.  */
    unsigned int i18n : 1;		/* I flag.  */
    unsigned int is_binary128 : 1;	/* Floating-point argument is ABI-compatible
                                     with IEC 60559 binary128.  */
    unsigned int __pad : 3;		/* Unused so far.  */
    unsigned short int user;	/* Bits for user-installed modifiers.  */
    int32_t pad;			/* Padding character.  */
};

#define OUTSTREAM FILE
#define OUTSTR(out,str) fprintf(out,"%s",str)
#define OUTINT(out,form,val) fprintf(out,form,val)
#define REGISTER_PRINTF register_printf_specifier

#ifdef __FreeBSD__
#undef OUTSTREAM
#undef OUTSTR
#undef OUTINT
#undef REGISTER_PRINTF
#define OUTSTREAM struct __printf_io
#define OUTSTR(out,str) __printf_puts(out,str,strlen(str))
#define OUTINT(out,form,val) output_int(out,form,val)
#define REGISTER_PRINTF register_printf_render
static int output_int(OUTSTREAM* out, const char* form, unsigned val) {
    char buf[20];
    sprintf(buf, form, val);
    return OUTSTR(out, buf);
}
#endif

//uint16_t*のweucjpをprintfで使うための関数
//フィールド幅指定があればu16とする。
static int print_wej(OUTSTREAM* stream, const struct printf_info* info_orig, const void* const* args)
{
    const uint16_t* w = *((const uint16_t**)args[0]);
    int len;
    if (w == NULL) {
        len = OUTSTR(stream, "<null>");
    }
    else {
        char* ej = ToMb(w);
        len = OUTSTR(stream, ej);
        free(ej);
    }
    return len;
}

static int print_wej_arginfo(const struct printf_info* info, size_t n,
#if defined(__linux__)
    int* argtypes,
#endif
    int* size)
{
    if (n > 0) {
#if defined(__linux__)
        argtypes[0] = PA_INT | PA_FLAG_SHORT | PA_FLAG_PTR;
#endif
        size[0] = sizeof(uint16_t*);
    }
    return 1;
}

//utf8
static int print_u8(OUTSTREAM* stream, const struct printf_info* info_orig, const void* const* args)
{
    const char* str = *((const char**)args[0]);
    int len;
    if (str == NULL) {
        len = OUTSTR(stream, "<null>");
    }
    else {
        char* ej = U8ToEj(NULL, str);
        len = OUTSTR(stream, ej);
        free(ej);
    }
    return len;
}

static int print_u8_arginfo(const struct printf_info* info, size_t n,
#if defined(__linux__)
    int* argtypes,
#endif
    int* size)
{
    if (n > 0) {
#if defined(__linux__)
        argtypes[0] = PA_STRING;
#endif
        size[0] = sizeof(char*);
    }
    return 1;
}

//utf16
static int print_u16(OUTSTREAM* stream, const struct printf_info* info_orig, const void* const* args)
{
    const uint16_t* str = *((const uint16_t**)*args);
    int len;
    if (str == NULL) {
        len = OUTSTR(stream, "<null>");
    }
    else {
        char* ej = U16ToEj(NULL, NULL, str, -1);
        len = OUTSTR(stream, ej);
        free(ej);
    }
    return len;
}

//wejと同じ
#define print_u16_arginfo print_wej_arginfo


//width=個数 prec=ブロック数(1,2,4):デフォルト1
//#=10進数で表示
static int print_dump(OUTSTREAM* stream, const struct printf_info* info_orig, const void* const* args)
{
    const struct w32_printf_info* info = (const struct w32_printf_info*)info_orig;
    int out = 0;
    const char* form = "";
    const unsigned char* ptr = *((const unsigned char**)args[0]);
    int len = info->width;
    int bs = info->prec;
    if (bs != 1 && bs != 2 && bs != 4)
        bs = 1;
    if (info->alt) {
        switch (bs) {
        case 1: form = "%hhu"; break;
        case 2: form = "%hu"; break;
        case 4: form = "%u"; break;
        }
    }
    else {
        switch (bs) {
        case 1: form = "0x%02hhx"; break;
        case 2: form = "0x%04hx"; break;
        case 4: form = "0x%08x"; break;
        }
    }
    while (--len >= 0) {
        unsigned val;
        switch (bs) {
        case 4:
            val = *(uint32_t*)ptr;
            break;
        case 2:
            val = *(int16_t*)ptr;
            break;
        default:
            val = *ptr;
        }
        if (out > 0)
            out += OUTSTR(stream, " ");
        out += OUTINT(stream, form, val);
        ptr += bs;
    }
    return out;
}

static int print_dump_arginfo(const struct printf_info* info, size_t n,
#if defined(__linux__)
    int* argtypes,
#endif
    int* size)
{
    if (n > 0) {
#if defined(__linux__)
        argtypes[0] = PA_POINTER;
#endif
        size[0] = sizeof(void*);
    }
    return 1;
}

void CustomPrintf()
{
    REGISTER_PRINTF('S', print_wej, print_wej_arginfo);
    REGISTER_PRINTF('U', print_u8, print_u8_arginfo);
    REGISTER_PRINTF('W', print_u16, print_u16_arginfo);
    REGISTER_PRINTF('D', print_dump, print_dump_arginfo);
}

//(C) 2018 thomas
