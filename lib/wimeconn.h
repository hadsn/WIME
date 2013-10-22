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
#define LOCKFILEMODE 0600

#ifndef LOGMARK
#define LOGMARK LogMark
#endif

char* MakeSocketPath(int socket_num);
bool ConnectServer(void);
void DisconnectServer(void);

void ShmStartServer(void);
void ShmStartClient(void);
void ShmEndClient(void);

#include <semaphore.h>

typedef bool (*wime_sem_after_wait)(sem_t*);
bool SemPost(void);
bool SemWait(wime_sem_after_wait);
void SemUnlink(void);

#ifdef __cplusplus
}
#endif

#endif

//(C) 2009 thomas
