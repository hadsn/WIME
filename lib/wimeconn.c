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
#include "wimeconn.h"
#include "log.h"

#if defined(__FreeBSD__)
//mremapをつくる。mmapはそれに合うように変更。mmapの代わりにMMAPを呼ぶようにしている。
//lfindの比較関数のtypedef。
#include "freebsd.c"
#else
//linuxでは何もする必要なし。
#define MMAP mmap
#endif

int Fd = -1;
char* SocketPath=NULL;

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

/* lockにSEM_FAILED、tableにMAP_FAILEDがセットされるときがある。
   テーブルの大きさを返す。失敗したら０。
*/
int lock_pid_table(sem_t** lock,pid_t** table)
{
    int tbsize=0;
    *table = MAP_FAILED;
    *lock = sem_open(SEM_LOCK,O_CREAT|O_RDWR,LOCKFILEMODE,1);
    if(*lock != SEM_FAILED && sem_wait(*lock) == 0){
	int shm = shm_open(SHMNAME,O_RDWR|O_CREAT,LOCKFILEMODE);
	if(shm != -1){
	    struct stat sb;
	    fstat(shm,&sb);
	    if(sb.st_size == 0){ //新規作成
		sb.st_size = (tbsize = PIDTABLE_PAGE)*sizeof(pid_t);
		ftruncate(shm,sb.st_size);
	    }else{
		tbsize = sb.st_size/sizeof(pid_t);
	    }
	    *table = MMAP(NULL,sb.st_size,PROT_READ|PROT_WRITE,MAP_SHARED,shm,0);
	    if(*table == MAP_FAILED)
		tbsize = 0;
	    close(shm);
	}
    }else
	LOG("fail lock(%d) %m\n",errno);
    return tbsize;
}

void unlock_pid_table(sem_t* lock,pid_t* table,int tbsize)
{
    if(table!=MAP_FAILED){
	if(munmap(table,tbsize*sizeof(pid_t))!=0)
	    ERR("fail munmap (%d) %m\n",errno);
    }
    if(lock!=SEM_FAILED){
	sem_post(lock);
	sem_close(lock);
	sem_unlink(SEM_LOCK);
    }
}

/*共有メモリを拡大する。新しいアドレスを返す。失敗したら元のアドレスを返す。
  tbsizeを更新する。
*/
void* resize_pid_table(void* adr,int* tbsize)
{
    if(*tbsize >= PIDTABLE_MAX)
	return adr;

    int shm = shm_open(SHMNAME,O_RDWR,LOCKFILEMODE);
    int new_bytes = (*tbsize+PIDTABLE_PAGE)*sizeof(pid_t);
    if(ftruncate(shm,new_bytes) == 0){
	void* newadr = mremap(adr,*tbsize*sizeof(pid_t),new_bytes,MREMAP_MAYMOVE);
	if(newadr != MAP_FAILED){
	    *tbsize += PIDTABLE_PAGE;
	    adr = newadr;
	}
    }
    close(shm);
    return adr;
}

//サーバー開始時の共有メモリの処理
void ShmStartServer(void)
{
    sem_t* lock;
    pid_t* pidtable;
    int tbsize;
    if((tbsize = lock_pid_table(&lock,&pidtable))){
	for(int x=0; x<tbsize; ++x){
	    if(pidtable[x] != 0){
		MSG("send restart signal to pid %d\n",pidtable[x]);
		if(kill(pidtable[x],WIMERESTARTSIG) != 0)
		    pidtable[x] = 0; //無効なpidだった
	    }
	}
    }
    unlock_pid_table(lock,pidtable,tbsize);
}

//lfind()の比較関数。
static int eq_pid(const void* a,const void* b){
    return *(const pid_t*)a == *(const pid_t*)b ? 0 : 1;
}
static int neq_pid(const void* a,const void* b){
    return !eq_pid(a,b);
}

//lfind()が長ったらしい
inline void* lfind_pid(pid_t pid,void* table,size_t tbsize,comparison_fn_t cmp)
{
    return lfind(&pid,table,&tbsize,sizeof(pid_t),cmp);
}

/*クライアント開始時の共有メモリの処理
  WimeInitialize()から呼び出される。
*/
void ShmStartClient(void)
{
    sem_t* lock;
    pid_t* pidtable;
    int tbsize;
    if((tbsize = lock_pid_table(&lock,&pidtable))){
	pid_t self=getpid(),*p;
	if((p = lfind_pid(self,pidtable,tbsize,eq_pid)) == NULL){
	    //まだ登録されてないので空きを探す
	    if((p = lfind_pid(0,pidtable,tbsize,eq_pid)) == NULL){
		pidtable = resize_pid_table(pidtable,&tbsize);
		p = lfind_pid(0,pidtable,tbsize,eq_pid);
	    }
	    if(p!=NULL){
		LOG("register pid %d\n",(int)self);
		*p = self;
	    }else
		ERR("pid table full.\n");
	}else
	    MSG("already registered pid %d\n",(int)self);
    }
    unlock_pid_table(lock,pidtable,tbsize);
}

/*WimeFinalize()から呼び出される。
 */
void ShmEndClient(void)
{
    sem_t* lock;
    pid_t* pidtable;
    int tbsize;
    if((tbsize = lock_pid_table(&lock,&pidtable))){
	pid_t self=getpid(),*p;
	if((p = lfind_pid(self,pidtable,tbsize,eq_pid)) != NULL){
	    *p = 0;
	}else{
	    ERR("no register pid %d\n",self);
	}
	if(lfind_pid(0,pidtable,tbsize,neq_pid) == NULL){ //0以外のpidを探す
	    if(shm_unlink(SHMNAME)!=0) //共有メモリを使っているプロセスがなくなったら削除
		ERR("fail shm_unlink (%d) %m\n",errno);
	}
    }
    unlock_pid_table(lock,pidtable,tbsize);
}

static const char SemRun[]="/wimerun";

#define SEM_OPEN_MODE (O_CREAT|O_RDWR)
#ifdef __FreeBSD__
#undef SEM_OPEN_MODE
#define SEM_OPEN_MODE O_CREAT
#endif

//セマフォをオープンしてpost。サーバーが使う。
bool SemPost(void)
{
    bool st = false;
    sem_t* ini_sem = sem_open(SemRun,SEM_OPEN_MODE,LOCKFILEMODE,0);
    if(ini_sem != SEM_FAILED){
	st = (sem_post(ini_sem) == 0); //先に待っているプロセスがあればそれを起こす。
	sem_close(ini_sem);
    }
    if(!st)
	ERR("%m(%d)\n",errno);
    return st;
}

//セマフォをオープンして待つ。
//postが異常なしでafter_waitがNULLでなければafter_waitを呼び出す。戻り値はafter_waitが返した値。
bool SemWait(wime_sem_after_wait after_wait)
{
    bool st=false,st_post=false;
    sem_t* ini_sem = sem_open(SemRun,O_CREAT|O_RDWR,LOCKFILEMODE,0);
    if(ini_sem != SEM_FAILED){
	if(sem_wait(ini_sem) == 0){
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
	ERR("%m(%d)\n",errno);
    return st;
}

void SemUnlink(void)
{
    sem_unlink(SemRun);
}

//(C) 2009 thomas
