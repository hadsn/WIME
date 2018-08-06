// -*- coding:euc-jp -*-
/*
  wimeとのソケットや共有メモリの作成。
  このディレクトリの他の関数とは色合いが違うが、os64ビット-wine32ビットの場合
  32ビットwime本体からも64ビットlibwimeからも使われるため、このディレクトリに配置する。
*/
#define _GNU_SOURCE /*mremap*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <search.h>
#include <time.h>
#include "wimeconn.h"
#include "log.h"

#if defined(__FreeBSD__)
//mremapをつくる。mmapはそれに合うように変更。mmapの代わりにMMAPを呼ぶようにしている。
//lfindの比較関数のtypedef。
#include "freebsd.h"
#define MMAP mmap_freebsd
#else
//linuxでは何もする必要なし。
#define MMAP mmap
#endif

int Fd = -1;
char* SocketPath=NULL;

#define DEFAULT_SOCKET "/tmp/.iroha_unix/IROHA"
#define NUM_LEN 5 /* "65535" ソケット名に付け足す数値の最大文字数 */
/*
  socket_num:ソケットに追加する数値。
  かんなのソケットのパスを返す。後ろに付く数値は1...0xffffに限定される。
  文字列はfreeすること
*/
char* MakeSocketPath(int socket_num)
{
    char* buf = malloc(sizeof(DEFAULT_SOCKET)+1+NUM_LEN+1+1);
    const char* fmt = socket_num==0 ? "%s" : "%s:%u";
    sprintf(buf,fmt,DEFAULT_SOCKET,socket_num);
    return buf;
}

void DisconnectServer(void)
{
    if(Fd != -1){
	close(Fd);
	Fd = -1;
    }
}

//SocketPathに対してconnectする。
bool ConnectServer(void)
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
	DisconnectServer();
    return st;
}

#define PIDTABLE_PAGE 16
#define PIDTABLE_MAX 1024
static const char SHMNAME[]="/wimepid";
static const char SEM_LOCK[]="/wimelock";

#define get_shm_name() SHMNAME
#define get_lock_name() SEM_LOCK

/* lockにSEM_FAILED、tableにMAP_FAILEDがセットされるときがある。
   テーブルの大きさ(要素数)を返す。失敗したら０。
*/
int lock_pid_table(sem_t** lock,PidTableElt** table)
{
    int tbsize=0;
    int openflags = O_CREAT|O_RDWR;
#if defined(__FreeBSD__)
    openflags = O_CREAT;
#endif
    *table = MAP_FAILED;
    *lock = sem_open(get_lock_name(),openflags,LOCKFILEMODE,1);
    if(*lock != SEM_FAILED && sem_wait(*lock) == 0){
	int shm = shm_open(get_shm_name(),O_RDWR|O_CREAT,LOCKFILEMODE);
	if(shm != -1){
	    struct stat sb;
	    fstat(shm,&sb);
	    if(sb.st_size == 0){ //新規作成
		sb.st_size = (tbsize = PIDTABLE_PAGE)*sizeof(PidTableElt);
		ftruncate(shm,sb.st_size);
	    }else{
		tbsize = sb.st_size/sizeof(PidTableElt);
	    }
	    *table = MMAP(NULL,sb.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,shm,0);
	    if(*table == MAP_FAILED)
		tbsize = 0;
	    close(shm);
	}
    }else
	FATALLOG(CH_GLOBAL,"fail lock(%d) %s\n",errno,strerror(errno));
    return tbsize;
}

void unlock_pid_table(sem_t* lock,PidTableElt* table,int tbsize)
{
    if(table!=MAP_FAILED){
	if(munmap(table,tbsize*sizeof(PidTableElt))!=0)
	    FATALLOG(CH_GLOBAL,"fail munmap (%d) %m\n",errno);
    }
    if(lock!=SEM_FAILED){
	sem_post(lock);
	sem_close(lock);
	sem_unlink(get_lock_name());
    }
}

