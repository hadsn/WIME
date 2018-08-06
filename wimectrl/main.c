// -*- coding:euc-jp -*-
#define _GNU_SOURCE
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "so/wimeapi.h"
#include "so/winkey.h"
#include "so/xres.h"
#include "lib/array.h"
#include "lib/wimeconn.h"
#include "lib/ut.h"
#include "lib/log.h"
#include "lib/version.h"
#include "lib/list.h"
#include "lib/cmdlineopt.h"
#include "lib/printf.h"

static bool ini_wime(void);
static bool kill_wime(void);
static bool kill_xim(void);
static bool reconvert_window(const char* src_u8);


//オプション処理関数が返すフラグ
const int PROC_CMD=1;
const int END_FAIL=2;

//-p
static int SocketNum;
static bool get_socket(const char* arg,void* flag)
{
    bool st = CmdlineOptInt(arg,&SocketNum);
    return st && SocketNum>=0 && SocketNum<=0xffff;
}

static bool Initialized;
static bool ini_wime(void)
{
    if(!Initialized){
	Initialized = (WimeInitialize(SocketNum,'c')>=0);
    }
    return Initialized;
}

static char* get_str_from_stdin(void)
{
    char* buf=NULL;
    size_t bufsz=0,len;
    len = getline(&buf,&bufsz,stdin);
    if(len == -1){
        free(buf);
        buf=NULL;
    }else{
        if(buf[len-1]=='\n')
            buf[len-1]=0; //改行文字を消す
    }
    return buf;
}

//-c
static bool reconv_str(const char* arg,void* flag)
{
    char* str = strcmp(arg,"-")==0 ? get_str_from_stdin() : strdup(arg);
    if(str!=NULL && *str!=0 && ini_wime()){
	char* u8 = (*CurToU8)(NULL,str,-1);
	reconvert_window(u8);
	free(u8);
    }else
	*(int*)flag |= END_FAIL;
    *(int*)flag |= PROC_CMD;
    free(str);
    return true;
}

//-d
static bool user_dic(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    if(!(ini_wime() && WimeOpenIMEDialog(WIME_DIALOG_SELECTDIC)))
	*(int*)flag |= END_FAIL;
    return true;
}

//-e
static bool reset_wime(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    if(!(ini_wime() && WimeReset()))
	*(int*)flag |= END_FAIL;
    return true;
}

//-k[x]
static bool kill_app(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    bool st = true;
    if(arg == NULL){
	if(kill_wime()){
	    Initialized = false; //WimeFinalize()を呼ばないようにする。
	}
    }else{
	if(strcmp(arg,"x") == 0){
	    kill_xim();
	}else
	    st = false;
    }
    return st;
}

//-l
static bool set_dbg(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    if(!(ini_wime() && WimeSetDebugChannel(Verbose,DebugChannel)))
	*(int*)flag |= END_FAIL;
    return true;
}

//-r
static bool reg_word(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    if(!(ini_wime() && WimeOpenIMEDialog(WIME_DIALOG_REGISTERWORD)))
	*(int*)flag |= END_FAIL;
    return true;
}

//-s
static bool ime_setting(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    if(!(ini_wime() && WimeOpenIMEDialog(WIME_DIALOG_PROPERTY)))
	*(int*)flag |= END_FAIL;
    return true;
}

//セマフォをwait-->postできた後の処理。
static bool after_wait(sem_t* sem)
{
    bool st = ini_wime(); //もう一度接続してみる。
    if(!st){
	//ソケットを残したままwimeが死亡したと思われる。
	sem_wait(sem); //自分がpostした分
	sem_wait(sem); //wimeがpostするのを再度待つ
	sem_post(sem);
	st = ini_wime();
    }
    return st;
}

//-t[msec]
//利用可能になるまで待つ。 -1:無期限  ≧0:ミリ秒待機
static bool wait_usable(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    bool st=true;
    if(!ini_wime()){
	st = SemWait(after_wait,SocketNum,arg==NULL?-1:(int)strtol(arg,NULL,0));
    }
    if(!st)
	*(int*)flag |= END_FAIL;
    return true;
}

//-w
static bool wordstyle_list(const char* arg,void* flag)
{
    bool st = false;
    *(int*)flag |= PROC_CMD;
    if(ini_wime()){
	int count;
	int* code;
    
	Array* desclist =  WimeGetStyleList(&count,&code);
	for(int index=0; index<count; ++index){
	    char* ej = U8ToEj(NULL,ListInc(desclist,index));
	    printf("0x%x %s\t\n",code[index],ej);
	    free(ej);
	}
	free(code);
	free(ArDelete(desclist));
	st = true;
    }
    if(!st)
	*(int*)flag |= END_FAIL;
    return true;
}

