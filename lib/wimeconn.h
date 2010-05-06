#ifndef WIME_LIB_WIMECONN
#define WIME_LIB_WIMECONN

#ifdef __cplusplus
extern "C"{
#endif

extern int Fd;
extern char* SocketPath;

enum{
    PID_WIME,
    PID_CLIENT,
    PID_MAX=256
};
extern pid_t *Pid;

#define WIMERESTARTSIG SIGUSR1
#define STARTNAME "/tmp/.wimestart"
#define LOCKFILEMODE 0600

    void WimeLock(void);
    void WimeUnlock(void);
    void WimeLockClose(void);

#ifndef LOGMARK
#define LOGMARK LogMark
#endif

int mkdirp(const char* p);
char* MakeSocketPath(int socket_num);
bool WimeConnect(void);
void WimeDisconnect(void);
void WimeShmInit(int logmark);
void WimeShmFin(void);

#ifdef __cplusplus
}
#endif

#endif
