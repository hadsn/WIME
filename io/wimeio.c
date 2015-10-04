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
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <regex.h>
#include <X11/Xlib.h>
#include <libgen.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "lib/array.h"
#include "wimeio.h"
#include "exe/canna.h"
#include "lib/ut.h"
#include "lib/wimeconn.h"
#include "so/wimeapi.h"

Array CannaFds;
int ActiveFd;
int ListenNum; //接続を受けるソケットの数。通常１。tcpも使うときは2

#define PERROR(s) fprintf(stderr,"%s:%d:%s\n",s,__LINE__,strerror(errno))

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
    regex_t reg;

    ArNew(&ht,sizeof(HinshiCor),NULL);
    ArNew(&lb,1,NULL);
    if((fp = fopen(fn,"r")) == NULL){
	tab = NULL;
    }else{
	int linenum=0;
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

	int bytesize = ht.use * ht.blocksize;
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

static bool make_socket(int domain,int type,int proto,struct sockaddr* addr,size_t addrlen)
{
    int skt;

    if((skt = socket(domain,type,proto)) == -1)
	return false;
    if(bind(skt,addr,addrlen)!=0 || listen(skt,SOMAXCONN)!=0){
	close(skt);
	return false;
    }
    ArAdd(&CannaFds,&skt);
    ++ListenNum;
    return true;
}

#define SERVER_ADDR "localhost"
#define SERVICE_NAME "canna"

/*
  boolを返す。
  socket_num pオプションの数値
  use_top 0=tcpは使わない。-1=デフォルトサービス名を使う。 >0=ポート番号とする
*/
int ImInit(unsigned socket_num,int use_tcp)
{
    struct sockaddr_un sock_name;
    char *sock_path_cp;

    errno = 0;
    ArNew(&CannaFds,sizeof(int),NULL);

    SocketPath = MakeSocketPath(socket_num);
    MkDir(dirname(sock_path_cp = strdup(SocketPath)));
    free(sock_path_cp);
    chmod(SocketPath,0777);

    sock_name.sun_family = AF_UNIX;
    strcpy(sock_name.sun_path,SocketPath);
    if(!make_socket(AF_UNIX,SOCK_STREAM,0,(struct sockaddr*)&sock_name,SUN_LEN(&sock_name))){
	PERROR(__func__);
	return 0;
    }

    if(use_tcp){
	struct addrinfo *ai,*rp,hint;
	int st;
	char port[8];
	
	if(use_tcp > 0){
	    sprintf(port,"%d",use_tcp&0xffff);
	    memset(&hint,0,sizeof(hint));
	    hint.ai_family = AF_INET;
	    hint.ai_socktype = SOCK_STREAM;
	    rp = &hint;
	}else{
	    //ポート指定なしの時はできるだけシステムに任せる
	    strcpy(port,SERVICE_NAME);
	    rp = NULL;
	}
	if((st = getaddrinfo(SERVER_ADDR,port,rp,&ai)) != 0){
	    printf("%s:%s\n",__func__,gai_strerror(st));
	    if(st == EAI_SYSTEM)
		PERROR(__func__);
	    return 0;
	}

	((struct sockaddr_in*)(ai->ai_addr))->sin_port += htonl(socket_num);
	for(rp=ai; rp!=NULL; rp=rp->ai_next)
	    if(make_socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol,rp->ai_addr,rp->ai_addrlen))
		break;
	if(rp == NULL){
	    PERROR(__func__);
	    return 0;
	}
	freeaddrinfo(ai);
    }
    return 1;
}

//応答があったファイルディスクリプタを返す
int ImSelect(void)
{
    int n,fd;
    fd_set rs;

    if(ArUsing(&CannaFds) == 0) //０なら終了処理中
	return 0;

    while(1){
	FD_ZERO(&rs);
	int maxfd = 0;

	for(n=0; n<ArUsing(&CannaFds); ++n){
	    fd = *(int*)ArElem(&CannaFds,n);
	    FD_SET(fd,&rs);
	    if(fd > maxfd)
		maxfd = fd;
	}

	if(select(maxfd+1,&rs,NULL,NULL,NULL) <= 0){
	    PERROR(__func__);
	    if(errno==EINTR)
		continue;
	    return 0;
	}

	for(n=0; n<ArUsing(&CannaFds); ++n){
	    fd = *(int*)ArElem(&CannaFds,n);
	    if(FD_ISSET(fd,&rs))
		break;
	}

	if(n >= ListenNum)
	    break; //クライアントとの通信があった

	//fdにはlistenしているソケットが入っている
	if((fd = accept(fd,NULL,NULL)) < 0){
	    PERROR(__func__);
	    continue;
	}
	ArAdd(&CannaFds,&fd);
    }

    return ActiveFd = fd;
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
    for(int n=0; n<ArUsing(&CannaFds); ++n){
	shutdown(*(int*)ArElem(&CannaFds,n),SHUT_RD);
    }
    ArDelete(&CannaFds);
    unlink(SocketPath);
    return 1;
}

static Display* Disp;
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

    SemUnlink(); //!!!明示的に呼び出すべき？
}

/*コンストラクタにするとselect待ちになる前にロック解除してしまうので
  明示的に呼び出すことにする。*/
//__attribute__((constructor))
void ImSemStart(void)
{
    SemPost();
}

//(C) 2009 thomas
