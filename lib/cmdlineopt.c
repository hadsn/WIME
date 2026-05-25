#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include "array.h"
#include "ut.h"
#include "version.h"
#include "log.h"
#include "cmdlineopt.h"

//OptArgをgetopt()用のオプションデータにする
static void optarg_to_getopt(Array* oa, Array* shortname, Array* longname)
{
    for (int id = 0; id < ArUsing(oa); ++id) {
        OptArg* oap = ArElem(oa, id);
        if (oap->long_name != NULL) {
            ArAdd1(longname, &(struct option){oap->long_name, oap->has_arg, NULL, oap->short_name});
        }
        if (oap->short_name < 256) {
            ArAddChar(shortname, oap->short_name);
            switch (oap->has_arg) {
            case required_argument:
                ArAddChar(shortname, ':');
                break;
            case optional_argument:
                ArDec(ArAddStr(shortname, "::"));
            }
        }
    }
    ArAddChar(shortname, 0);
    ArAdd1(longname, &(struct option){NULL, 0, NULL, 0});
}

//OptArgの配列から短いオプション名が*valである要素を探す。
static int match_shortname(const void* elem, const void* val)
{
    return ((OptArg*)elem)->short_name == *(int*)val;
}

//整数オプションを*to_intに返す
bool CmdlineOptInt(const char* arg, void* to_int)
{
    errno = 0;
    char* endp;
    *(int*)to_int = (int)strtol(arg, &endp, 0);
    return (*endp == 0 && errno == 0);
}

static bool verbose_level(const char* arg, void* to_vl)
{
    int* vl = to_vl;
    if (arg == NULL) {
        ++* vl;
        return true;
    }
    if (strcmp(arg, "-") == 0) {
        *vl = 0;
        return true;
    }
    return CmdlineOptInt(arg, vl) && *vl >= 0 && *vl < LOG_MAX;
}

static bool get_socket_num(const char* arg, void* to_num)
{
    int* num = to_num;
    return CmdlineOptInt(arg, num) && *num >= 0 && *num <= 0xffff;
}


static bool print_version(const char* arg, void* tmp)
{
    printf("%s\n%s\n", WIME_VER_STR, COPYRIGHT);
    exit(0);
    return true;
}

//print_usageに渡すデータ。いくつかのデータをひとまとめにする。
typedef struct {
    const char* av0; //main()のav[0]
    const char* additional_msg; //オプション以外の引数の説明
    Array* all_optarg; //OptArg全部
} usage_data;

//-h,--help
static bool print_usage(const char* arg, void* tmp)
{
    usage_data* ud = tmp;
    if (ud->all_optarg == NULL)
        return true; //wimectrlで最初にpオプションだけを取得するとき。1回目は何もしない。

    printf("%s [options]\n", ud->av0);
    if (ud->additional_msg)
        printf("  %s\n", ud->additional_msg);
    for (int id = 0; id < ArUsing(ud->all_optarg); ++id) {
        OptArg* oap = ArElem(ud->all_optarg, id);
        printf("  ");
        if (oap->short_name < 256) {
            printf("-%c", oap->short_name);
        }
        if (oap->long_name != NULL) {
            if (oap->short_name < 256)
                printf(",");
            printf("--%s ", oap->long_name);
        }
        if (oap->argtype_text != NULL) {
            printf("%s", oap->argtype_text);
        }
        printf("\t%s\n", oap->explain);
    }
    exit(0);
    return true;
}

#define CHDEF(s) {#s,CH_##s}
struct {
    const char* label;
    int val;
} ChDef[] = {
    CHDEF(GLOBAL),
    CHDEF(COMPOSITION),
    CHDEF(NOTIFY),
    CHDEF(REQUEST),
    CHDEF(IMEMSG),
    CHDEF(CANNA),
    CHDEF(XIM),
    CHDEF(GTK),
    CHDEF(QT),
    CHDEF(WINMSG),
    CHDEF(TIME),
    CHDEF(COMPO_IMC),
    CHDEF(NOTI_IMC),
    CHDEF(REQ_IMC),
    {"ALL",(1 << (CH_MAXBIT + 1)) - 1}
};

