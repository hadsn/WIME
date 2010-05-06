#ifndef WIME_LIB_LIST
#define WIME_LIB_LIST

#ifdef __cplusplus
extern "C"{
#endif

int SubList(const char* g,const char* s);
int ListCount(const char* l);
int ListRemove(char* l,int n);
int ListFind(const char* l,const char* x);
char* ListInsert(char* l0,int pos,const char* x);
char* ListInc(char* l,int n);

#ifdef __cplusplus
}
#endif

#endif
