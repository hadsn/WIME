#include <string.h>

//リストgにリストsが含まれていれば開始文字列番号を返す
int SubList(const char* g,const char* s)
{
    int pos=0;
    const char *s0=s;
    while(*g!=0 && *s!=0){
	if(strcmp(g,s) == 0){
	    s += strlen(s)+1;
	}else{
	    ++pos;
	    s = s0;
	}
	g += strlen(g)+1;
    }
    return *s==0 ? pos : -1;
}

//リストの数を返す
int ListCount(const char* l)
{
    int n=0;
    if(l != NULL){
	while(*l != 0){
	    l += strlen(l)+1;
	    ++n;
	}
    }
    return n;
}

//リストのバイト数を返す（ヌル文字を含む）
static int total_size(const char* l)
{
    int sz=1; //終了マークの分
    while(*l != 0){
	int len = strlen(l)+1;
	sz += len;
	l += len;
    }
    return sz;
}

//n番目の要素へ
char* ListInc(char* l,int n)
{
    while(--n >= 0)
	l += strlen(l)+1;
    return l;
}

/*
  n(>=0)番目の要素を削除する
  削除したバイト数を返す
*/
int ListRemove(char* l,int n)
{
    l = ListInc(l,n);
    char *ls = ListInc(l,1); //(n+1)番目の要素
    memcpy(l,ls,total_size(ls));
    return ls-l;
}

//要素xの番号を返す
int ListFind(const char* l,const char* x)
{
    int len=strlen(x)+1;
    char buf[len+1];
    memcpy(buf,x,len);
    buf[len] = 0;
    return SubList(l,buf);
}

//リストlの位置posにxを挿入する
//pos==-1のときリストの最後に追加する
char* ListInsert(char* l0,int pos,const char* x)
{
    if(pos < 0)
	pos = ListCount(l0);

    char *l = ListInc(l0,pos);
    int sz = strlen(x)+1;
    memcpy(memmove(l+sz,l,total_size(l)),x,sz);
    return l0;
}

//(C) 2008 thomas
