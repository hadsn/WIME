// -*- coding:euc-jp -*-
#include <windows.h>
#include <stdint.h>
#include <imm.h>
#include <stdio.h>
#include "canna.h"
#include "apisup.h"
#include "lib/ut.h"
#include "lib/array.h"
#include "lib/log.h"
#include "lib/list.h"

#define AT_OK		0
#define AT_FAIL		-1
#define AT_NOTATOK	-2
#define ATASSISTDICMAX			4
#define ATDICFILENAME_MAX		(256+2)
#define ATDICFILESETNICKNAME_MAX	(80+1)
#define	ATCHECKVERSION			0
#define	ATCHECKVERSION_ORGREATER	1

typedef struct{
    char* UserDicName;
    char* SystemDicName;
    char* AssistDicName[ATASSISTDICMAX];
} ATDICFILENAMESET;

HINSTANCE AtDll;

bool at_get_dic_list(CanHeader *ch,int fd);
bool at_get_dir_list(CanHeader *ch,int fd);

#define ATFUNC0(rettype,fname)\
    rettype WINAPI fname(void)\
    {\
	static typeof(fname)* funcp;\
	if(funcp == NULL)\
	    funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);\
	return funcp();\
    }
#define ATFUNC1(rettype,fname,a1type,a1name)\
    rettype WINAPI fname(a1type a1name)\
    {\
	static typeof(fname)* funcp;\
	if(funcp == NULL)\
	    funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);\
	return funcp(a1name);\
    }
#define ATFUNC2(rettype,fname,a1type,a1name,a2type,a2name)\
    rettype WINAPI fname(a1type a1name,a2type a2name)\
    {\
	static typeof(fname)* funcp;\
	if(funcp == NULL)\
	    funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);\
	return funcp(a1name,a2name);\
    }
#define ATFUNC3(rettype,fname,a1type,a1name,a2type,a2name,a3type,a3name)\
    rettype WINAPI fname(a1type a1name,a2type a2name,a3type a3name)\
    {\
	static typeof(fname)* funcp;\
	if(funcp == NULL)\
	    funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);\
	return funcp(a1name,a2name,a3name);\
    }
#define ATFUNC4(rettype,fname,a1type,a1name,a2type,a2name,a3type,a3name,a4type,a4name) \
    rettype WINAPI fname(a1type a1name,a2type a2name,a3type a3name,a4type a4name)\
    {\
	static typeof(fname)* funcp;\
	if(funcp == NULL)\
	    funcp = (typeof(funcp))GetProcAddress(AtDll,__FUNCTION__);\
	return funcp(a1name,a2name,a3name,a4name);\
    }
ATFUNC3(int,AT_GetDicFileSetNickname,HIMC,imc,int,fno,uint16_t*,str)
ATFUNC1(int,AT_GetDefaultDicNo,HIMC,imc)
ATFUNC2(int,AT_SetDefaultDicNo,HIMC,imc,int,n)
ATFUNC2(BOOL,AT_IsATOKDefaultIME,int,ver,int,mode)
ATFUNC2(BOOL,AT_IsATOKInstall,int,ver,int,mode)
ATFUNC0(int,AT_GetATOKLatestInstallVersion)
ATFUNC3(int,AT_GetDicFileNameSet,HIMC,imc,int,fno,ATDICFILENAMESET*,dic_name_pack)

static int get_atok_version(void)
{
    return AT_GetATOKLatestInstallVersion();
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
	FATALLOG(CH_CANNA,"fail LoadLibray() atoklib.dll\n");
	return false;
    }
    if(!AT_IsATOKInstall(12,ATCHECKVERSION_ORGREATER)){
	FATALLOG(CH_CANNA,"atok is installed incompletely.\n");
	return false;
    }

    for(p=sp; p->func!=NULL; ++p)
	tab[p->mn][p->mj] = p->func;

    WimeData.GetCandidate = GetCandidateAtok;
    WimeData.ImeVersion = get_atok_version;
    return true;
}

/*06 辞書テーブル一覧 [atok]
辞書テーブル(辞書リスト(マウントリスト)に登録可能な辞書群)に登録されている辞書一覧を取得する．
要求パケット(Type 3)
	i16	コンテクスト番号
	u16	辞書名リストのバッファサイズ
応答パケット(Type 6)
	i16	辞書数  エラー時: −1
	s8	辞書名リスト '辞書名@...@辞書名@@'

デフォルト辞書セットの辞書ファイル名をリストにする。
が、u16モードの時の文字コードはどうするか？ s8なのでutf16はまずいだろう。
→通常モードではeuc-jpを、u16モードの時はutf8を返すことにする。
 */
