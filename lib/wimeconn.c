/*
  wimeとのソケットや共有メモリの作成。
  このディレクトリの他の関数とは色合いが違うが、32ビットのwime本体からも
  64ビットのlibwimeからも使われるため、このディレクトリに配置する。
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <signal.h>
#include <libgen.h>
#include <errno.h>
#include "wimeconn.h"
#include "log.h"

int Fd = -1;
char* SocketPath;

char LogMark; //catch_restart_wime用に持っておく。メッセージ表示にも使う。

#define SHMNAME "/.wimeshm"
pid_t* Pid;
static int PidOffset;
#define PIDTAB_SIZE (sizeof(*Pid)*PID_MAX)


int mkdirp(const char* p)
{
    char *pp;
    int r=0;

    if(p[0]=='/' && p[1]==0)
	return 1;

    pp = strdup(p);
    if(mkdirp(dirname(pp))){
	r = (mkdir(p,0777)==0);
	if(r)
	    chmod(p,0777);
	else
	    if(errno==EEXIST)
		r=1;
    }
    free(pp);
    return r;
}


#define DEFAULT_SOCKET "/tmp/.iroha_unix/IROHA"
#define NUM_LEN 5 /* "65535" */
/*
  かんなのソケットのパスを返す。後ろに付く数値は0...0xffff
  文字列はfreeすること
*/
char* MakeSocketPath(int socket_num)
{
    char* buf = malloc(sizeof(DEFAULT_SOCKET)+1+NUM_LEN+1+1);
    const char* fmt = socket_num==0 ? "%s" : "%s:%u";

    sprintf(buf,fmt,DEFAULT_SOCKET,socket_num&0xffff);
    return buf;
}

void WimeDisconnect(void)
{
    if(Fd != -1){
	close(Fd);
	Fd = -1;
    }
}

bool WimeConnect(void)
{
    if(Fd != -1)
	return true;

    bool st;
    struct sockaddr_un sock_name;
    if((Fd = socket(AF_UNIX,SOCK_STREAM,0)) == -1)
	return false;
    sock_name.sun_family = AF_UNIX;
    strcpy(sock_name.sun_path,SocketPath);
    if(!(st=(connect(Fd,(struct sockaddr*)&sock_name,SUN_LEN(&sock_name))==0)))
	WimeDisconnect();
    return st;
}

#define LOCKFILENAME "/tmp/.wimememlock"
static int LockFd = -1;

void WimeLock(void)
{
    if(LockFd == -1)
	LockFd = open(LOCKFILENAME,O_CREAT|O_RDWR,LOCKFILEMODE);
    if(LockFd != -1)
	flock(LockFd,LOCK_EX);
}

void WimeUnlock(void)
{
    flock(LockFd,LOCK_UN);
}

void WimeLockClose(void)
{
    close(LockFd);
    LockFd = -1;
}

//logmark==0のときは再スタートシグナルハンドラからの呼び出し
//!!! サーバーからもクライアントからも呼ばれるというのは何とかしたい。
void WimeShmInit(int logmark)
{
    struct stat sb;
    int shm;
    pid_t pid = getpid();

    if(logmark != 0)
	LogMark = logmark;

    WimeLock();
    shm = shm_open(SHMNAME,O_RDWR|O_CREAT,LOCKFILEMODE);
    fstat(shm,&sb);
    if(sb.st_size == 0){ //新規作成
	ftruncate(shm,PIDTAB_SIZE);
    }
    Pid = mmap(NULL,PIDTAB_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shm,0);
    close(shm);
    if(LogMark == SERVER_MARK){
	//wime
	if(Pid[PID_WIME] != 0){ //すでにpidがある=再起動した
	    for(int x=PID_CLIENT; x<PID_MAX; ++x){
		if(Pid[x] != 0){
		    MSG("send restart signal to pid %d\n",Pid[x]);
		    if(kill(Pid[x],WIMERESTARTSIG) != 0)
			Pid[x] = 0; //無効なpidだった
		}
	    }
	}
	PidOffset = PID_WIME;
    }else{
	//xim,gim
	//wimeの再起動による再接続であればすでにpidが登録されている
	for(PidOffset=PID_CLIENT; PidOffset<PID_MAX; ++PidOffset)
	    if(Pid[PidOffset] == pid)
		break;
	if(PidOffset == PID_MAX){ //まだ登録されてないので空きを探す
	    for(PidOffset=PID_CLIENT; PidOffset<PID_MAX; ++PidOffset)
		if(Pid[PidOffset] == 0)
		    break;
	    if(PidOffset == PID_MAX)
		ERR("PID TABLE FULL.\n");
	}
    }
    if(PidOffset < PID_MAX){
	Pid[PidOffset] = pid;
	msync(Pid+PidOffset,sizeof(*Pid),MS_SYNC);
    }
    WimeUnlock();
}

void WimeShmFin(void)
{
    int use_shm;

    WimeLock();
    if(PidOffset < PID_MAX){
	Pid[PidOffset] = 0;
	msync(Pid+PidOffset,sizeof(*Pid),MS_SYNC);
    }
    for(use_shm=0; use_shm<PID_MAX; ++use_shm)
	if(Pid[use_shm] != 0)
	    break;
    WimeUnlock();
    munmap(Pid,PIDTAB_SIZE);
    if(use_shm == PID_MAX){ //使っているプロセスがなくなったら削除
	shm_unlink(SHMNAME);
    }
    WimeLockClose();
}
