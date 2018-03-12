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
#include "exe/version.h"

bool word_list(void);
bool wait_usable(const char*);
bool kill_wime(void);
bool kill_xim(void);
bool ini_wime(void);
bool fail_ret(void);
bool check_xim(void);
bool reconvert_window(const char* src);
char* get_str_from_stdin(void);
bool wime_debug_cmd(const char* arg);

int SocketNum;
bool Initialized;

void usage()
{
    printf("wimectrl [option]\n"
	   "  -c <str>	reconvert string(euc-jp)\n"
	   "  -d\t	user dictionary\n"
	   "  -e\t	reset wime\n"
	   "  -k\t	kill wime\n"
	   "  -l\t	set new verbose level and channel\n"
	   "  -p <num>	socket path number\n"
	   "  -r\t	register word\n"
	   "  -s\t	IME setting(default)\n"
	   "  -t\t	wait till wime is usable\n"
	   "  -u\t	exit with 0 if wime is enable\n"
	   "  -v [num]	verbose level\n"
	   "  -w\t	wordstyle list\n"
	   //"  -G	follow options apply to wime-gtk\n"
	   //"  -I\n"
	   //"  -Q	follow options apply to wime-qt\n"
	   //"  -S\n"
	   //"  -U\n"
	   "  -W\t	follow options apply to wime(default)\n"
	   "  -X\t	follow options apply to wimexim\n"
	   "  --channel <str>	new debug channel\n"
	   "  -h,--help	this message\n"
	   "  --version	print version\n"
	   "  -x s<wime-context>[,[str]]	set result string\n"
	   "  -x c[wime-context[,flag-val]]	set/get flags\n"
	);
}

int main(int ac,char *av[])
{
    bool st,cmd=false;
    int c;
    char* str;
    struct option longopt[]={
	{"channel",	required_argument,NULL,'ch'},
	{"help",	no_argument,NULL,'h'},
	{"version",	no_argument,NULL,'vsn'},
	{NULL,0,NULL,0}
    };
    struct funcs {
	bool (*kill_server)(void);
	bool (*check_enabled)(void);
    };
    struct funcs func_w={kill_wime,ini_wime};
    struct funcs func_x={kill_xim,check_xim};
    struct funcs func_g={fail_ret,fail_ret};
    struct funcs func_q={fail_ret,fail_ret};
    struct funcs *func=&func_w;
	      
    while((c = getopt_long(ac,av,"c:dehklp:rstuv::wx:GQWX",longopt,NULL)) != -1){
	switch(c){
	case 'e':
	    cmd = true;
	    st = ini_wime() && WimeReset();
	    break;
	case 'w':
	    cmd = true;
	    st = ini_wime() && word_list();
	    break;
	case 's':
	    cmd = true;
	    st = ini_wime() && WimeOpenIMEDialog(WIME_DIALOG_PROPERTY);
	    break;
	case 'r':
	    cmd = true;
	    st = ini_wime() && WimeOpenIMEDialog(WIME_DIALOG_REGISTERWORD);
	    break;
	case 'd':
	    cmd = true;
	    st = ini_wime() && WimeOpenIMEDialog(WIME_DIALOG_SELECTDIC);
	    break;
	case 'k':
	    cmd = true;
	    st = (*func->kill_server)();
	    if(st)
		Initialized=false; //WimeFinalize()を呼ばないようにする。
	    break;
	case 'p':
	    SocketNum = atoi(optarg);
	    if(SocketNum<0 || SocketNum>0xffff){
		printf("option p:invalid number %d\n",SocketNum);
		return 1;
	    }
	    break;
	case 'vsn':
	    cmd = true;
	    printf("%s\n",WIME_VER_STR);
	    break;
	case 'h':
	    cmd = true;
	    usage();
	    break;
	case 'u':
	    cmd = true;
	    st = (*func->check_enabled)();
	    break; //WimeInitializeが成功すればよしとする
	case 't':
	    cmd = true;
	    st = wait_usable(av[0]);
	    break;
	case 'x':
	    cmd = true;
	    st = wime_debug_cmd(optarg);
	    break;
	case 'W':
	    func = &func_w;
	    break;
	case 'X':
	    func = &func_x;
	    break;
	case 'G':
	    func = &func_g;
	    break;
	case 'Q':
	    func = &func_q;
	    break;
	case 'c':
	    str = strcmp(optarg,"-")==0 ? get_str_from_stdin() : strdup(optarg);
	    if(str!=NULL && *str!=0)
		st = ini_wime() && reconvert_window(str);
	    cmd = true;
	    free(str);
	    break;
	case 'v':
	    if(optarg==NULL)
		++Verbose;
	    else if(strcmp(optarg,"-")==0)
		Verbose = 0;
	    else if(isdigit(optarg[0]))
		Verbose = optarg[0]-'0';
	    else
		usage();
	    break;
	case 'ch':
	    ParseChannelStr(optarg);
	    break;
	case 'l':
	    cmd = true;
	    st = ini_wime() && WimeSetDebugChannel(Verbose,DebugChannel);
	    break;
	default:
	    usage();
	    st = false;
	    goto end;
	}
    }
    if(!cmd){ //動作オプションなし（デフォルト）の動作
	if(ini_wime())
	    st = WimeOpenIMEDialog(WIME_DIALOG_PROPERTY);
    }
end:
    if(Initialized)
	WimeFinalize();
    return st ? 0 : 1;
}

