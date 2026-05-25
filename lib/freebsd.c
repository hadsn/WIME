
#include "array.h"
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <errno.h>

typedef struct {
    int Fd;
    void* Adr;
} FdAdrPair;

static Array FdAdr;

void* mmap_freebsd(void* adr, size_t size, int prot, int flags, int fd, off_t offset)
{
    if ((adr = mmap(adr, size, prot, flags, fd, offset)) != MAP_FAILED) {
        if (ArUsing(&FdAdr) == 0)
            ArNew(&FdAdr, sizeof(FdAdrPair), NULL);
        FdAdrPair fa = { fd,adr };
        if (!ArAdd1(&FdAdr, &fa)) {
            munmap(adr, size);
            adr = MAP_FAILED;
            errno = EFAULT;
        }
    }
    return adr;
}

static int find_adr(const void* e, const void* v)
{
    return (((const FdAdrPair*)e)->Adr == v);
}

void* mremap(void* old_adr, size_t old_size, size_t new_size, int flags, ...)
{
    int pos = ArFindIf(&FdAdr, 0, find_adr, old_adr);
    if (pos < 0) {
        errno = EFAULT;
        return MAP_FAILED;
    }
    int fd = ((FdAdrPair*)ArElem(&FdAdr, pos))->Fd;
    size_t save_size = (old_size < new_size ? old_size : new_size);
    void* new_adr = MAP_FAILED;
    void* tmpbuf = malloc(save_size);
    if (tmpbuf == NULL) {
        errno = ENOMEM;
    }
    else {
        memcpy(tmpbuf, old_adr, save_size);
        munmap(old_adr, old_size);
        new_adr = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (new_adr != MAP_FAILED) {
            memcpy(new_adr, tmpbuf, save_size);
            ((FdAdrPair*)ArElem(&FdAdr, pos))->Adr = new_adr;
        }
        free(tmpbuf);
    }
    return new_adr;
}

#ifndef FREEBSD_MEMPCMP
void* mempcpy(void* d, const void* s, int n)
{
    return (char*)memcpy(d, s, n) + n;
}
#endif

//??? -mno-cygwinがあると定義されない?
//char *strtok_r(char*,const char*,char**);
/* gccの-Hで見ると、no-cygwinがあるときのstring.hは/usr/local/include/wine/msvcrt,
   無いときはそれ以外(/usr/include)から読み込んでいる。で、wine-1.1.1の段階で
   msvcrt/string.hにstrtok_rは無い。
   →windowsではstrtok_sみたい。でもwineにはない。
*/
char* strtok_r(char* s, const char* d, char** p)
{
    if (s == NULL)
        s = *p;
    if (s != NULL) {
        s += strspn(s, d);
        if (*s != 0) {
            *p = s + strcspn(s, d);
            if (**p != 0)
                *((*p)++) = 0;
        }
        else
            *p = s = NULL;
    }
    return s;
}

//(C) 2015 thomas
