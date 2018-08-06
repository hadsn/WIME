#ifndef WIME_LIB_WIMECONN
#define WIME_LIB_WIMECONN

#include <sys/types.h> //pid_t
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

extern int Fd;
extern char* SocketPath;

#define WIMERESTARTSIG SIGUSR1
#define LOCKFILEMODE (S_IRUSR|S_IWUSR) /*0600*/

char* MakeSocketPath(int socket_num);
bool ConnectServer(void);
void DisconnectServer(void);

//共有メモリに作るテーブル
typedef struct{
    pid_t Pid;
    int SocketNum;
    bool UseUtf16;
} PidTableElt;

void ShmStartServer(int socket_num);
void ShmStartClient(int socket_num,bool use_utf16);
void ShmEndClient(void);
bool ShmGetPidData(pid_t pid,PidTableElt* elt);

#include <semaphore.h>

typedef bool (*wime_sem_after_wait)(sem_t*);
bool SemPost(int socket_num);
bool SemWait(wime_sem_after_wait,int socket_num,int ms);
void SemUnlink(int socket_num);

#ifdef __cplusplus
}
#endif

#endif

//(C) 2009 thomas