//--result-str <wime-context>[,[str]]
static bool set_str(const char* arg,void* flag)
{
    *(int*)flag |= PROC_CMD;
    bool st = false;
    char* optstr;
    int cxn = strtol(arg,&optstr,0);
    if(cxn>0 && ini_wime()){
	char e = *(optstr++);
	if(e==0 || e!=','){
	    //番号のみ、あるいは番号の続きが','のみで文字列がないときは標準入力から読み込む。
	    if((optstr = get_str_from_stdin()) == NULL)
		return false; //エラーならすぐ終わる。
	}
	st = WimeSetResultStr(-cxn,EjToU8(NULL,optstr,-1));
    }
    if(!st)
	*(int*)flag |= END_FAIL;
    return true;
}

//--flags [wime-context[,flag-val]]
static bool set_flag(const char* arg,void* flag)
{
    bool st=false;
    *(int*)flag |= PROC_CMD;
    if(ini_wime()){
	int dumpnum,flagval=0;
	bool do_set=false;
	int cxn = 0;
	if(arg!=NULL && *arg != 0){
	    char* end;
	    cxn = strtol(arg,&end,0);
	    if(*end != 0){ //区切り文字（カンマ）があればフラグ数値を読み込む
		flagval = strtol(++end,NULL,0);
		do_set = true;
	    }
	}
	uint32_t* cxdump = WimeDumpContext(do_set,cxn,flagval,&dumpnum);
	if(dumpnum < 0){
	    printf("error occurred.\n");
	}else{
	    const char* flagname[]={
		"OPEN_STATUS_WINDOW",
		"PROC_NOTIFY_MSG",
		"PROC_COMP_MSG",
		"PENDING_RECONV",
		"SEND_KEY",
		"TRAP_OPEN_CAND",
		"CATCH_OPEN_CAND",
		"CATCH_CHG_CAND",
	    };
	    for(uint32_t* f=cxdump; dumpnum>0; --dumpnum){
		printf("%u\t",*(f++));
		for(int n=0; n<ITEMS(flagname); ++n)
		    if((*f & (1<<n)))
			printf("%s ",flagname[n]);
		printf("\n");
		f++;
	    }
	    free(cxdump);
	}
	st=true;
    }
    if(!st)
	*(int*)flag |= END_FAIL;
    return true;
}

static bool kill_wime(void)
{
    bool st=false;
    if(ini_wime())
	st = CannaKillServer();
    return st;
}

/*
  wimeximのwindow-idを返す
*/
static Window xim_window(Display** disp)
{
    Window w=None;

    if((*disp = XOpenDisplay(NULL)) != NULL){
	const char* sel_str = "@server=wime";
	if(SocketNum > 0){
	    char buf[strlen(sel_str)+10];
	    sprintf(buf,"%s%d",sel_str,SocketNum);
	    sel_str = strdup(buf);
	}
	Atom sel;
	if((sel = XInternAtom(*disp,sel_str,True)) != None){
	    w = XGetSelectionOwner(*disp,sel);
	}
    }
    return w;
}

static bool kill_xim(void)
{
    Display* disp;
    Window x;

    /* wimeximのトップウィンドウを破棄している。
       !!! pidを調べて終了シグナルを送る方がまともか？
    */
    if((x = xim_window(&disp)) != None)
	XDestroyWindow(disp,x);
    XCloseDisplay(disp);
    return x!=None;
}

#if 0
static bool check_xim(void)
{
    Display* disp;
    Window x = xim_window(&disp);
    XCloseDisplay(disp);
    return x!=None;
}
#endif

#if 0
#include <sys/mman.h>
#include <errno.h>
static bool dump_pidfile(const char* arg,void* flag)
{
    static const char shmname[]="/wimepid";

    int shm = shm_open(shmname,O_RDWR|O_CREAT,LOCKFILEMODE);
    if(shm == -1){
	fprintf(stderr,"cannot open pid file.\n");
	return false;
    }
    struct stat sb;
    fstat(shm,&sb);
    if(sb.st_size == 0){
	fprintf(stderr,"pid file size is zero.\n");
	close(shm);
	return false;
    }
    PidTableElt* table = mmap(NULL,sb.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,shm,0);
    if(table == MAP_FAILED){
	fprintf(stderr,"fail mmap.\n");
	close(shm);
	return false;
    }

    PidTableElt* tp = table;
    printf("pid\tskt\tu16\n");
    Array cmdline;
    ArNew(&cmdline,1,NULL);
    for(int tbsize = sb.st_size/sizeof(PidTableElt); tbsize>0; --tbsize,++tp){
	if(tp->Pid!=0){
	    char proc[80];
	    sprintf(proc,"/proc/%d/cmdline",tp->Pid);
	    FILE* fp = fopen(proc,"r");
	    if(fp != NULL){
		for(int ch; (ch=fgetc(fp))!=EOF;){
		    if(ch==0)
			ch=' ';
		    ArAddChar(&cmdline,ch);
		}
		ArAddChar(&cmdline,0);
		fclose(fp);
	    }else{
		ArAddStr(&cmdline,strerror(errno));
	    }
	    printf("%d\t%d\t%d\t%s\n",tp->Pid,tp->SocketNum,tp->UseUtf16,ArAdr(&cmdline));
	    ArClear(&cmdline);
	}
    }

    if(munmap(table,sb.st_size)!=0)
	fprintf(stderr,"fail munmap (%d) %m\n",errno);
    
    close(shm);
    *(int*)flag |= PROC_CMD;
    return true;
}
#endif