bool fail_ret(void)
{
    return false;
}

bool ini_wime(void)
{
    if(!Initialized){
	Initialized = (WimeInitialize(SocketNum,'c')>=0);
    }
    return Initialized;
}

bool word_list(void)
{
    WimeWordStyle *ws,*ws_o;
    int count;

    ws = ws_o = WimeGetStyleList(&count);
    while(--count >= 0){
	printf("0x%x %s\t\n",ws->Code,ws->Desc);
	ws = (WimeWordStyle*)((char*)ws+ws->Size);
    }
    free(ws_o);
    return true;
}

char* get_str_from_stdin(void)
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

bool set_str(int cxn,char* str)
{
    bool st=false;

    if(str==NULL || *str==0)
	str = get_str_from_stdin();
    if(str != NULL){
	st = WimeSetResultStr(-cxn,str);
	free(str);
    }
    return st;
}

//???ini_wime()は先に呼び出しておいた方がいいか？
bool wime_debug_cmd(const char* arg)
{
    bool st=false;
    int cxn;
    char* optstr;

    switch(*(arg++)){
    case 's': //番号のみ、あるいは番号の続きが','のみで文字列がないときは標準入力から読み込む。
	cxn = strtol(arg,&optstr,0);
	if(cxn>0 && ini_wime()){
	    char e = *(optstr++);
	    if(e==0 || e!=',')
		optstr = NULL;
	    st = set_str(cxn,optstr);
	}
	break;
    case 'c':
	printf("initialized...");fflush(stdout);
	if(ini_wime()){
	    printf("done\n");
	    int dumpnum,flagval=0;
	    bool do_set=false;
	    cxn = 0;
	    if(*arg != 0){
		cxn = strtol(arg,&optstr,0);
		if(*optstr != 0){ //区切り文字（カンマ）があればフラグ数値を読み込む
		    flagval = strtol(++optstr,NULL,0);
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
		    "IN_FOCUS"
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
	break;
    }
    return st;
}

bool kill_wime(void)
{
    bool st=false;
    if(ini_wime())
	st = CannaKillServer();
    return st;
}

/*
  wimeximのwindow-idを返す
*/
Window xim_window(Display** disp)
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

bool kill_xim(void)
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

bool check_xim(void)
{
    Display* disp;
    Window x = xim_window(&disp);
    XCloseDisplay(disp);
    return x!=None;
}

/*
  再変換を表示するウィンドウ
*/
#define RCWIN_WIDTH 300
#define RCWIN_HEIGHT 50

bool reconvert_window(const char* src)
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
    WimeReconvert(cxn,EjToU16(NULL,src),0,&pos);

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

//セマフォをwait-->postできた後の処理。
bool after_wait(sem_t* sem)
{
    bool st=ini_wime();
    if(!st){
	//ソケットを残したままwimeが死亡したと思われる。
	sem_wait(sem); //自分がpostした分
	sem_wait(sem); //wimeがpostするのを再度待つ
	sem_post(sem);
	st=ini_wime();
    }
    return st;
}

//利用可能になるまで待つ
bool wait_usable(const char* prog_name)
{
    bool st=true;

    if(!ini_wime()){
	st = SemWait(after_wait);
	SemUnlink();
    }
    return st;
}

//(C) 2008 thomas
