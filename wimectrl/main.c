#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "so/wimeapi.h"
#include "so/wimelog.h"
#include "lib/array.h"
#include "exe/version.h"
#include "lib/wimeconn.h"

bool word_list(void);
bool set_str(int cxn);
bool wait_usable(void);
bool kill_wime(void);
bool kill_xim(void);
bool ini_wime(void);
bool fail_ret(void);
bool check_xim(void);
bool reconvert_window(const char* src);
char* get_str_from_stdin(void);

int SocketNum;
bool Initialized;

void usage()
{
    printf("wimectrl [option]\n"
	   "  -c <str>	reconvert string(euc-jp)\n"
	   "  -d	user dictionary\n"
	   "  -e	reset wime\n"
	   "  -k	kill wime\n"
	   "  -p <num>	socket path number\n"
	   "  -r	register word\n"
	   "  -s	IME setting(default)\n"
	   "  -t	wait till wime is usable\n"
	   "  -u	exit with 0 if wime is enable\n"
	   "  -w	wordstyle list\n"
	   //"  -x <wime context>\n"
	   //"		send text to context\n"
	   //"  -G	follow options use wime-gtk\n"
	   //"  -I\n"
	   //"  -Q	follow options use wime-qt\n"
	   //"  -S\n"
	   //"  -U\n"
	   "  -W	follow options use wime(default)\n"
	   "  -X	follow options use wimexim\n"
	   "  -h,--help	this message\n"
	   "  --version	print version\n"
	);
}

int main(int ac,char *av[])
{
    bool st,cmd=false;
    int c;
    char* str;
    struct option longopt[]={
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
	      
    while((c = getopt_long(ac,av,"c:ewksrdhp:utx:WXGQ",longopt,NULL)) != -1){
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
	    printf("%s\n",WIME_VERSION);
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
	    st = wait_usable();
	    break;
	case 'x':
	    cmd = true;
	    st = set_str(strtol(optarg,NULL,0));
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
	Initialized = WimeInitialize(SocketNum,LOGMARK);
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

bool set_str(int cxn)
{
    bool st=false;
    char* str;
    if(cxn!=0 && (str=get_str_from_stdin())!=NULL){
	st = WimeSetResultStr(-cxn,str);
	free(str);
    }
    return st;
}

bool kill_wime(void)
{
    bool st=false;
    if(ini_wime())
	st = WimeKillServer();
    return st;
}

/*
  wimeximのwindow-idを返す
*/
Window xim_window(Display** disp)
{
    const char sel_str[]="@server=wime";
    Window w=None;
    Atom sel;

    if((*disp = XOpenDisplay(NULL)) != NULL){
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
    Window x;
    Display* disp;
    
    x = xim_window(&disp);
    XCloseDisplay(disp);
    return x!=None;
}

/*
  再変換を表示するウィンドウ
*/
#include "so/winkey.h"
#include "so/xres.h"
#include "lib/ut.h"

#define RCWIN_WIDTH 300
#define RCWIN_HEIGHT 50

bool reconvert_window(const char* src)
{
    Window win;
    XEvent ev;
    XButtonEvent* evk = (typeof(evk))&ev;
    XConfigureEvent* evc = (typeof(evc))&ev;
    int st,height,pos,cxn;
    char* res,*comp_font;
    Display* disp;

    disp = XOpenDisplay(NULL);
    InitDatabase(disp,"xim");
    cxn = WimeCreateContext();
    comp_font = GetCompFont(disp);
    height = comp_font!=NULL ? WimeSetCompFont(cxn,comp_font,0) : RCWIN_HEIGHT;
    win = XCreateSimpleWindow(disp,XDefaultRootWindow(disp),0,0,RCWIN_WIDTH,height,0,XBlackPixel(disp,0),XWhitePixel(disp,0));
    XSelectInput(disp,win,KeyPressMask|StructureNotifyMask);
    XMapWindow(disp,win);
    XFlush(disp);

    //wmから終了メッセージを受け取る
    Atom delwin = XInternAtom(disp,"WM_DELETE_WINDOW",True);
    XSetWMProtocols(disp,win,&delwin,1);

    WimeShowToolbar(cxn,true,true);
    WimeEnableIme(cxn,IME_ON);
    WimeReconvert(cxn,EjToU16(NULL,src),0,&pos);

    while(1){
	XNextEvent(disp,&ev);
	switch(ev.type){
	case KeyPress:
	    res=NULL;
	    st = WimeSendKey(cxn,ConvToVk(XKeycodeToKeysym(disp,evk->button,0),evk->state),&res);
	    free(res);
	    if(res!=NULL)
		goto fin;
	    break;
	case ConfigureNotify:
	    WimeMoveShadowWin(cxn,evc->x,evc->y-evc->height,evc->width,evc->height);
	    WimeSetCompWin(cxn,WIME_POS_POINT,0,evc->height);
	    break;
	case ClientMessage:
	    if(ev.xclient.data.l[0] == delwin)
		goto fin;
	}
    }

fin:
    WimeFlushMsg();
    WimeEndConvert(cxn,0,0,NULL,0);
    WimeEnableIme(cxn,IME_OFF);
    WimeCloseContext(cxn);
    XDestroyWindow(disp,win);
    XCloseDisplay(disp);
    return true;
}

//利用可能になるまで待つ
bool wait_usable(void)
{
    char name[] = STARTNAME;
    char buf;
    int fd,st;

    if(!(st = ini_wime())){
	WimeLock();
	mkfifo(name,LOCKFILEMODE);
	fd = open(name,O_CREAT|O_RDWR,LOCKFILEMODE);
	WimeUnlock();
	WimeLockClose();
	read(fd,&buf,1);
	close(fd);
	st = ini_wime();
    }
    unlink(name);
    return st;
}