/*
  再変換を表示するウィンドウ
*/
#define RCWIN_WIDTH 300
#define RCWIN_HEIGHT 50

static bool reconvert_window(const char* src_u8)
{
    int st,pos;

    Display* disp = XOpenDisplay(NULL);
    InitDatabase(disp,"xim");
    int cxn = CannaCreateContext();
    char* comp_font = GetCompFont(disp);
    int height = comp_font!=NULL ? WimeSetCompFont(cxn,comp_font,0) : RCWIN_HEIGHT;
    Window win = XCreateSimpleWindow(disp,XDefaultRootWindow(disp),0,0,RCWIN_WIDTH,height,0,XBlackPixel(disp,0),XWhitePixel(disp,0));
    XSelectInput(disp,win,KeyPressMask|StructureNotifyMask);
    XMapWindow(disp,win);
    XFlush(disp);

    //wmから終了メッセージを受け取る
    Atom delwin = XInternAtom(disp,"WM_DELETE_WINDOW",True);
    XSetWMProtocols(disp,win,&delwin,1);

    WimeShowCandidateWindow(cxn,true);
    WimeShowToolbar(cxn,true,true);
    WimeEnableIme(cxn,IME_ON);
    WimeReconvert(cxn,src_u8,0,&pos);

    while(1){
	char* res;
	XEvent ev;
	XButtonEvent* evk = (typeof(evk))&ev;
	XConfigureEvent* evc = (typeof(evc))&ev;
	XNextEvent(disp,&ev);
	switch(ev.type){
	case KeyPress:
	    res=NULL;
	    st = WimeSendKey(cxn,ConvToVk(XKEYCODETOKEYSYM(disp,evk->button,0),evk->state),&res);
	    free(res);
	    if(res!=NULL)
		goto fin;
	    break;
	case ConfigureNotify:
	    WimeMoveShadowWin(cxn,evc->x,evc->y-evc->height,evc->width,evc->height);
	    WimeSetCompWin(cxn,WIME_POS_POINT,0,evc->height);
	    break;
	case ClientMessage:
	    if(ev.xclient.data.l[0] == delwin){
		st = true;
		goto fin;
	    }
	}
    }

fin:
    WimeFlushMsg();
    CannaEndConvert(cxn,0,0,NULL);
    WimeEnableIme(cxn,IME_OFF);
    CannaCloseContext(cxn);
    XDestroyWindow(disp,win);
    XCloseDisplay(disp);
    return st;
}

int main(int ac,char *av[])
{
    CustomPrintf();
    int flag=0;
    OptArg oa[]={
	{NULL,'p',required_argument,get_socket,&flag,	NULL,NULL}, //cmdlineopt.cの処理を上書き
	{NULL,'c',required_argument,reconv_str,&flag,	"reconvert string"," <str>"},
	{NULL,'d',no_argument,user_dic,&flag,		"\tuser dictionary dialog",NULL},
	{NULL,'e',no_argument,reset_wime,&flag,		"\treset wime",NULL},
	{NULL,'k',optional_argument,kill_app,&flag,	"\tkill wime(xim)","[x]"},
	{NULL,'l',no_argument,set_dbg,&flag,		"\tset new verbose level and channel",NULL},
	{NULL,'r',no_argument,reg_word,&flag,		"\tregister word",NULL},
	{NULL,'s',no_argument,ime_setting,&flag,	"\tIME setting(default)",NULL},
	{NULL,'t',optional_argument,wait_usable,&flag,	"wait till wime is usable","[msec]"},
	{NULL,'w',no_argument,wordstyle_list,&flag,	"\twordstyle list",NULL},
	   //"  -G	follow options apply to wime-gtk\n"
	   //"  -I\n"
	   //"  -Q	follow options apply to wime-qt\n"
	   //"  -S\n"
	   //"  -U\n"
	{"result-str",	'xr',required_argument,set_str,&flag,	"set result string","<wime-context>[,[str]]"},
	{"flags",	'xf',optional_argument,set_flag,&flag,	"set/get flags", "[wime-context[,flag-val]]"},
#if 0
	{"pidfile",	'xp',no_argument,dump_pidfile,&flag,	"dump pid file",NULL},
#endif
    };

    CmdlineOpt(ac,av,oa,ITEMS(oa),NULL);
    if(!(flag & PROC_CMD)){ //動作オプションなし（デフォルト）の動作
	if(ini_wime())
	    WimeOpenIMEDialog(WIME_DIALOG_PROPERTY);
	else
	    flag |= END_FAIL;
    }
    if(Initialized)
	WimeFinalize();
    return (flag & END_FAIL) ? 1 : 0;
}

//(C) 2008 thomas
