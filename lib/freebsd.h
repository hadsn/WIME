#ifndef WIME_LIB_FREEBSD
#define WIME_LIB_FREEBSD

#ifdef __cplusplus
extern "C"{
#endif

void* mmap_freebsd(void* adr,size_t size,int prot,int flags,int fd,off_t offset);
void* mremap(void* old_adr,size_t old_size,size_t new_size,int flags,...);

typedef int (*comparison_fn_t)(const void*,const void*); //lfind()

void* mempcpy(void* d,const void* s,int n);
char* strtok_r(char* s,const char* d,char** p);

#define MREMAP_MAYMOVE 1

#ifdef __cplusplus
}
#endif

#endif

//(C) 2015 thomas
