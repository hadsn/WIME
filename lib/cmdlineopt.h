#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COPYRIGHT "(C) 2008-2024 thomas"
#define WIME_SOCKET "WIME_SOCKET"
#define WIME_DEBUG "WIME_DEBUG"

    typedef struct {
        const char* long_name;
        int short_name;
        int has_arg; //struct option‚Ìhas_arg
        bool (*proc)(const char* arg, void* tmp);
        void* tmp; //proc‚É“n‚·ƒf[ƒ^
        const char* explain; //à–¾
        const char* argtype_text; //ˆø”‚Ìà–¾
    } OptArg;

    bool CmdlineOptInt(const char* arg, void* to_int);
    int ParseEnv(int def_ch);
    int CmdlineOpt(int ac, char** av, const OptArg* oa, int oa_num, const char* helpmsg);

#ifdef __cplusplus
}
#endif

//(C) 2018 thomas