bool at_get_dic_list(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    Req3(ch,&cxn,&bufsize);
    DEBUGLOG(CH_CANNA,"context %hd, buffer size %hd\n",cxn,bufsize);

    Array lst;
    ArNew(&lst,1,NULL);
    HIMC imc;
    CannaContext_t* cx = GetContext(cxn,&imc,__FUNCTION__);
    if(cx != NULL){
	ATDICFILENAMESET dicnames;

	dicnames.UserDicName = calloc(1,ATDICFILENAME_MAX);
	dicnames.SystemDicName = calloc(1,ATDICFILENAME_MAX);
	for(int num=0; num<ATASSISTDICMAX; ++num)
	    dicnames.AssistDicName[num] = calloc(1,ATDICFILENAME_MAX);
	
	if(AT_GetDicFileNameSet(imc,AT_GetDefaultDicNo(imc),&dicnames) == AT_OK){
	    char* (*sj_to_xx)(char*,const char*,int) = (cx->Flags & USE_UTF16)==0 ? SjToEj : SjToU8;
	    char* convname = malloc(ATDICFILENAME_MAX*2);
	    ArAddStr(&lst,(*sj_to_xx)(convname,dicnames.UserDicName,-1));
	    ArAddStr(&lst,(*sj_to_xx)(convname,dicnames.SystemDicName,-1));
	    for(int num=0; num<ATASSISTDICMAX; ++num){
		if(dicnames.AssistDicName[num] != NULL){
		    ArAddStr(&lst,(*sj_to_xx)(convname,dicnames.AssistDicName[num],-1));
		}
	    }
	    ArAddChar(&lst,0); //リスト終了コード
	    free(convname);
	}else
	    INFOLOG(CH_CANNA,"fail AT_GetDicFileNameSet\n");

	free(dicnames.UserDicName);
	free(dicnames.SystemDicName);
	for(int num=0; num<ATASSISTDICMAX; ++num)
	    free(dicnames.AssistDicName[num]);
	ImmReleaseContext(cx->Win,imc);
    }

    bool st = ArUsing(&lst)>0 && ArUsing(&lst)<=bufsize ? Reply6(ch->Major,ch->Minor,ListCount(&lst),ArAdr(&lst),ArUsing(&lst)) : Reply6(ch->Major,ch->Minor,-1,NULL,0);
    ArDelete(&lst);
    return st;
}

/*07 辞書ディレクトリ一覧 [atok]
辞書ディレクトリ(辞書テーブル(dics.dir)を持ったディレクトリ)にある辞書の一覧を取得する．
要求パケット(Type 3)
	i16	コンテクスト番号
	u16	バッファサイズ
応答パケット(Type 6)
	i16	辞書数  エラー時: −1
	s8	辞書ディレクトリ名リスト  '辞書名@...@ 辞書名@@'

選択されている辞書セットの名前を返す。
文字コードは06と同様にする。
 */
bool at_get_dir_list(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    char* str = NULL;
    HIMC imc;

    Req3(ch,&cxn,&bufsize);
    DEBUGLOG(CH_CANNA,"context %hd, buffer size %hd\n",cxn,bufsize);
    CannaContext_t* cx = GetContext(cxn,&imc,__FUNCTION__);
    if(cx != NULL){
	int dn = AT_GetDefaultDicNo(imc);
	if(dn >= 0){
	    uint16_t* u16 = calloc(2,ATDICFILESETNICKNAME_MAX);
	    str = calloc(1,ATDICFILESETNICKNAME_MAX*2);
	    AT_GetDicFileSetNickname(imc,dn,u16);
	    DEBUGLOG(CH_CANNA,"dic number=%d,name='%W'\n",dn,u16);
	    if((cx->Flags & USE_UTF16) == 0){
		U16ToEj(str,NULL,u16,-1);
	    }else{
		U16ToU8(str,NULL,u16,-1);
	    }
	    free(u16);
	}else{
	    INFOLOG(CH_CANNA,"fail AT_GetDefaultDicNo\n");
	}
	ImmReleaseContext(cx->Win,imc);
    }
    int len = str!=NULL ? strlen(str)+2 : 0;
    bool st = str!=NULL && len<=bufsize ? Reply6(ch->Major,ch->Minor,1,str,len) : Reply6(ch->Major,ch->Minor,-1,NULL,0);
    free(str);
    return st;
}

//(C) 2008 thomas
