/*
  wime本体とlinux側との通信
  元はsoディレクトリにあったが、32ビットでコンパイルする必要があるため独立させた。
  wime.dllと１対１に対応するので、dllディレクトリに移動させる方がいいかもしれない。
*/

#include <sys/select.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include <X11/Xlib.h>
#include <libgen.h>
#include <fcntl.h>
#include "lib/array.h"
#include "wimeio.h"
#include "exe/canna.h"
#include "lib/wimeconn.h"
#include "so/wimeapi.h"

Array CannaFds;
int ActiveFd;
//char* SocketPath; //wimeconnのものを使う

/*
#ifndef SUN_LEN
#define SUN_LEN(n) (sizeof(n)-sizeof((n)->sun_path)+strlen((n)->sun_path))
#endif
*/

//改行文字もヌル文字もつかない
int get_line(FILE* stream,Array* ws)
{
    int c;
    while(c=fgetc(stream),c!=EOF && c!='\n'){
	ArAdd(ws,&c);
    }
    return c!=EOF;
}

HinshiCor* read_hinshi_def(char* fn)
{
    FILE *fp;
    Array ht,lb;
    char delim[]=" \t",*tok;
    HinshiCor hc,*tab;
    int linenum=0,bytesize;
    regex_t reg;

    ArNew(&ht,sizeof(HinshiCor),NULL);
    ArNew(&lb,1,NULL);
    if((fp = fopen(fn,"r")) == NULL){
	tab = NULL;
    }else{
	while(get_line(fp,&lb)){
	    ArAdd1(&lb,0);
	    ++linenum;
	    if(lb.use==1 || *(char*)lb.adr=='#')
		continue; //空行かコメント行
	    hc.Wcode = strtoul(lb.adr,&tok,0);
	    if(lb.adr == tok){
		//数値以外のものが書かれている
		printf("%s:%d:hinshi file format error\n",fn,linenum);
		continue;
	    }
	    strtok(tok,delim); //品詞名は無視
	    while((tok=strtok(NULL,delim)) != NULL){
		//正規表現のチェック
		if(regcomp(&reg,tok,REG_EXTENDED) == 0){
		    hc.Ccode = strdup(tok);
		    ArAdd(&ht,&hc);
		    regfree(&reg);
		}else{
		    printf("%s:%d:regex error\n",fn,linenum);
		}
	    }
	    ArClear(&lb);
	}
	fclose(fp);

	//終了マーク
	hc.Ccode = NULL;
	ArAdd(&ht,&hc);

	bytesize = ht.use * ht.blocksize;
	tab = memcpy(malloc(bytesize),ht.adr,bytesize);
    }

    ArDelete(&ht);
    ArDelete(&lb);
    return tab;
}

#define SYSTEMWIDECONFDIR PREFIX "/share/wime"
#define HINSHIPATH "/hinshi"
#define maxint(a,b) ({int _a = (a), _b = (b); _a > _b ? _a : _b; })

/* 設定ファイルを読み込む
   戻り値：0=失敗 1=ローカル 2=システム
*/
int ImReadSetting(void* _gd)
{
    int st=1;
    struct GlobalData_t* gd = (struct GlobalData_t*)_gd;
    char* home = getenv("HOME");
    char hinshifile[maxint(strlen(home)+1+sizeof(CONFDIR),sizeof(SYSTEMWIDECONFDIR))+sizeof(HINSHIPATH)+1];
    sprintf(hinshifile,"%s/%s",home,CONFDIR HINSHIPATH);
    free(gd->HinshiTab);
    if((gd->HinshiTab = read_hinshi_def(hinshifile)) == NULL){
	strcpy(hinshifile,SYSTEMWIDECONFDIR HINSHIPATH);
	gd->HinshiTab = read_hinshi_def(hinshifile);
	st = 2;
    }
    if(gd->HinshiTab == NULL)
	st = 0;
    return st;
}

