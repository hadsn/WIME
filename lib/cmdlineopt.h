#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COPYRIGHT "(C) 2008-2021 thomas"
#define WIME_SOCKET "WIME_SOCKET"
#define WIME_DEBUG "WIME_DEBUG"

typedef struct{
    const char* long_name;
    int short_name;
    int has_arg; //struct optionのhas_arg
    bool (*proc)(const char* arg,void* tmp);
    void* tmp; //procに渡すデータ
    const char* explain; //説明
    const char* argtype_text; //引数の説明
} OptArg;

    bool CmdlineOptInt(const char* arg,void* to_int);
    int ParseEnv(int def_ch);
    int CmdlineOpt(int ac,char** av,const OptArg* oa,int oa_num,const char* helpmsg);

#ifdef __cplusplus
}
#endif

//(C) 2018 thomas
