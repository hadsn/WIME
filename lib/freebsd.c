#include "array.h"

typedef struct{
  int Fd;
  void* Adr;
} FdAdrPair;

Array FdAdr;

void* MMAP(void* adr,size_t size,int prot,int flags,int fd,off_t offset)
{
    adr = mmap(adr,size,prot,flags,fd,offset);
    if(ArUsing(&FdAdr)==0)
	ArNew(&FdAdr,sizeof(FdAdrPair),NULL);
    FdAdrPair fa={fd,adr};
    ArAdd(&FdAdr,&fa);
    return adr;
}

int find_adr(const void* e,const void* v)
{
    return (((const FdAdrPair*)e)->Adr == v);
}

#define MREMAP_MAYMOVE 1

void* mremap(void* old_adr,size_t old_size,size_t new_size,int flags,...)
{
    size_t save_size = (old_size<new_size ? old_size:new_size);
    void* tmpbuf = malloc(save_size);
    memcpy(tmpbuf,old_adr,save_size);
    int pos = ArFindIf(&FdAdr,0,find_adr,old_adr);
    if(pos<0){
	errno = EFAULT;
	return MAP_FAILED;
    }
    int fd = ((FdAdrPair*)ArElem(&FdAdr,pos))->Fd;

    munmap(old_adr,old_size);
    void* new_adr = mmap(NULL,new_size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    memcpy(new_adr,tmpbuf,save_size);
    ((FdAdrPair*)ArElem(&FdAdr,pos))->Adr = new_adr;
    return new_adr;
}

typedef int (*comparison_fn_t)(const void*,const void*); //lfind()

//(C) 2015 thomas