//errnoが返る。エラーがなければ０
//socket_num=pオプションの数値
int ImInit(unsigned socket_num)
{
    struct sockaddr_un sock_name;
    char *sock_path_cp;
    int cannasocket;

    errno=0;

    SocketPath = MakeSocketPath(socket_num);
    mkdirp(dirname(sock_path_cp = strdup(SocketPath)));
    free(sock_path_cp);

    if((cannasocket = socket(AF_UNIX,SOCK_STREAM,0)) == -1){
	perror(__FUNCTION__);
	return errno;
    }

    sock_name.sun_family = AF_UNIX;
    strcpy(sock_name.sun_path,SocketPath);
    if(bind(cannasocket,(struct sockaddr*)&sock_name,SUN_LEN(&sock_name)) != 0){
	perror(__FUNCTION__);
	close(cannasocket);
	return errno;
    }
    chmod(SocketPath,0777);

    if(listen(cannasocket,SOMAXCONN) != 0){
	perror(__FUNCTION__);
	close(cannasocket);
    }

    ArNew(&CannaFds,sizeof(int),NULL);
    ArAdd(&CannaFds,&cannasocket);
    return 0;
}


//応答があったファイルディスクリプタを返す
int ImSelect(void)
{
    int n,fd,maxfd,cont_loop;
    fd_set rs;

    if(CannaFds.use == 0) //０なら終了処理中
	return 0;

    do{
	FD_ZERO(&rs);
	maxfd = cont_loop = 0;
	for(n=0; n<ArUsing(&CannaFds); ++n){
	    fd = *(int*)ArElem(&CannaFds,n);
	    FD_SET(fd,&rs);
	    if(fd > maxfd)
		maxfd = fd;
	}
	if(select(maxfd+1,&rs,NULL,NULL,NULL) <= 0){
	    if(errno==EINTR){
		fprintf(stderr,"%s:select eintr\n",__FUNCTION__);
		continue;
	    }
	    perror(__FUNCTION__);
	    return 0;
	}
	fd = *(int*)ArElem(&CannaFds,0); //先頭=ソケット→接続要求
	if(FD_ISSET(fd,&rs)){
	    if((fd = accept(fd,NULL,NULL)) < 0){
		perror(__FUNCTION__);
		return 0;
	    }
	    ArAdd(&CannaFds,&fd);
	    cont_loop = 1;
	}
    }while(cont_loop);

    ActiveFd = 0;
    for(n=0; n<ArUsing(&CannaFds); ++n){
	fd = *(int*)ArElem(&CannaFds,n);
	if(FD_ISSET(fd,&rs)){
	    ActiveFd = fd; //応答のあったfdをActiveFdへ
	    break;
	}
    }
    return ActiveFd;
}

int ImRead(void* buf,int len)
{
    return (int)read(ActiveFd,buf,len);
}

//lenだけ書き込まれたら1を返す
bool ImWrite(const void *buf,int len)
{
    return write(ActiveFd,buf,len)==(ssize_t)len;
}

int ImDisconnect(void)
{
    close(ActiveFd);
    ArRemove(&CannaFds,ArFind(&CannaFds,0,&ActiveFd));
    return ActiveFd;
}

int ImCloseAll(void)
{
    for(int n=0; n<CannaFds.use; ++n){
	close(*(int*)ArElem(&CannaFds,n));
    }
    ArDelete(&CannaFds);
    unlink(SocketPath);
    return 1;
}

Display* Disp;
void ImAuxInput(unsigned xw)
{
    XKeyPressedEvent ev;

    if(Disp == NULL){
	Disp = XOpenDisplay(NULL);
    }

    ev.type = KeyPress;
    ev.display = Disp;
    ev.root = XDefaultRootWindow(Disp);
    ev.window = xw;
    ev.subwindow = None;
    ev.time = CurrentTime;
    //ev.x = ev.y = 1; //ev.x_root = ev.y_root = 1;
    ev.same_screen = true;
    ev.state = AUX_INPUT_MOD;
    ev.keycode = 8;
    XSetInputFocus(Disp,xw,RevertToNone,CurrentTime); //ximでは必要
    XSendEvent(Disp,xw,true,KeyPressMask,(XEvent*)&ev);
    XFlush(Disp);
}

__attribute__((destructor))
void close_disp()
{
    if(Disp != NULL)
	XCloseDisplay(Disp);
}


/*コンストラクタにするとselect待ちになる前にロック解除してしまうので
  明示的に呼び出すことにする。*/
//__attribute__((constructor))
void WimeSemStart(void)
{
    char name[] = STARTNAME; //wimeconn.hにある
    int fd;

    WimeLock();
    mkfifo(name,LOCKFILEMODE);
    if((fd = open(name,O_CREAT|O_RDWR,LOCKFILEMODE)) != -1){
	char buf;
	write(fd,&buf,1);
	close(fd);
    }
    WimeUnlock();
}