/*共有メモリを拡大する。新しいアドレスを返す。失敗したら元のアドレスを返す。
  tbsizeを更新する。
*/
void* resize_pid_table(void* adr,int* tbsize)
{
    if(*tbsize >= PIDTABLE_MAX)
	return adr;

    int shm = shm_open(get_shm_name(),O_RDWR,LOCKFILEMODE);
    int new_bytes = (*tbsize+PIDTABLE_PAGE)*sizeof(PidTableElt);
    if(ftruncate(shm,new_bytes) == 0){
	void* newadr = mremap(adr,*tbsize*sizeof(PidTableElt),new_bytes,MREMAP_MAYMOVE);
	if(newadr != MAP_FAILED){
	    *tbsize += PIDTABLE_PAGE;
	    adr = newadr;
	}
    }
    close(shm);
    return adr;
}

//サーバー開始時の共有メモリの処理
//WIMERESTARTSIGを送る。
void ShmStartServer(int socket_num)
{
    sem_t* lock;
    PidTableElt* pidtable;
    int tbsize = lock_pid_table(&lock,&pidtable);
    for(int x=0; x<tbsize; ++x){
	if(pidtable[x].Pid!=0 && pidtable[x].SocketNum==socket_num){
	    if(kill(pidtable[x].Pid,WIMERESTARTSIG) == 0)
		ERRORLOG(CH_GLOBAL,"send restart signal to pid %d\n",pidtable[x].Pid);
	    else{
		ERRORLOG(CH_GLOBAL,"clear pid %d\n",pidtable[x].Pid);
		pidtable[x].Pid = 0; //無効なpidだった
	    }
	}
    }
    unlock_pid_table(lock,pidtable,tbsize);
}

//lfind()の比較関数。a=&pid_t b=&PidTableElt
static int eq_pid(const void* a,const void* b){
    return *(const pid_t*)a == ((const PidTableElt*)b)->Pid ? 0 : 1;
}
static int neq_pid(const void* a,const void* b){
    return !eq_pid(a,b);
}

//lfind()が長ったらしい
static inline void* lfind_pid(pid_t pid,void* table,size_t tbsize,comparison_fn_t cmp)
{
    return lfind(&pid,table,&tbsize,sizeof(PidTableElt),cmp);
}

/*クライアント開始時の共有メモリの処理
  WimeInitialize()から呼び出される。
*/
void ShmStartClient(int socket_num,bool use_utf16)
{
    sem_t* lock;
    PidTableElt* pidtable;
    int tbsize = lock_pid_table(&lock,&pidtable);
    if(tbsize != 0){
	pid_t self = getpid();
	PidTableElt* p = lfind_pid(self,pidtable,tbsize,eq_pid);
	if(p == NULL){
	    //まだ登録されてないので空きを探す
	    if((p = lfind_pid(0,pidtable,tbsize,eq_pid)) == NULL){
		pidtable = resize_pid_table(pidtable,&tbsize);
		p = lfind_pid(0,pidtable,tbsize,eq_pid);
	    }
	    if(p != NULL){
		DEBUGLOG(CH_GLOBAL,"register pid %d\n",(int)self);
		*p = (PidTableElt){.Pid=self, .SocketNum=socket_num, .UseUtf16=use_utf16};
	    }else
		FATALLOG(CH_GLOBAL,"pid table full.\n");
	}else
	    INFOLOG(CH_GLOBAL,"already registered pid %d\n",(int)self);
    }
    unlock_pid_table(lock,pidtable,tbsize);
}

/*WimeFinalize()から呼び出される。
 */
void ShmEndClient(void)
{
    sem_t* lock;
    PidTableElt* pidtable;
    int tbsize = lock_pid_table(&lock,&pidtable);
    if(tbsize != 0){
	pid_t self = getpid();
	PidTableElt* p = lfind_pid(self,pidtable,tbsize,eq_pid);
	if(p != NULL){
	    p->Pid = 0;
	}else{
	    INFOLOG(CH_GLOBAL,"no register pid %d\n",self);
	}
	if(lfind_pid(0,pidtable,tbsize,neq_pid) == NULL){ //0以外のpidを探す
	    if(shm_unlink(get_shm_name()) != 0) //共有メモリを使っているプロセスがなくなったら削除
		FATALLOG(CH_GLOBAL,"fail shm_unlink (%d) %m\n",errno);
	}
    }
    unlock_pid_table(lock,pidtable,tbsize);
}