//エラーの時-1
static int parse_channel_str(const char* str0, int chval)
{
    char* str_save = strdup(str0);
    for (char* s = str_save; *s != 0; ++s)
        *s = toupper(*s);

    char* str = str_save;
    char* ch;
    while ((ch = strsep(&str, ",")) != NULL) {
        if (ch[0] != 0) {
            bool dis = false;
            int bitmask = 0;
            if (ch[0] == '-') { //このビットは消す。
                dis = true;
                ++ch;
            }
            if (isdigit(ch[0])) {
                bitmask = (int)strtol(ch, NULL, 0);
            }
            else {
                int n;
                for (n = 0; n < ITEMS(ChDef); ++n) {
                    if (strcmp(ch, ChDef[n].label) == 0) {
                        bitmask = ChDef[n].val;
                        break;
                    }
                }
                if (n == ITEMS(ChDef)) {
                    fprintf(stderr, "unknown channel:%s\n", ch);
                    chval = -1;
                    break;
                }
            }
            if (dis)
                chval &= ~bitmask;
            else
                chval |= bitmask;
        }
    }
    free(str_save);
    return chval;
}

//環境変数からの設定。immoduleはこれを呼び出すこと。
//ソケット番号を返す。
int ParseEnv(int def_ch)
{
    //デバッグチャンネル
    char* str = getenv(WIME_DEBUG);
    if (str != NULL && strlen(str) != 0) {
        char* str_save = str = strdup(str);
        Verbose = isdigit(str[0]) ? atoi(strsep(&str, ",")) : 1;
        if (str != NULL) {
            int ch = parse_channel_str(str, def_ch | DebugChannel);
            if (ch != -1)
                DebugChannel = ch;
        }
        free(str_save);
    }

    //ソケット
    str = getenv(WIME_SOCKET);
    int socket_num;
    if (str == NULL || *str == 0 || !get_socket_num(str, &socket_num))
        socket_num = 0;
    return socket_num;
}

//--ch
bool set_ch(const char* arg, void* to_ch)
{
    int ch = parse_channel_str(arg, *(int*)to_ch);
    return ch == -1 ? false : (*(int*)to_ch = (CH_GLOBAL | ch), true);
}

/*
  コマンドラインオプションの処理
  間違ったオプションのエラー表示はgetopt()にまかせている。
  helpmsg=オプション以外の引数の説明
  戻り値=ソケット番号。エラーの時-1
  短いオプション名が同じだった場合procとtmpを上書きする。
*/
int CmdlineOpt(int ac, char** av, const OptArg* oa, int oa_num, const char* helpmsg)
{
    int socket_num = ParseEnv(CH_GLOBAL);

    Array all_oa, shortopt, longopt;
    OptArg def_oa[] = {
        {NULL,'p',required_argument,get_socket_num,&socket_num,"socket number(1..65535)"," <num>"},
        {NULL,'v',optional_argument,verbose_level,&Verbose,"verbose level","[num]"},
        {"channel",'ch',required_argument,set_ch,&DebugChannel,"debug channdel","<str>"},
        {"help",'h',no_argument,print_usage,&(usage_data){av[0],helpmsg,oa != NULL ? &all_oa : NULL},"this message",NULL},
        {"version",'vsn',no_argument,print_version,NULL,"print version",NULL},
    };

    ArNew(&all_oa, sizeof(OptArg), NULL);
    ArAddN(&all_oa, def_oa, ITEMS(def_oa));
    for (int num = 0; num < oa_num; ++num) {
        //短いオプション名が同じだった場合procとtmpを上書きする。
        OptArg* el = ArElem(&all_oa, ArFindIf(&all_oa, 0, match_shortname, &(int){oa[num].short_name}));
        if (el != NULL) {
            el->proc = oa[num].proc;
            el->tmp = oa[num].tmp;
        }
        else
            ArAdd1(&all_oa, &oa[num]);
    }

    {//wimectrl用にpだけを先に処理する。
        OptArg* el = ArElem(&all_oa, ArFindIf(&all_oa, 0, match_shortname, &(int){'p'}));
        if (el != NULL) {
            char buf[20];
            snprintf(buf, sizeof(buf), "%d", socket_num);
            (el->proc)(buf, el->tmp);
        }
    }

    optarg_to_getopt(&all_oa, ArNew(&shortopt, 1, NULL), ArNew(&longopt, sizeof(struct option), NULL));
    int c;
    while ((c = getopt_long(ac, av, ArAdr(&shortopt), ArAdr(&longopt), NULL)) != -1) {
        if (c == '?' || c == ':') {
            socket_num = -1;
            break;
        }
        OptArg* el = ArElem(&all_oa, ArFindIf(&all_oa, 0, match_shortname, &(int){c}));
        if (el != NULL && !(el->proc)(optarg, el->tmp)) {
            fprintf(stderr, "error in option ");
            if (el->short_name <= 0xff)
                fprintf(stderr, "-%c\n", el->short_name);
            else
                fprintf(stderr, "--%s\n", el->long_name);
        }
    }

    ArDelete(&all_oa);
    ArDelete(&shortopt);
    ArDelete(&longopt);
    return socket_num;
}

//(C) 2018 thomas
