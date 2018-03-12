#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "log.h"
#include "ut.h"

int Verbose;
int DebugChannel = CH_GLOBAL;
char LogMark; //メッセージ表示にも使う。

#define CHDEF(s) {#s,CH_##s}
struct{
    const char* label;
    int val;
} ChDef[]={
    CHDEF(GLOBAL),
    CHDEF(COMPOSITION),
    CHDEF(NOTIFY),
    CHDEF(REQUEST),
    CHDEF(IMEMSG),
    CHDEF(CANNA),
    CHDEF(XIM),
    CHDEF(GTK),
    CHDEF(QT),
    CHDEF(IBUS),
    CHDEF(WINMSG),
    CHDEF(TIME),
    {"ALL",(1<<(CH_MAXBIT+1))-1}
};

static int parse_channel_str(const char* str0)
{
    int chval = 0;
    char* str_save = strdup(str0);
    for(char* s=str_save; *s!=0; ++s)
	*s = toupper(*s);

    char* str = str_save;
    char* ch;
    while((ch = strsep(&str,",")) != NULL){
	if(ch[0]!= 0){
	    bool dis=false;
	    int bitmask=0;
	    if(ch[0]=='-'){ //このビットは消す。
		dis=true;
		++ch;
	    }
	    if(isdigit(ch[0])){
		bitmask = (int)strtol(ch,NULL,0);
	    }else{
		int n;
		for(n=0; n<ITEMS(ChDef); ++n){
		    if(strcmp(ch,ChDef[n].label)==0){
			bitmask = ChDef[n].val;
			break;
		    }
		}
		if(n==ITEMS(ChDef)){
		    ERR("unknown channel:%s\n",ch);
		}
	    }
	    if(dis)
		chval &= ~bitmask;
	    else
		chval |= bitmask;
	}
    }
    free(str_save);
    return chval;
}

#define DEBUGENVSTR "WIME_DEBUG"

void ParseChannelEnv(int def_ch)
{
    DebugChannel = def_ch;
    char* str = getenv(DEBUGENVSTR);
    if(str==NULL || strlen(str)==0)
	return;
    
    char* str_save = str = strdup(str);
    Verbose = isdigit(str[0]) ? atoi(strsep(&str,",")) : 1;
    if(str != NULL)
	DebugChannel |= parse_channel_str(str);
    free(str_save);
}

void ParseChannelStr(const char* str)
{
    DebugChannel = parse_channel_str(str);
}
