#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "pkt.h"

#ifdef __cplusplus
extern "C" {
#endif

    bool Snd0(int fd, const char* ver, const char* user);
    bool Snd1(int fd, int prn);
    bool Snd2(int fd, int prn, int16_t p1);
    bool Snd3(int fd, int prn, int16_t p1, uint16_t p2);
    bool Snd4(int fd, int prn, int16_t p1, uint16_t p2, uint16_t p3, uint16_t p4, uint16_t* p5, int p5len);
    bool Snd5(int fd, int prn, int16_t p1, uint16_t p2, int32_t p3);
    bool Snd6(int fd, int prn, int16_t p1, int16_t p2, uint16_t p3);
    bool Snd7(int fd, int prn, int16_t p1, int16_t p2, int16_t p3);
    bool Snd9(int fd, int prn, int16_t p1, int16_t p2, int16_t, int16_t);
    bool Snd10(int fd, int prn, int16_t p1, int16_t p2, int32_t p3, int16_t* p4, int p4len);
    bool Snd11(int fd, int prn, int16_t p1, int16_t p2, const uint16_t* p3, int len);
    bool Snd14(int fd, int prn, int32_t p1, int16_t p2, const uint16_t* p3);
    bool Snd15(int fd, int prn, int32_t p1, int16_t p2, const char* p3);
    bool Snd17(int fd, int prn, const char* s);
    bool SndN(int fd, int prn, const void* r, unsigned size);

    void* RcvN(int fd, CanHeader* buf0, int bufsize);
    int Rcv0(int fd, int* ver);
    bool Rcv2(int fd, char* p1);
    bool Rcv3(int fd, char* p1, uint16_t** p2);
    int Rcv4v(int fd, char* p1, int32_t** p2);
    bool Rcv4(int fd, char* p1, int32_t* p2);
    bool Rcv5(int fd, int16_t*);
    bool Rcv6(int fd, int16_t* p1, char** p2);
    bool Rcv7(int fd, int16_t* p1, uint16_t** p2);
    uint16_t* Rcv8(int fd, int16_t* p1, uint16_t** p3);
    int Rcv9v(int fd, int16_t* p1, uint32_t** p2);
    bool Rcv10(int fd, char* p1, char** p2, char** p3, int32_t* p4);
    bool Rcv64(int fd, unsigned* p1, void** bin, unsigned* binbytes, char** str);

#ifdef __cplusplus
}
#endif

//(C) 2008 thomas