//テーブルからpidの情報を取得する。エラーがあったときはeltは変更しない。
bool ShmGetPidData(pid_t pid,PidTableElt* elt)
{
    sem_t* lock;
    PidTableElt* pidtable;
    int tbsize = lock_pid_table(&lock,&pidtable);
    if(tbsize != 0){
	PidTableElt* tab = lfind_pid(pid,pidtable,tbsize,eq_pid);
	if(tab != NULL)
	    *elt = *tab;
	else
	    elt = NULL; //エラー状態を示す。
    }
    unlock_pid_table(lock,pidtable,tbsize);
    return elt!=NULL;
}


#define SEM_OPEN_MODE (O_CREAT|O_RDWR)
#ifdef __FreeBSD__
#undef SEM_OPEN_MODE
#define SEM_OPEN_MODE O_CREAT
#endif

static const char SEMRUN[]="/wimerun";
#define SEMNAMEMAXLEN (sizeof(SEMRUN)+NUM_LEN)
static char* get_sem_name(int socket_num,char* name)
{
    snprintf(name,SEMNAMEMAXLEN,"%s%d",SEMRUN,socket_num);
    return name;
}

static sem_t* open_sem(int socket_num)
{
    char sem_name[SEMNAMEMAXLEN];
    return sem_open(get_sem_name(socket_num,sem_name),SEM_OPEN_MODE,LOCKFILEMODE,0);
}

//セマフォをオープンしてpost。サーバーが使う。
bool SemPost(int socket_num)
{
    bool st = false;
    sem_t* ini_sem = open_sem(socket_num);
    if(ini_sem != SEM_FAILED){
	st = (sem_post(ini_sem) == 0); //先に待っているプロセスがあればそれを起こす。
	//DEBUGDO(CH_GLOBAL,{int val;sem_getvalue(ini_sem,&val);MESG("sem-value %d\n",val);});
	sem_close(ini_sem);
    }
    if(!st)
	ERR("%s(%d)\n",strerror(errno),errno);
    return st;
}

//セマフォをオープンして<ms>ミリ秒待つ。ms<0なら無期限。
//postが異常なしでafter_waitがNULLでなければafter_waitを呼び出す。戻り値はafter_waitが返した値。
bool SemWait(wime_sem_after_wait after_wait,int socket_num,int ms)
{
    bool st=false,st_post=false;
    sem_t* ini_sem = open_sem(socket_num);
    if(ini_sem != SEM_FAILED){
	//DEBUGDO(CH_GLOBAL,{int val;sem_getvalue(ini_sem,&val);MESG("sem-value %d\n",val);});
	if(ms < 0)
	    st = (sem_wait(ini_sem) == 0);
	else{
	    struct timespec t;
	    clock_gettime(CLOCK_REALTIME,&t); 
	    t.tv_sec += ms/1000;
	    t.tv_nsec += (ms%1000)*1000;
	    st = (sem_timedwait(ini_sem,&t) == 0);
	}
	if(st){
	    st = (sem_post(ini_sem)==0);
	    if(st){
		st_post=true;
		if(after_wait!=NULL)
		    st = (*after_wait)(ini_sem);
	    }
	}
	sem_close(ini_sem);
    }
    if(!st_post && !st) //postが成功していればafter_waitの結果に対するエラー表示はしない。
	ERR("%s(%d)\n",strerror(errno),errno);
    return st;
}

void SemUnlink(int socket_num)
{
    char sem_name[SEMNAMEMAXLEN];
    sem_unlink(get_sem_name(socket_num,sem_name));
}

//(C) 2009 thomas
