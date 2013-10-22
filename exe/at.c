#include <windows.h>
#include <stdint.h>
#include <imm.h>
#include <stdio.h>
#include "canna.h"
#include "apisup.h"
#include "lib/ut.h"
#include "lib/array.h"

#define AT_OK		0
#define AT_FAIL		-1
#define AT_NOTATOK	-2
#define ATDICFILESETNICKNAME_MAX	81
#define	ATCHECKVERSION			0
#define	ATCHECKVERSION_ORGREATER	1

HINSTANCE AtDll;

bool at_get_dic_list(CanHeader *ch,int fd);
bool at_get_dir_list(CanHeader *ch,int fd);

int WINAPI AT_GetDicFileSetNickname(HIMC imc,int fno,uint16_t* str)
{
    static typeof(AT_GetDicFileSetNickname)* funcp;
    if(funcp == NULL)
	funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);
    return funcp(imc,fno,str);
}

int WINAPI AT_GetDefaultDicNo(HIMC imc)
{
    static typeof(AT_GetDefaultDicNo)* funcp;
    if(funcp == NULL)
	funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);
    return funcp(imc);
}

int WINAPI AT_SetDefaultDicNo(HIMC imc,int n)
{
    static typeof(AT_SetDefaultDicNo)* funcp;
    if(funcp == NULL)
	funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);
    return funcp(imc,n);
}

BOOL WINAPI AT_IsATOKDefaultIME(int ver,int mode)
{
    static typeof(AT_IsATOKDefaultIME)* funcp;
    if(funcp == NULL)
	funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);
    return funcp(ver,mode);
}

/*
  ??? ここらへんのwineのパッチはどうするか？
*/
bool AtInit(WMCANNAPROTO* tab[])
{
    struct{
	int mj,mn;
	WMCANNAPROTO func;
    } sp[]={
	{0x06,0,at_get_dic_list},
	{0x07,0,at_get_dir_list},
	{0,0,NULL}
    },*p;

    AtDll = LoadLibrary("atoklib.dll");
    if(AtDll == NULL){
	MSG("fail LoadLibray() atoklib.dll\n");
	return false;
    }
    if(!AT_IsATOKDefaultIME(12,ATCHECKVERSION_ORGREATER)){
	MSG("atok is not default ime.\n");
	return false;
    }

    for(p=sp; p->func!=NULL; ++p)
	tab[p->mn][p->mj] = p->func;
    LOG("ok\n");
    return true;
}

/* 選択されている辞書セットの名前を返す */
//06
bool at_get_dic_list(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    int len=0;
    uint16_t u[ATDICFILESETNICKNAME_MAX];
    char ej[ATDICFILESETNICKNAME_MAX*3];
    CannaContext_t *cx;

    Req3(ch,&cxn,&bufsize);
    LOG("context %hd, buffer size %hd\n",cxn,bufsize);

    if((cx = ValidContext(cxn,__FUNCTION__))!=NULL || bufsize>=ATDICFILESETNICKNAME_MAX){
	HIMC imc = ImmGetContext(cx->Win);
	int dn=AT_SetDefaultDicNo(imc,0);
	if((dn = AT_GetDefaultDicNo(imc)) >= 0){
	    AT_GetDicFileSetNickname(imc,dn,u);
	    U16ToEj(ej,u,-1);
	    len = strlen(ej)+1;
	    ej[len++] = 0; //リストの終了マーク。lenはマークを含めたバイト数になる。
	    LOG("dic number=%d,name='%s'\n",dn,ej);
	}
	ImmReleaseContext(cx->Win,imc);
    }
    return Reply6(ch->Major,ch->Minor,(len!=0?1:-1),ej,len);
}

/* 有効な辞書セット名をリストにする */
//07
bool at_get_dir_list(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    int n,len;
    Array lst;
    bool st;
    CannaContext_t *cx;

    Req3(ch,&cxn,&bufsize);
    LOG("context %hd, buffer size %hd\n",cxn,bufsize);

    n = 0;
    ArNew(&lst,1,NULL);
    if((cx = ValidContext(cxn,__FUNCTION__))!=NULL){
	uint16_t u[ATDICFILESETNICKNAME_MAX];
	char ej[ATDICFILESETNICKNAME_MAX*3];
	HIMC imc = ImmGetContext(cx->Win);
	Array lb;

	ArNew(&lb,1,NULL);
	while(AT_GetDicFileSetNickname(imc,n,u) == AT_OK){
	    U16ToEj(ej,u,-1);
	    ArAddN(&lst,ej,strlen(ej)+1);
	    ++n;
	    ArPrint(&lb,"[%s]",ej);
	}
	ArAdd1(&lst,0);
	ImmReleaseContext(cx->Win,imc);
	LOG("dics:%s\n",ArAdr(&lb));
	ArDelete(&lb);
    }

    len = (n>0 ? lst.use : 0);
    if(len > bufsize){
	n = -1;
	len = 0;
    }
    st = Reply6(ch->Major,ch->Minor,n,lst.adr,len);
    ArDelete(&lst);
    return st;
}
