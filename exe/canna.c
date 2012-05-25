#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <windows.h>
#include <ddk/imm.h>
#include <stdbool.h>
#include "so/wimeapi.h"
#include "io/wimeio.h"
#include "lib/ut.h"
#include "canna.h"
#include "apisup.h"
#include "lib/list.h"
#include "version.h"

int MsgLoopN(int n,...); //wime.c
bool wm_ime_composition(CannaContext_t* cx,char mj);

extern Array InputWins;

/*
  コンテキスト番号は０から。
  !!! Initが返すグローバルコンテキスト番号は０とは限らないが、まずいか？
  接続(fd)ごとに０からの番号を返すようにすべきか？
*/

//01
bool Init(CanHeader* ch UNUSED,int fd)
{
    int len,client_major,client_minor,n;
    uint16_t res[2];
    char *user;

    ImRead(&len,4);
    len = Swap4(len);
    char data[len];
    ImRead(data,len);
    LOG("data='%s', fd=%d\n",data,fd);

    if(FindClient(fd) != NULL){
	//複数回の初期化
	res[0] = res[1] = -1;
    }else{
	n = sscanf(data,"%d.%d",&client_major,&client_minor);
	if((user = strchr(data,':')) != NULL)
	    ++user;
	if(n!=2 || client_major>WIME_CANNA_MAJOR || user==NULL){
	    /* 送られたデータがおかしい
	       メジャーバージョンがあわないときはRETURN_VERSION_ERROR_STATを返すことになっているが、これはかんなソースのIRproto.hにあり、libcannaには含まれていない。これのためだけにかんなソースを持ってくるのも面倒なので、ヘッダファイルに書いておく。
	       マイナーバージョンはとりあえず無視する。
	       ユーザー名がないのは別のエラーにするべきだが面倒なので。
	    */
	    *(uint32_t*)res = Swap4(RETURN_VERSION_ERROR_STAT);
	    MSG("illegal data\n");
	}else{
	    res[0] = Swap2(WIME_CANNA_MINOR);
	    res[1] = Swap2(OpenConnection(fd,user));
	    LOG("context=%hd fd=%d user='%s'\n",Swap2(res[1]),fd,user);
	}
    }
    return ImWrite(res,4);
}

//02
bool Finalize(CanHeader* ch,int fd)
{
    char st = (CloseConnection(fd) ? 0:-1);
    LOG("fd %d ,status %hhd\n",fd,st);
    return Reply2(ch->Major,ch->Minor,st);
}

//03
bool CreateContext(CanHeader* ch,int fd)
{
    int16_t cxn=-1;
    if(FindClient(fd) != NULL)
	OpenCannaContext(fd,&cxn);
    LOG("context number %d,fd %d\n",cxn,fd);
    return Reply5(ch->Major,ch->Minor,cxn);
}

//04
bool DupContext(CanHeader* ch,int fd)
{
    int16_t srcn,dstn=-1;
    CannaContext_t *cxs,*cxd;

    srcn = Req2(ch);
    LOG("fd=%d, context=%hd\n",fd,srcn);
    if(ValidContext(srcn,__FUNCTION__) != NULL){
	DupWinParam params;
	cxd = OpenCannaContext(fd,&dstn);
	cxs = ValidContext(srcn,__FUNCTION__); //cxdを作った後で改めてアドレスを得る
	SetWinParam(cxd->Win,GetWinParam(cxs->Win,&params));
	ArCopy(&cxd->Dics,&cxs->Dics);
	ArCopy(&cxd->DicMode,&cxs->DicMode);

	LOG("%d --> %d\n",(int)srcn,(int)dstn);
    }
    return Reply5(ch->Major,ch->Minor,dstn);
}

//05
bool CloseContext(CanHeader* ch,int fd UNUSED)
{
    char st=-1;
    CannaContext_t* cx;
    int16_t cxn = Req2(ch);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	CloseCannaContext(cx);
	st = 0;
    }
    return Reply2(ch->Major,ch->Minor,st);
}

/*???
  06 GetDictionaryList	辞書テーブルに登録されている辞書の一覧
  07 GetDirectoryList	辞書ディレクトリにある辞書の一覧
  0b GetMountDicList	辞書テーブルに登録されている辞書リスト
  06と0bの説明は同じだ。なんだこれ？
  atok指定時は
  06 使用するように設定している辞書の一覧(デフォルトの辞書セット)
  07 使用可能な辞書の一覧
  0b マウントされている辞書の一覧
  としてみる。
  imm-apiに辞書関連のものはないので、ここらへんの関数はどうしようもない。
*/
//06 [atok]
bool GetDicList(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    Req3(ch,&cxn,&bufsize);
    LOG("context=%hd, bufsize=%hd\n",cxn,bufsize);
    MSG("*** NOT IMPLIMENT ***\n"); 
    MSG("*** I DO NOTHING ***\n");
    return Reply6(ch->Major,ch->Minor,0,NULL,0); //リストなしで正常終了
}

//07 [atok]
bool GetDirList(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    Req3(ch,&cxn,&bufsize);
    LOG("context=%hd, bufsize=%hd\n",cxn,bufsize);
    MSG("*** NOT IMPLIMENT ***\n"); 
    MSG("*** I DO NOTHING ***\n");
    return Reply6(ch->Major,ch->Minor,0,NULL,0);
}

//辞書名を記録するだけ
//08
bool MountDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *name,st=-1;
    CannaContext_t *cx;

    name = Req15(ch,&mode,&cxn);
    VERBOSE(MSG("mode=0x%x context=%hd dic-name='%s'\n",mode,cxn,name);
	    MSG("*** I DO NOTHING ***\n"));
    if((cx = ValidContext(cxn,__FUNCTION__))!=NULL){
	char listterm=0;
	if(cx->Dics.use > 0)
	    -- cx->Dics.use; //リストの終了マークをとる
	ArAddN(&cx->Dics,name,strlen(name)+1);
	ArAdd(&cx->Dics,&listterm);
	ArAdd(&cx->DicMode,&mode);
	st = 0;
    }
    return Reply2(ch->Major,ch->Minor,st);
}

//辞書名を削除するだけ
//09
bool UnmountDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *dicname,st=-1;
    CannaContext_t *cx;
    int dn;

    dicname = Req15(ch,&mode,&cxn);
    VERBOSE(MSG("mode=0x%x, context=%hd, dictionary name='%s'\n",mode,cxn,dicname);
	    MSG("*** I DO NOTHING ***\n"));
    if((cx = ValidContext(cxn,__FUNCTION__))!=NULL){
	if(cx->Dics.use>0 && (dn = ListFind(cx->Dics.adr,dicname))>0){
	    cx->Dics.use -= ListRemove(cx->Dics.adr,dn);
	    if(cx->Dics.use == 1) //リスト終了マークのみになった
		cx->Dics.use = 0;
	    ArRemove(&cx->DicMode,dn);
	    st = 0;
	}else
	    LOG("not found dictionary '%s'\n",dicname);
    }
    return Reply2(ch->Major,ch->Minor,st);
}

/*???
  唐突に優先度が使われるが、これはどういう数値なのか？
  よくわからないので、指定された辞書をリストの先頭に持ってくる。
*/
//0a
bool RemountDic(CanHeader* ch,int fd UNUSED)
{
    char *dicname;
    int32_t pr;
    int16_t cxn;
    CannaContext_t *cx;
    int dn;
    char st=-1;

    dicname = Req15(ch,&pr,&cxn);
    VERBOSE(MSG("context=%hd, priority=%d, dic-name='%s'\n",cxn,pr,dicname);
	    MSG("*** I DO NOTHING ***\n"));
    if((cx = ValidContext(cxn,__FUNCTION__))!=NULL){
	dn = ListFind(cx->Dics.adr,dicname);
	if(dn >= 0){
	    int mode = *(int32_t*)ArElem(&cx->DicMode,dn);
	    ArInsert(ArRemove(&cx->DicMode,dn),0,1,&mode);
	    ListRemove(cx->Dics.adr,dn);
	    ListInsert(cx->Dics.adr,0,dicname);
	    st = 0;
	}else
	    LOG("not mount dictionary\n");
    }
    return Reply2(ch->Major,ch->Minor,st);
}

//0b
bool MountDicList(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    int n=-1,len;
    char* p = NULL;
    CannaContext_t *cx;

    Req3(ch,&cxn,&bufsize);
    LOG("context %hd, buffer size %hd\n",cxn,bufsize);
    if((cx = ValidContext(cxn,__FUNCTION__))!=NULL &&
       (len = ArUsing(&cx->Dics)) <= bufsize)
	n = ListCount(p = ArAdr(&cx->Dics));
    return Reply6(ch->Major,ch->Minor,n,p,len);
}

//0c
bool QueryDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *user,*dic;

    dic = Req19(ch,&mode,&cxn,&user);
    VERBOSE(MSG("context=%hd, mode=0x%x, user='%s', dic='%s'\n",cxn,mode,user,dic);	
	    MSG("*** NOT IMPLIMENT ***\n"));
    return Reply2(ch->Major,ch->Minor,-1);
}


/*
  かんなの品詞コード("#"はつけない)をwinの数値コードにする
  品詞テーブルがないとき0
  ??? IME_REGWORD_STYLE_USER_FIRSTをImmRegisterWordで使うとエラーになった。
  IME_REGWORD_STYLE_USER_FIRST+1以降が確実にあるとしていいのか？
  今のところでは、品詞対応ファイルがなければエラーにしておく。
*/
int canna_hinshi_to_win(const char* can_code)
{
    HinshiCor* hc;
    regex_t reg;
    regmatch_t m;

    if((hc=WimeData.HinshiTab) == NULL){
	MSG("not found hinshi table\n");
	return 0;
    }
    for(; hc->Ccode!=NULL; ++hc){
	if(regcomp(&reg,hc->Ccode,REG_EXTENDED)==0){
	    if(regexec(&reg,can_code,1,&m,0)==0)
		break;
	    regfree(&reg);
	}
    }
    if(hc->Ccode != NULL)
	regfree(&reg);
    else{
	MSG("unknown part code:%s\n",can_code);
	hc=WimeData.HinshiTab; //先頭にあるコードを返す
    }
    return hc->Wcode;
}

//??? -mno-cygwinがあると定義されない?
//char *strtok_r(char*,const char*,char**);
/* gccの-Hで見ると、no-cygwinがあるときのstring.hは/usr/local/include/wine/msvcrt,
   無いときはそれ以外(/usr/include)から読み込んでいる。で、wine-1.1.1の段階で
   msvcrt/string.hにstrtok_rは無い。
   →windowsではstrtok_sみたい。でもwineにはない。
*/
char* strtok_r_(char* s,const char* d,char** p)
{
    if(s == NULL)
	s = *p;
    if(s != NULL){
	s += strspn(s,d);
	if(*s != 0){
	    *p = s+strcspn(s,d);
	    if(**p != 0)
		*((*p)++) = 0;
	}else
	    *p = s = NULL;
    }
    return s;
}
#define strtok_s strtok_r_

//0d RkDefineDic
bool reg_or_unreg_word(CanHeader* ch,BOOL WINAPI (*proc)(HKL,LPCWSTR, DWORD, LPCWSTR))
{
    int16_t cxn;
    char *dicname,*wordrec,*hinshi,*wrp,*tok;
    char *mb[2]/*読みと漢字*/;
    int sty;
    uint16_t *wd[2];
    const char spc[] = " \t";

    dicname = Req12((Req12_t*)ch,&cxn,&wordrec);
    LOG("context=%hd, words='%s', dic-name=%s\n",cxn,wordrec,dicname);

    wrp = wordrec;
    while((mb[0] = strtok_s(wrp,spc,&tok)) != NULL){
	hinshi = strtok_s(NULL,spc,&tok); //品詞コード文字列
	mb[1] = strtok_s(NULL,spc,&tok); //登録する漢字
	if(hinshi==NULL || mb[1]==NULL){
	    MSG("illegal word info.\n");
	    break;
	}
	sty = canna_hinshi_to_win(hinshi+1);
	if(sty == 0){ //品詞テーブルがない
	    mb[0] = ""; //エラーにするために適当なアドレスを入れておく
	    break;
	}
	LOG("reading=[%s] style=0x%x word=[%s]\n",mb[0],sty,mb[1]);

	for(int n=0; n<2; ++n)
	    wd[n] = EjToU16(NULL,mb[n]);
	if(!proc(GetKeyboardLayout(0),wd[0],sty,wd[1])){
	    MSG("fail Imm(Un)RegisterWordW\n");
	    break;
	}
	for(int n=0; n<2; ++n)
	    free(wd[n]);
	wrp = NULL;
    }
    free(wordrec);
    return Reply2(ch->Major,ch->Minor,(mb[0]==NULL?0:-1));
}

//0d RkDefineDic
bool DefineWord(CanHeader* ch,int fd UNUSED)
{
    LOG("\n"); //関数名だけ表示
    return reg_or_unreg_word(ch,ImmRegisterWordW);
}

//0e RkDeleteDic
bool DeleteWord(CanHeader* ch,int fd UNUSED)
{
    LOG("\n"); //関数名だけ表示
    return reg_or_unreg_word(ch,ImmUnregisterWordW);
}

bool set_yomi_str(CannaContext_t* cx,int sentence_mode,int notify_cmd,const char* yomi,int32_t fer_mode)
{
    int r=0;
    HIMC imc = ImmGetContext(cx->Win);

#ifdef SETCONTEXT_FAIL
    SetCurrentImc(imc,TRUE);
#else
    ImmSetOpenStatus(imc,TRUE);
    ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CANCEL,0); //きちんと終了処理をしない場合新しいHWNDでも以前のデータが残っているときがある。
#endif

    ImmSetConversionStatus(imc,CONV_MODE,sentence_mode);

    if((r = (*WimeData.SetRead)(imc,yomi))){
	if((r=ImmNotifyIME(imc,NI_COMPOSITIONSTR,notify_cmd,0))){
	    cx->FerMode = fer_mode;
	    cx->Conv = 0;
	}else
	    MSG("fail ImmNotifyIME()\n");
    }else
	MSG("fail ImmSetCompositionStringA/W()\n");
    ImmReleaseContext(cx->Win,imc);
    return r!=0;
}

//0f RkBgnBun
bool BeginConv(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    bool r=false;
    char *yomi;
    CannaContext_t *cx;

    yomi = Req14(ch,&mode,&cxn);
    LOG("mode 0x%x, context %hd, yomi='%s'\n",mode,cxn,yomi);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	r = set_yomi_str(cx,IME_SMODE_PHRASEPREDICT,CPS_CONVERT,yomi,mode);
    }
    free(yomi);
    return r ? wm_ime_composition(cx,ch->Major) : (cx->Conv=-1,Reply5(ch->Major,ch->Minor,-1));
}

//変換モードで追加される候補の数を返す
//append_fer_cand()も参照
int fer_mode_num(int32_t mode)
{
    int count;
    for(count=0; (mode&RK_XFERMASK)!=0; mode>>=RK_XFERBITS){
	//モード0x0fは除外する。 ???これはなんだろう
	if((mode & RK_XFERMASK) != RK_XFERMASK)
	    ++ count;
    }
    return count;
}

/*
  現在の変換結果のリストを作る。リスト終了のヌル文字を追加する。
  文字コードはcej
  戻り値：文節数
*/
int current_cand_list(int clstart,Array* lst,const CannaContext_t* cx,HIMC imc)
{
    int st=0;
    Array cej;
    char at;

    ArNew(&cej,2,NULL);
    for(; GetClause(imc,cx,GCS_COMPSTR,clstart,clstart,&cej,&at)!=NULL; ++clstart){
	if(at!=ATTR_TARGET_CONVERTED && at!=ATTR_CONVERTED && at!=ATTR_FIXEDCONVERTED)
	    break; //まだ変換されてなければそこで終わる
	ArAddAr(lst,&cej);
	//{Array x;ArNew(&x,1,NULL);Dump1(" %x",ArAdr(lst),ArUsingBytes(lst),&x);printf("***%s\n",ArAdr(&x));}
	++ st; //有効な文節の数を数える
    }
    if(st > 0){
	uint16_t nl=0;
	ArAdd(lst,&nl); //リスト終了マーク
    }
    ArDelete(&cej);
    return st;
}

/*
  begin_convert,resize_pauseの続き
  WM_IME_COMPOSITIONの処理
  Context[cx].Convに注目文節番号

  ??? 1.5.1までは回ってきたメッセージを捕まえて処理していたが、1.6.0では変換処理関数を
  呼んだ後メッセージループに行かずに直接これを呼ぶことにした。atok21では問題なさそうだが、
  ほかのimeではどうだろう？ メッセージが送られるタイミングは変換処理がすべて終わった後として
  かまわないのか？
  特に自動変換の場合、明示的に変換をしているわけではない。キーを送った後すぐにこの関数を呼んでいるが、大丈夫だろうか。文字処理が非同期に行われれば問題が起こりそうな気がする。
  しばらくこれでやってみて、おかしければ元に戻そう。
*/
bool wm_ime_composition(CannaContext_t* cx,char mj)
{
    int st;
    Array candlist;
    bool ret;

    HIMC imc = ImmGetContext(cx->Win);
    VERBOSE(MSG("major code=0x%hhx, target clause number %d\n",mj,cx->Conv);DbgComp(imc,__func__));
    SaveFixedClause(imc,cx); //変換が起こるたびに固定文節情報は上書きされてしまうので保存する。

    //分節毎にc-eucjpに変換しながらコピー
    ArNew(&candlist,2,NULL);
    st = current_cand_list(cx->Conv,&candlist,cx,imc); //有効な文節の数を数える
    if(st > 0)
	st += cx->Conv; //ResizePauseに返す文節数は注目文節以降ではなく全文節数
    VERBOSE(Array a;
	    ArNew(&a,1,NULL);
	    MSG("cl-count=%d, candi-data-size=%d, data=%s\n",st,ArUsingBytes(&candlist),ArAdr(Dump1(" %02x",ArAdr(&candlist),ArUsingBytes(&candlist),&a)));
	    ArDelete(&a);
	);

    ret = Reply7(mj,0,st,ArAdr(&candlist),ArUsing(&candlist));
    cx->Conv = -1;
    ImmReleaseContext(cx->Win,imc);
    ArDelete(&candlist);
    return ret;
}

/*
  変換候補番号を変換リストページ番号とページ内番号にする。
  候補ウィンドウが出ないとき *page=-1
  先頭候補の時、エラーの時-1

  !!! GetCandidacyListのモードで追加されたカタカナ読みなどが選択された時はどうする？
  当然ページ外なのでエラーになる。
*/
int page_index(int cln,Array* candlistpage,int index,int* page)
{
    if(index > 0){ //先頭候補以外
	if(cln >= candlistpage->use){
	    *page = CANDLISTMAX; //この文節で候補一覧は出していない
	}else{
	    CandListPageInfo* pi = ArElem(candlistpage,cln);
	    if(pi->Seq > 0)
		*page = -1; //候補ウィンドウなし
	    else{
		for(*page=0; *page<CANDLISTMAX; ++*page){
		    if(pi->Size[*page] == 0){ //候補リストがない？
			MSG("clause %d:candidate list page %d is none\n",cln,*page);
			index = -1;
			break;
		    }
		    if(index < pi->Size[*page])
			break;
		    index -= pi->Size[*page];
		}
	    }
	}
	if(*page == CANDLISTMAX){
	    MSG("clause %d:candidate page not found\n",cln);
	    index = -1;
	}
    }else{
	LOG("clause %d:first candidate word\n",cln);
	index = -1;
    }
    return index;
}

//最終的な変換候補をimeに反映させる
void update_cand(HIMC imc,const int16_t* candnum,int len,Array* pi,const CannaContext_t* cx)
{
    int cn,page;
    for(int clnum=cx->FixedNum; clnum<len; ++clnum,++candnum){
	if((cn=page_index(clnum,pi,*candnum,&page))>=0){
	    switch(SetTarget(imc,clnum,cx)){
	    case ChangeTargetSuccess:
		if(page >= 0){
		    if(ImmNotifyIME(imc,NI_OPENCANDIDATE,page,0) &&
		       ImmNotifyIME(imc,NI_SELECTCANDIDATESTR,page,cn+WimeData.CandIndexStart)){
			LOG("candidate page %d,index %d\n",page,cn);
		    }else
			MSG("fail ImmNotifyIME\n");
		}else{
		    while(--cn >= 0)
			ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0);
		}
	    case ChangeTargetFixed:
		break;
	    case ChangeTargetFail:
		MSG("fail SetTarget\n");
	    }
	}
    }
}

//10 RkEndBun
bool EndConv(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,clses,*candnums;
    int32_t mode;
    int numlen;
    CannaContext_t *cx;
    char st=-1;

    candnums = Req10((Req10_t*)ch,&cxn,&clses,&mode);
    numlen = (ch->Length-8)/2;
    VERBOSE(Array a;
	    ArNew(&a,1,NULL);
	    MSG("context %hd, clauses %hd, mode %d, candidate list(%d)%s\n",cxn,clses,mode,numlen,ArAdr(Dump2(" %hd",candnums,numlen,&a)));
	    ArDelete(&a);
	);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	HIMC imc = ImmGetContext(cx->Win);
	//??? mode!=1はキャンセルとしていいのか？
	if(mode==1){
	    update_cand(imc,candnums,numlen,&cx->CandInfo,cx);
	    st = ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_COMPLETE,0);
	    if(st)
		VERBOSE(DbgComp(imc,__FUNCTION__));
	}else{
	    ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_REVERT,0);
	    st = ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CANCEL,0);
	}
	if(!st)
	    MSG("fail ImmNotifyIME\n");
	ImmNotifyIME(imc,NI_CLOSECANDIDATE,0,0); //???各文節に必要か？
#ifdef SETCONTEXT_FAIL
	SetCurrentImc(imc,FALSE);
#else
	ImmSetOpenStatus(imc,FALSE);
#endif
	ImmReleaseContext(cx->Win,imc);
	ResetContext(cx);
	st = 0;
    }
    return Reply2(ch->Major,ch->Minor,st);
}

/*
  モードにしたがってyomi(cej,全角ひらがな)を変換して候補リストに追加する
  追加した数を返す
  ??? モードが0xf(たぶんRK_CTRLHENKAN)なのは何だ？
*/
int append_fer_cand(int mode,Array* lb,uint16_t* yomi)
{
    Array fer;
    int count=0,len;

    ArNew(&fer,2,NULL);
    len = WcLen(yomi)+1; //ヌル文字も含める
    for(; (mode&RK_XFERMASK)!=0; mode>>=RK_XFERBITS){
	switch(mode & RK_XFERMASK){
	case RK_HFER: //半角文字
	    LOG("Hankaku\n");
	    ZenToHan(ArAlloc(&fer,len*2+1),(char*)yomi); //濁点のために２文字になるかもしれない
	    ToWc(ArAdr(&fer),ArAdr(&fer));
	    ArSetUsing(&fer,WcLen(ArAdr(&fer))+1);
	    ArAddAr(lb,&fer);
	    break;
	case RK_KFER: //カタカナ
	    LOG("Katakana\n");
	    HiraToKata(ArAlloc(&fer,len),(char*)yomi,-1);
	    ToWc(ArAdr(&fer),ArAdr(&fer));
	    ArSetUsing(&fer,WcLen(ArAdr(&fer))+1);
	    ArAddAr(lb,&fer);
	    break;
	case RK_XFER: //ひらがな ??? RK_ZFERとの違いは？
	    LOG("Hiragana\n");
	    ArAddN(lb,yomi,len);
	    break;
	case RK_ZFER: //全角文字
	    LOG("Zenkaku\n");
	    ArAddN(lb,yomi,len);
	}
	++count;
    }
    ArDelete(&fer);
    return count;
}

/*???
  atok2008では変換候補は語幹だけが得られる。かんなでは送りがなも含めて必要。
  windowsでの候補リストウィンドウでは送りがなは淡色で表示されている。
  この送りがなの取得方法がわからない。imm apiには無さそうだが。
  とりあえず、候補を１つずつ選択し、そのときの変換文字列を取り出して
  かんなクライアントに渡すことにする。
  [wine1.1.22]vje-dではImmNotifyIME()を呼んでもimcが更新されない。よくわからないので
  vjeの場合はGetCandidateA()を使うことにする。
  atokを自動判別してこの関数を使うのならat.cも有効にするべきか？
*/
void GetCandidateAtok(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb)
{
    Array cej;
    ArNew(&cej,2,NULL);
    for(unsigned cannum=0; cannum<cb->dwCount; ++cannum){
	if(!ImmNotifyIME(imc,NI_SELECTCANDIDATESTR,listnum,cannum+WimeData.CandIndexStart)){
	    MSG("fail ImmNotifyIME(NI_SELECTCANDIDATESTR)\n");
	    break;
	}
	ArAddAr(euclist,GetClause(imc,cx,GCS_COMPSTR,clnum,clnum,&cej,NULL));
    }
    ArDelete(&cej);
}

void GetCandidateA(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb)
{
    char *sj;
    Array cej;

    ArNew(&cej,2,NULL);
    for(unsigned cannum=0; cannum<cb->dwCount; ++cannum){
	sj = (char*)cb + cb->dwOffset[cannum];
	SjToEj(ArAlloc(&cej,strlen(sj)+1),sj,-1);
	ToWc(ArAdr(&cej),ArAdr(&cej));
	ArAddN(euclist,ArAdr(&cej),WcLen(ArAdr(&cej))+1);
    }
    ArDelete(&cej);
}	

void GetCandidateW(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb)
{
    uint16_t *u16;
    Array cej;

    ArNew(&cej,2,NULL);
    for(unsigned cannum=0; cannum<cb->dwCount; ++cannum){
	u16 = (uint16_t*)((char*)cb + cb->dwOffset[cannum]);
	ArAlloc(&cej,WcLen(u16)+1);
	U16ToCej(ArAdr(&cej),u16,-1);
	ArAddAr(euclist,&cej);
    }
    ArDelete(&cej);
}	

/*
  変換候補をc-eucjpにしてリストに追加する。リスト終了マークはつかない。
  euclistのblocksize:2(wchar)
  リストの数を返す。エラーの時-1
*/
int lookup_cand_win(HIMC imc,Array* euclist,CandListPageInfo* pi,int clnum,const CannaContext_t* cx)
{
    Array candpage;
    int n,cand_count,listnum;
    CANDIDATELIST *cb;

    ArNew(&candpage,1,NULL);
    cand_count = listnum = 0;
    do{
	//ImmGetCandidateListはA/Wがなくても問題なさそう。
	if((n = ImmGetCandidateList(imc,listnum,NULL,0)) == 0){
	    LOG("page %d has no candidate list\n",listnum);
	    break;
	}
	LOG("ImmGetCandidateList:page %d size %d\n",listnum,n);
	ArAlloc(&candpage,n);
	ImmGetCandidateList(imc,listnum,ArAdr(&candpage),ArUsingBytes(&candpage));

	//c-eucjpに変換しながらリストバッファに追加する
	cb = ArAdr(&candpage);
	cand_count += cb->dwCount;
	pi->Size[listnum] = cb->dwCount;
	(*WimeData.GetCandidate)(imc,cx,euclist,clnum,listnum,cb);
    }while(++listnum<CANDLISTMAX && ImmNotifyIME(imc,NI_CHANGECANDIDATELIST,listnum,0));

#if 1
    //第１候補に戻す
    if(listnum > 0)
	ImmNotifyIME(imc,NI_CHANGECANDIDATELIST,0,0);
    if(cand_count > 0)
	ImmNotifyIME(imc,NI_SELECTCANDIDATESTR,0,WimeData.CandIndexStart);
#else
    ImmNotifyIME(imc,NI_CLOSECANDIDATE,0,0);
    ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0);
    //??? 未確定に戻ってしまうので、再度変換する。なんで？
#endif

    ArDelete(&candpage);
    return cand_count;
}

//[3.4.4,r9] 現在選択されている候補の番号(0から)を返す。
int cand_index(HIMC imc)
{
    CANDIDATELIST* cb;
    Array candpage;
    int sz,idx=-1;

    ArNew(&candpage,1,NULL);
    sz = ImmGetCandidateList(imc,0,NULL,0);
    if(sz != 0){
	cb = ArAlloc(&candpage,sz);
	ImmGetCandidateList(imc,0,cb,ArUsingBytes(&candpage));
	idx=cb->dwSelection;
    }
    ArDelete(&candpage);
    return idx;
}

/*
  変換候補をc-eucjpにしてリストに追加する。リスト終了マークはつかない。
  euclistのblocksize:2(wchar)
  リストの数を返す。エラーの時-1
*/
int make_cand_list(HIMC imc,Array* euclist,CandListPageInfo* pi,int clnum,const CannaContext_t* cx)
{
    int count=0;
    Array cej;
    bool open_cand_win;

    /*!!!
      候補数≦"候補ウィンドウ表示までに必要な変換回数"のとき、ImmGetCandidateListは０を返す。ImmNotifyIMEでNI_OPENCANDIDATEを指定しても候補ウィンドウは表示されない。なんともならないので、imcは１回目の変換状態であるとして、未変換状態になるまでImmNotifyIMEで変換しそのたびに変換結果を記録する。
      この方法で変換候補リストを作った場合、候補へのランダムアクセスができない。２０番目の候補で確定したら再度２０回変換しなければならない。なので、変換したらメッセージを調べ、WM_IME_NOTIFY(IMN_OPENCANDIDATE)が来たら処理をやめてこれまでと同じやり方で変換候補を取得する。
      全ての文節で最終候補を選択する、という状況はまず無いだろうし、この方法だけにすれば処理は簡単だし(これまでの方法でも結局全候補を選択している)、CandListPageInfoも必要なくなるのだが。どうするか？
      uimは何かするたびに全文節の候補を調べるんだった。めちゃくちゃ遅くなるな。だめか。
      [3.4.4,r9]ImmNotifyIMEで変換していって一周したとき属性が未変換に戻らないときがある(send_keyで変換しているときか?)。現在の候補の番号が0に戻ったどうかを調べることにする。
    */
    ArNew(&cej,2,NULL);
    do{
	ArAddAr(euclist,GetClause(imc,cx,GCS_COMPSTR,clnum,clnum,&cej,NULL));
	++count;
	ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0);
	open_cand_win = (MsgLoopN(1,WM_IME_NOTIFY,IMN_OPENCANDIDATE,NULL)==0);
    }while(!(open_cand_win || GetAttr(imc,clnum,cx)==ATTR_TARGET_NOTCONVERTED || (count>1&&cand_index(imc)==0)));

    if(open_cand_win){
	//リセットしてやり直し
	LOG("retry call lookup_cand_win()\n");
	count = lookup_cand_win(imc,ArClear(euclist),pi,clnum,cx);
    }else{
	//１回目の変換状態に戻す
	ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0);
	pi->Seq = count;
    }
    ArDelete(&cej);
    return count;
}

void dump_cand_list(int num,const uint16_t* ws)
{
    Array lb;
    char *wd;

    ArNew(&lb,1,NULL);
    for(; *ws!=0; ws+=WcLen(ws)+1){
	ArPrint(&lb,"[%s]",wd=ToMb(ws));
	free(wd);
    }
    MSG("list=%d %s\n",num,ArAdr(&lb));
    ArDelete(&lb);
}

//11
//返される候補数は (変換候補の数+読みの数)
bool GetCandiList(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,cls_num,cand_count=-1;
    uint16_t bufsize,nul=0;
    HIMC imc;
    Array euclist,cej;
    CannaContext_t *cx;
    bool ret;

    Req6(ch,&cxn,&cls_num,&bufsize);
    LOG("context %d,clause-number %d,buffer size=%u\n",cxn,cls_num,bufsize);
    ArNew(&euclist,2,NULL);
    if((cx=ValidContext(cxn,__FUNCTION__)) != NULL){
	imc = ImmGetContext(cx->Win);
	ArNew(&cej,2,NULL);
	switch(SetTarget(imc,cls_num,cx)){ //注目文節を変更
	case ChangeTargetSuccess:
	    if(cls_num < ArUsing(&cx->CandInfo))
		//念のため以前の情報は消しておく
		memset(ArElem(&cx->CandInfo,cls_num),0,sizeof(CandListPageInfo));
	    else
		ArAlloc(&cx->CandInfo,cls_num+1); //この文節までは確保する

	    /*[r11]send_keyで変換されているときはすでに何番目かの候補になっているので、最初の候補に戻す。*/
	    if(cx->Flags & CATCH_OPEN_CAND){
		cx->Flags &= ~CATCH_OPEN_CAND;
		ImmNotifyIME(imc,NI_SELECTCANDIDATESTR,0,WimeData.CandIndexStart);
	    }

	    cand_count = make_cand_list(imc,&euclist,ArElem(&cx->CandInfo,cls_num),cls_num,cx);
	    if(cand_count >= 0){
#if 0
		//[1.8.5]cand_countが0になることは無いと思う
                if(cand_count == 0){
                    /* 変換結果以外に候補がなければ変換結果を入れる */
		    ArAddAr(&euclist,GetClause(imc,cx,GCS_COMPSTR,cls_num,cls_num,&cej,NULL));
                    ++cand_count;
                }
#endif
		//読みを追加
		GetClause(imc,cx,GCS_COMPREADSTR,cls_num,cls_num,&cej,NULL);
		cand_count += append_fer_cand(cx->FerMode,&euclist,ArAdr(&cej)); //モードにしたがって候補リストに追加する
		ArAddAr(&euclist,&cej); //読みを追加
		ArAdd(&euclist,&nul);	//リスト終了を示すヌル文字

		VERBOSE(dump_cand_list(cand_count,ArAdr(&euclist)));

		if(ArUsingBytes(&euclist) > bufsize){
		    MSG("bufsize too small,need %d\n",ArUsingBytes(&euclist));
		    ArClear(&euclist);
		    cand_count = -1;
		    //???バッファアドレスを渡されたわけでもないのにバッファサイズを確認することに意味あるのか?
		}
	    }
	    break;
	case ChangeTargetFixed:
	    LOG("this clause is fixed\n");
	    break;
	case ChangeTargetFail:
	    MSG("fail SetTarget\n");
	}
	ArDelete(&cej);
    }
    ret = Reply7(ch->Major,ch->Minor,cand_count,ArAdr(&euclist),ArUsing(&euclist));
    ArDelete(&euclist);
    ImmReleaseContext(cx->Win,imc);
    return ret;
}

//12
bool GetYomi(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,cln;
    uint16_t bufsize;
    HIMC imc;
    bool st=true;
    CannaContext_t *cx;
    Array cej;

    ArNew(&cej,2,NULL);
    Req6(ch,&cxn,&cln,&bufsize);
    LOG("context=%hd, clause=%hd, bufsize=%hd\n",cxn,cln,bufsize);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	imc = ImmGetContext(cx->Win);
	if(ArUsingBytes(GetClause(imc,cx,GCS_COMPREADSTR,cln,cln,&cej,NULL)) <= bufsize){
	    LOG("yomi:[%s]\n",ArAdr(&cej));
	}else{
	    MSG("buffer too small\n");
	    st = false;
	}
    }
    st = Reply7(ch->Major,ch->Minor,(st ? EjLen(ArAdr(&cej)):-1),ArAdr(&cej),ArUsing(&cej));
    ArDelete(&cej);
    ImmReleaseContext(cx->Win,imc);
    return st;
}

//シフトキーの状態をセットする
void set_state(unsigned char state,uint16_t vk)
{
    if(vk & 0xff00){
	unsigned char keytab[256];
	GetKeyboardState(keytab);
	if((vk & VKMODKEY(WINMODKEY_SHIFT)) != 0)
	    keytab[VK_SHIFT]=state;
	if((vk & VKMODKEY(WINMODKEY_CTRL)) != 0)
	    keytab[VK_CONTROL]=state;
	if((vk & VKMODKEY(WINMODKEY_ALT)) != 0)
	    keytab[VK_MENU]=state;
	if((vk & VKMODKEY(WINMODKEY_LOCK)) != 0)
	    keytab[VK_CAPITAL]=state; //??? VK_OEM_ATTNだろうか？
	SetKeyboardState(keytab);
    }
}

#define KEYUP (1<<31)

/*??? '>','?'などシフトキーが必要な場合、SetKeyboardState()で無理矢理キー状態を変更している。 SendInput()などを使うべきか？ そもそも根本的にアプローチが間違ってる気がする。
  vk=仮想キーコード。上８ビットにはシフト状態をセットする。
*/
bool proc_key_vk(uint16_t vk,HWND wh,HKL kl)
{
    bool st = false;
    uint32_t vkch = vk&0xff; //仮想キーコード(シフトなし)
    uint32_t sc = MapVirtualKeyEx(vkch,0,kl); //仮想キーコード→スキャンコード
    uint32_t pk = (sc<<16)|1; //ImmProcessKey用のスキャンコード
    UINT msg = WM_NULL;

#if 0
    //!!! wm_wime_set_focus()でSetFocus()するようにした。
    if(GetFocus() != wh) //???調べずにいきなりSetFocusはだめか？
	SetFocus(wh);
#endif
    set_state(0xff,vk);
    if(ImmProcessKey(wh,kl,vkch,pk,0))
	msg = WM_KEYDOWN;
    else if(ImmProcessKey(wh,kl,vkch,pk|KEYUP,0))
	msg = WM_KEYUP;
    if(msg != WM_NULL){
	if(ImmTranslateMessage(wh,msg,VK_PROCESSKEY,pk))
	    st = true;
	else
	    LOG("fail ImmTranslateMessage(), vkey=0x%hx, scancode=0x%x\n",vk,sc);
    }else{
	LOG("fail ImmProcessKey(),vkey=0x%hx, scancode=0x%x\n",vk,sc);
    }
    set_state(0,vk);
    return st;
}
bool proc_key_ch(char ch,HWND wh,HKL kl)
{
    return proc_key_vk(VkKeyScanEx(ch,kl),wh,kl); //文字コード→仮想キーコード
}

/*
  eucjpの全角ひらがなをローマ字にしてHWNDに送る
*/
bool send_roman(const char* yomi,HWND wh,HKL kl)
{
    bool st=true;
    char roman[strlen(yomi)*3+1];
    for(char* rp=Zen2Roman(roman,yomi); *rp!=0; ++rp){
	if(!proc_key_ch(*rp,wh,kl)){
	    st = false;
	    break;
	}
    }
    return st;
}

//13
bool SubstYomi(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t beg,end,len;
    char *yomi;
    CannaContext_t* cx;
    bool st;

    yomi = Req4(ch,&cxn,&beg,&end,&len);
    VERBOSE(Array a;
	    ArNew(&a,1,NULL);
	    MSG("context=%hd, begin=%hd, end=%hd, length=%hd, yomi=%s\n",cxn,beg,end,len,yomi);
	    MSG("yomi dump:%s\n",ArAdr(Dump1(" %02x",yomi,strlen(yomi),&a)));
	    ArDelete(&a);
	);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	HKL kl = GetKeyboardLayout(0);
	if(len==0 && beg==end){
	    /*??? スペース(変換キー)を押すと長さ０、文字列なしで呼び出される。
	      これに対し文節数０を返すと、FlushYomiがくるようだ。
	      ??? 1.7.0:仮変換されているときにbsなどで入力を修正しても
	      呼び出される(beg<end)。現在の変換状態を返すのか？
	      ??? 1.8.1:変換キーの時は(beg==end)>0で,文節数０を返すとFlushYomi,
	      入力途中で、ローマ字の子音の時にbsで修正すると(beg==end)==0で現在の
	      変換状態を返す(こうしないと入力文字列が文字化けする）
	    */
	    int cln=0;
	    Array lst;
	    ArNew(&lst,2,NULL);
	    if(beg == 0){
		HIMC imc = ImmGetContext(cx->Win);
		cln = current_cand_list(0,&lst,cx,imc);
		ImmReleaseContext(cx->Win,imc);
	    }
	    st = Reply7(ch->Major,ch->Minor,cln,ArAdr(&lst),ArUsing(&lst));
	    ArDelete(&lst);
	}else{
	    while(++beg <= end)
		proc_key_ch('\b',cx->Win,kl); /* 読み文字列の修正 */
	    if(send_roman(yomi,cx->Win,kl)){ /* 最後尾に追加 */
		cx->Conv = 0;
		st = wm_ime_composition(cx,ch->Major);
	    }else
		st = Reply5(ch->Major,ch->Minor,-1); //エラー
	}
    }else
	st = Reply5(ch->Major,ch->Minor,-1);
    free(yomi);
    return st;
}

//14
bool StoreYomi(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,cl;
    char *yomi;
    CannaContext_t* cx;
    bool st;

    yomi = Req11(ch,&cxn,&cl);
    LOG("context=%hd, clause=%hd, yomi='%s'\n",cxn,cl,yomi);

    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	Array scs[CS_MAX];
	HIMC imc = ImmGetContext(cx->Win);
	CompNew(scs);

	StoreComp(scs,imc,0,cl,EN_ALL);
	LoadComp(scs,imc);
	send_roman(yomi,cx->Win,GetKeyboardLayout(0));
	if(cx->CandInfo.use > cl+1)
	    cx->CandInfo.use = cl+1; //候補情報はclまで

	CompDelete(scs);
	ImmReleaseContext(cx->Win,imc);
	
	cx->Conv = 0;
	st = wm_ime_composition(cx,ch->Major);
    }else
	st = Reply5(ch->Major,ch->Minor,-1);
    free(yomi);
    return st;
}

/*???
  どういうときに呼ばれるんだろう？ とりあえず言われたとおりにしてみる。
*/
//15
bool StoreRange(CanHeader* ch,int fd)
{
    int16_t cxn,cl;
    char *yomi;
    CannaContext_t *cx,*tmpcx=NULL;
    Array cs[CS_MAX],cand;
    HIMC imc,tmpimc;
    bool st;
    int cl_r;

    yomi = Req11(ch,&cxn,&cl);
    LOG("context=%hd, clause=%hd, yomi='%s'\n",cxn,cl,yomi);

    ArNew(&cand,2,NULL);
    if(ValidContext(cxn,__FUNCTION__) != NULL){
	tmpcx = OpenCannaContext(fd,NULL);
	cx = ArElem(&Context,cxn);
	if(cl >= cx->FixedNum && set_yomi_str(tmpcx,IME_SMODE_SINGLECONVERT,CPS_CONVERT,yomi,0)){
	    imc = ImmGetContext(cx->Win);
	    tmpimc = ImmGetContext(tmpcx->Win);
	    CompNew(cs);

	    SetTarget(imc,cl,cx);//imcとtmpimcで選択文節が重ならないようにする
	    cl_r = cl - cx->FixedNum;
	    StoreComp(cs,imc,0,cl_r,EN_ALL);	//cl-1まで
	    StoreComp(cs,tmpimc,0,-1,EN_ALL);	//単文節変換した新しいcl
	    StoreComp(cs,imc,cl_r+1,-1,EN_ALL);	//cl+1以降
	    if(LoadComp(cs,imc)){
		if(ArUsing(&cx->CandInfo) > cl_r+1)
		    ArSetUsing(&cx->CandInfo,cl_r+1); //候補情報はclまで
		GetClause(imc,cx,GCS_COMPSTR,cl,cl,&cand,NULL);
		VERBOSE(DbgComp(imc,__FUNCTION__));
	    }else
		MSG("fail load_comp()\n");

	    CompDelete(cs);
	    ImmReleaseContext(cx->Win,imc);
	    ImmReleaseContext(tmpcx->Win,tmpimc);
	}
	CloseCannaContext(tmpcx);
    }

    st = Reply3(ch->Major,ch->Minor,(ArUsing(&cand)>0?0:-1),ArAdr(&cand),ArUsing(&cand));
    ArDelete(&cand);
    free(yomi);
    return st;
}

//16
bool GetLastYomi(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,st=-1;
    uint16_t bufsize;
    Array cej;
    CannaContext_t* cx;
    bool ret;

    ArNew(&cej,2,NULL);
    Req3(ch,&cxn,&bufsize);
    LOG("context=%hd, bufsize=%hd\n",cxn,bufsize);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	int n;
	HIMC imc = ImmGetContext(cx->Win);
	if((n = GetAttrCl(imc,ATTR_INPUT,cx)) >= 0){
	    GetClause(imc,cx,GCS_COMPSTR,n,n,&cej,NULL);
	    st = ArUsing(&cej)-1;
	    LOG("cl-num=%d, string='%s'\n",n,(char*)ArAdr(&cej));
	}else{
	    //??? 未決文節がないときはエラーなのか？ とりあえず０で正常終了する。
	    st = 0;
	    LOG("noting\n");
	}
	ImmReleaseContext(cx->Win,imc);
    }
    ret = Reply7(ch->Major,ch->Minor,st,ArAdr(&cej),ArUsing(&cej));
    ArDelete(&cej);
    return ret;
}

//17
bool FlushYomi(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,cl,*cand;
    int32_t mode,n;
    bool st=false;
    CannaContext_t* cx;

    cand = Req10((Req10_t*)ch,&cxn,&cl,&mode);
    VERBOSE(Array a;
	    ArNew(&a,1,NULL);
	    MSG("context=%hd, clause=%hd, mode=%d, candidate=%s\n",cxn,cl,mode,ArAdr(Dump2("%hd ",cand,(ch->Length-8)/2,&a)));
	    ArDelete(&a);
	);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	HIMC imc = ImmGetContext(cx->Win);
	Array scs[CS_MAX],*at=scs+CS_STRATTR;
	char a;

	/*変換していない文節があるか？*/
	CompNew(scs);
	StoreComp(scs,imc,0,-1,EN_STRATTR);
	for(n=0; n<ArUsing(at); ++n){
	    a = *(char*)ArElem(at,n);
	    if(a==ATTR_INPUT || a==ATTR_TARGET_NOTCONVERTED)
		break;
	}
	cx->Conv = 0;
	cx->FerMode = mode;
	if(n < ArUsing(at)){
	    //未返還文節がある
	    if(ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0)){
		st = true; //あとはWM_IME_COMPOSITIONメッセージの処理に任せる
	    }else
		MSG("fail ImmNotifyIME\n");
	}else{
	    //すでに全部変換済み
	    LOG("already convert\n");
	    st = true; //??? また変換？
	}
	CompDelete(scs);
	ImmReleaseContext(cx->Win,imc);
    }
    return st ? wm_ime_composition(cx,ch->Major) : Reply5(ch->Major,ch->Minor,-1);
}

/*???
  StoreRangeと同じくいつ使われるかわからない。仮変換文節数が一定以上になったら呼ばれるのか?
  応答パケットの文節数は全文節数でいいのか？カレント文節以降の残りの文節数か？
*/
//18
bool RemoveYomi(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,cl,*cand;
    int32_t mode;
    CannaContext_t *cx;
    bool st=false;
    char clall=-1; //全文節数
    HIMC imc;

    cand = Req10((Req10_t*)ch,&cxn,&cl,&mode);
    VERBOSE(Array a;
	    ArNew(&a,1,NULL);
	    MSG("context=%hd, clause=%hd, mode=%d, candidate=%s\n",cxn,cl,mode,ArAdr(Dump2("%hd ",cand,(ch->Length-8)/2,&a)));
	    ArDelete(&a);
	);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	st = true;
	imc = ImmGetContext(cx->Win);
	if(mode && cl>=cx->FixedNum)
	    update_cand(imc,cand,cl-cx->FixedNum+1,&cx->CandInfo,cx);
	if(cl < cx->FixedNum-1){
	    //固定文節のみ削除する(固定文節は１つ以上残る)
	    Array *fx[]={&cx->FixedStr,&cx->FixedYomi};
	    uint16_t *p;
	    for(int n=0; n<2; ++n){
		p = StrListNthWc(fx[n]->adr,cx->FixedNum,cl+1);
		fx[n]->use -= p - (uint16_t*)(fx[n]->adr); //残りの長さ
		memcpy(fx[n]->adr,p,fx[n]->use*fx[n]->blocksize);
	    }
	    cx->FixedNum -= cl+1;
	    LOG("fixed clauses %d\n",cx->FixedNum);
	}else{
	    //固定文節は全部削除。未固定文節は削除されるかもしれない。
	    if(cl >= cx->FixedNum){
		Array scs[CS_MAX];
		CompNew(scs);
		SetTarget(imc,cl+1,cx);
		StoreComp(scs,imc,cl+1-cx->FixedNum,-1,EN_ALL);
		if((st = LoadComp(scs,imc)))
		    VERBOSE(DbgComp(imc,__FUNCTION__));
		else
		    MSG("fail LoadComp()\n");
		CompDelete(scs);
	    }
	    cx->FixedNum = 0;
	    ArDelete(&cx->FixedStr);
	    ArDelete(&cx->FixedYomi);
	}
	if(st){
	    clall = cx->FixedNum+ClauseLen(imc,cx);

	    /* clまでの候補情報があれば消して前に詰める */
	    int len = ArUsing(&cx->CandInfo) - cl - 1;
	    if(len >= 0){
		memcpy(cx->CandInfo.adr,ArElem(&cx->CandInfo,cl+1),len*cx->CandInfo.blocksize);
		cx->CandInfo.use = len;
		LOG("new candinfo length=%d\n",len);
	    }
	}
	ImmReleaseContext(cx->Win,imc);
    }
    //??? とりあえず残りの文節数を返してみる
    return Reply2(ch->Major,ch->Minor,clall);
}

//19
bool GetSimpleKanji(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    char *dic,*yomi;
    uint16_t yomi_len,cand_bufsize,hinshi_bufsize;

    dic = Req13((Req13_t*)ch,&cxn,&yomi,&yomi_len,&cand_bufsize,&hinshi_bufsize);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, dic=%s, yomi='%s', yomi-len=%hd, cand-bufsize=%hd, hinshi-bufsize=%hd\n",cxn,dic,yomi,yomi_len,cand_bufsize,hinshi_bufsize);
    free(yomi);
    return Reply5(ch->Major,ch->Minor,-1);
}

//1a
bool ResizePause(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,clnum,act;
    int st=0,*cls,*rcls;
    HIMC imc=NULL;
    char dum[5];
    CannaContext_t *cx;
    Array yomi,scs[CS_MAX],*cls_a=scs+CS_STRCL,*rcls_a=scs+CS_READCL;

    CompNew(scs);
    ArNew(&yomi,2,NULL);
    Req7(ch,&cxn,&clnum,&act);
    LOG("context=%hd, clause=%hd, action=%hd\n",cxn,clnum,act);
    if((cx = ValidContext(cxn,__FUNCTION__))!=NULL && clnum>=cx->FixedNum){
	imc = ImmGetContext(cx->Win);
	StoreComp(scs,imc,0,-1,EN_STRCL|EN_READCL);
	++ cls_a->use; //最後の要素（全文字数）を使う
	++ rcls_a->use;
	cls = cls_a->adr;
	rcls = rcls_a->adr;
	switch(SetTarget(imc,clnum,cx)){
	case ChangeTargetSuccess:
	    switch(act){
	    case -1: //文節を伸ばす → 右の文節の開始位置を１文字後ろへ
		cls[clnum+1] += WimeData.CharSize;
		GetClause(imc,cx,GCS_COMPREADSTR,clnum+1,clnum+1,&yomi,NULL);
		rcls[clnum+1] += EjZen2Han(dum,ArAdr(&yomi));
		st = 1;
		break;
	    case -2: //文節を縮める → 右の文節の開始位置を１文字前へ
		cls[clnum+1] -= WimeData.CharSize;
		/*
		  右の文節の調整
		  この文節の最後尾に濁音があれば２文字縮めなければならない("カ゛"とか)
		*/
		GetClause(imc,cx,GCS_COMPREADSTR,clnum,clnum,&yomi,NULL);
		rcls[clnum+1] -= EjZen2Han(dum,ForwardEj(ArAdr(&yomi),EjLen(ArAdr(&yomi))-1));
		st = 1;
		break;
	    default:
		//???正数が指定されたら文節の文字数をその数にする、ということだろうか?
		MSG("illegal action\n");
	    }
	    break;
	case ChangeTargetFixed:
	    LOG("this clause is fixed\n");
	    break;
	case ChangeTargetFail:
	    MSG("fail SetTarget()\n");
	}
    }
    if(st){
	st = (*WimeData.SetCompStr)(imc,SCS_CHANGECLAUSE,cls,ArUsingBytes(cls_a),rcls,ArUsingBytes(rcls_a));
	if(st){ //この文節と右の文節が影響を受ける→CandInfoを０に戻す
	    if(clnum < ArUsing(&cx->CandInfo)){
		int n=1;
		if(clnum+1 < ArUsing(&cx->CandInfo))
		    ++n;
		memset(ArElem(&cx->CandInfo,clnum),0,n*sizeof(CandListPageInfo));
	    }
	}else{
	    VERBOSE(DbgComp(imc,"fail ImmSetCompositionString"));
	}
    }
    if(imc!=NULL)
	ImmReleaseContext(cx->Win,imc);
    CompDelete(scs);
    ArDelete(&yomi);
    return st ? (cx->Conv=clnum,wm_ime_composition(cx,ch->Major)) : Reply5(ch->Major,ch->Minor,-1);
}

//1b
bool GetHinshi(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,cl,cand;
    uint16_t bufsize;

    Req8(ch,&cxn,&cl,&cand,&bufsize);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, clause=%hd, candidate=%hd, bufsize=%hu\n",cxn,cl,cand,bufsize);
    return Reply2(ch->Major,ch->Minor,-1);
}

//1c
bool GetLex(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,cl,cand,bufsize;

    Req9(ch,&cxn,&cl,&cand,&bufsize);
    MSG("*** NOT IMPLIMENT ***\n"); 
    LOG("context=%hd, clause=%hd, candidate=%hd, bufsize=%hd\n",cxn,cl,cand,bufsize);
    return Reply5(ch->Major,ch->Minor,-1);
}

//1d RkGetStat
bool GetStatus(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,clnum,cand;
    CannaContext_t *cx;
    int datalen=0;
    char st=-1;

    Req7(ch,&cxn,&clnum,&cand);
    LOG("context=%hd, clause number=%hd, candidate number=%hd\n",cxn,clnum,cand);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	Array e,dummy_el;
	HIMC imc = ImmGetContext(cx->Win);
	ArNew(&e,1,NULL);
	ArNew(&dummy_el,2,NULL);
	cx->RkSt.bunnum = clnum;
	cx->RkSt.candnum = cand;

	/* [1.8.4]uimでは候補リストを出す前にこのapiが呼ばれるらしいので、
	   候補数だけ調べる */
	CandListPageInfo *pi = ArElem(&cx->CandInfo,clnum);
	int cand_count = pi->Seq;
	if(clnum>=ArUsing(&cx->CandInfo) || (pi->Seq==0 && pi->Size[0]==0)){
	    /* この文節で候補リストを出していないとき。
	       候補がなかったときもSize[0]は0になる。候補リストを調べたかどうかが
	       わからないので、このときはあきらめて再度調べる。このためにフラグを追加
	       するのもうっとうしいし。*/
	    ArAlloc(&cx->CandInfo,clnum+1);
	    pi = ArElem(&cx->CandInfo,clnum);
	    switch(SetTarget(imc,clnum,cx)){
	    case ChangeTargetSuccess:
		cand_count = make_cand_list(imc,&dummy_el,pi,clnum,cx);
		break;
	    case ChangeTargetFixed:
		LOG("this clause is fixed\n");
		break;
	    case ChangeTargetFail:
		MSG("fail SetTarget\n");
	    }
	}
	cx->RkSt.diccand = cand_count; //pi->Seq; //[r12]cand_countが反映されていなかった?
	for(int n=0; n<CANDLISTMAX; ++n)
	    cx->RkSt.diccand += pi->Size[n];
	/*[1.8.5]候補が無くてもpi->Seqは1になる(はず)
        if(cand_count == 0)     // 変換結果以外に候補がなければ、変換結果を足す
            cx->RkSt.diccand++;
	*/
	cx->RkSt.maxcand = cx->RkSt.diccand + fer_mode_num(cx->FerMode);
	// ylen,klenはバイト数ではなく文字数！
	cx->RkSt.ylen = EjLen(ArAdr(GetClause(imc,cx,GCS_COMPREADSTR,clnum,clnum,&e,NULL)));
	cx->RkSt.klen = EjLen(ArAdr(GetClause(imc,cx,GCS_COMPSTR,clnum,clnum,&e,NULL)));
	cx->RkSt.tlen = 1; //??? これは何だろう？
	datalen = sizeof(RkStat)/4;
	st = 0;
	ArDelete(&e);
	ArDelete(&dummy_el);
	ImmReleaseContext(cx->Win,imc);

	LOG("bunnum=%d, candnum=%d, maxcand=%d, diccand=%d, ylen=%d, klen=%d, tlen=%d\n",cx->RkSt.bunnum,cx->RkSt.candnum,cx->RkSt.maxcand,cx->RkSt.diccand,cx->RkSt.ylen,cx->RkSt.klen,cx->RkSt.tlen);
    }
    return Reply4(ch->Major,ch->Minor,st,(int32_t*)(&cx->RkSt),datalen);
}

//1e
bool SetLocale(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *loc;

    loc = Req15(ch,&mode,&cxn);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, mode=%d, locale=%s\n",cxn,mode,loc);
    return Reply2(ch->Major,ch->Minor,-1);
}

/*???
  どう実装しよう？ IME_SMODE_AUTOMATICにして読み仮名をセットしても何も起こらない。
  そこからImmNotifyIME()で変換できるが、それでは普通の連文節変換と変わらない。
  クライアントから送られてくる読み仮名をローマ字に戻し、キー入力としてimeに送ることにする。
  しかしこれはあんまりだろう。もうちょっとましな方法はないか？
*/
//1f
bool AutoConv(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    int32_t mode;
    HIMC imc;
    CannaContext_t *cx;
    char st=-1;

    Req5(ch,&cxn,&bufsize,&mode);
    LOG("context=%hd, bufsize=%hd, mode=0x%x\n",cxn,bufsize,mode);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	imc = ImmGetContext(cx->Win);
#ifdef SETCONTEXT_FAIL
	SetCurrentImc(imc,TRUE);
#else
	ImmSetOpenStatus(imc,TRUE);
	ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CANCEL,0);
#endif
	ImmSetConversionStatus(imc,CONV_MODE,IME_SMODE_AUTOMATIC);
	ImmReleaseContext(cx->Win,imc);
	cx->FerMode = mode;
	st=0;
    }
    return Reply2(ch->Major,ch->Minor,st);
}

//20
bool QueryExt(CanHeader* ch,int fd UNUSED)
{
    static const char name[]=
	"GetServerInfo\0"	"GetAccessControlList\0"
	"CreateDictionary\0"	"DeleteDictionary\0"
	"RenameDictionary\0"	"GetWordTextDictionary\0"
	"ListDictionary\0"	"Sync\0"
	"ChmodDictionary\0"	"CopyDictionary\0"

	//追加	pkt.hのプロトコル番号,main.cのinit_cb()も変更すること
	"WimeOpenIMEDialog\0"
	"WimeSetCompWin\0"
	"WimeGetCompWin\0"
	"WimeSendKey\0"
	"WimeEnableIme\0"
	"WimeMoveShadowWin\0"
	"WimeSetCompFont\0"
	"WimeGetCompStr\0"
	"WimeSetCandWin\0"
	"WimeRegXWindow\0"
	"WimeGetResultStr\0"
	"WimeSetResultStr\0"
	"WimeReconvert\0"
	"WimeSetFocus\0"
	"WimeShowToolbar\0"
	"WimeGetStyleList\0"
	"WimeReset\0"
	"WimeFlushMsg\0"
	"WimeShowCandidateWindow\0"
	"WimeSelectCandidate\0"
	"WimeCloseCandidateWindow\0"
	;
    Req17_t* rq = (Req17_t*)ch;

    int index=SubList(name,rq->p1);
    LOG("found index=%d\n",index);
    return Reply2(ch->Major,ch->Minor,index);
}

/*???
  モードとコンテキストはどういったときに使う？
  コンテキストが指定されるということは、コンテキストごとにアプリ名が指定されるということ？
*/
//21
bool SetAppName(CanHeader* ch,int fd)
{
    char *name;
    int32_t mode;
    int16_t cxn;

    name = Req15(ch,&mode,&cxn);
    LOG("mode %d,context %hd,name %s, IGNORE mode and context\n",mode,cxn,name);
    FindClient(fd)->App = strdup(name);
    return Reply2(ch->Major,ch->Minor,0);
}

//22
bool NoticeGroup(CanHeader* ch,int fd)
{
    char *name;
    int32_t mode;
    int16_t cxn;

    name = Req15(ch,&mode,&cxn);
    LOG("mode %d,context %hd,group %s, IGNORE mode and context\n",mode,cxn,name);
    FindClient(fd)->Group = strdup(name);
    return Reply2(ch->Major,ch->Minor,0);
}

//24
bool KillServer(CanHeader* ch,int fd UNUSED)
{
    LOG("kill wime\n");
    Reply2(ch->Major,ch->Minor,0);
    ImCloseAll();
    PostQuitMessage(0);
    return true;
}

//1-01
bool GetServerInfo(CanHeader* ch,int fd UNUSED)
{
    MSG("*** NOT IMPLIMENT ***\n");
    return Reply2(ch->Major,ch->Minor,-1);
}

//1-02
bool GetAcl(CanHeader* ch,int fd UNUSED)
{
    MSG("*** NOT IMPLIMENT ***\n");
    return Reply5(ch->Major,ch->Minor,-1);
}

//1-03
bool CreateDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *dic;

    dic = Req15(ch,&mode,&cxn);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, mode=%d, dic=%s\n",cxn,mode,dic);
    return Reply2(ch->Major,ch->Minor,-1);
}

//1-04
bool DeleteDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *dic;

    dic = Req15(ch,&mode,&cxn);
    MSG("*** NOT IMPLIMENT ***\n"); 
    LOG("context=%hd, mode=%d, dic=%s\n",cxn,mode,dic);
    return Reply2(ch->Major,ch->Minor,-1);
}

//1-05
bool RenameDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *cur_dic,*new_dic;

    new_dic = Req19(ch,&mode,&cxn,&cur_dic);
    MSG("*** NOT IMPLIMENT ***\n"); 
    LOG("context=%hd, mode=%d, current-dic=%s, new-dic=%s\n",cxn,mode,cur_dic,new_dic);
    return Reply2(ch->Major,ch->Minor,-1);
}

//1-06
bool GetWordTextDic(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    char *dir,*dic;
    uint16_t bufsize;

    bufsize = Req18((Req18_t*)ch,&cxn,&dir,&dic);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, directory=%s, dic=%s, bufsize=%hu\n",cxn,dir,dic,bufsize);
    return Reply5(ch->Major,ch->Minor,-1);
}

//1-07
/* ドキュメントでは要求タイプ18になっている。
   このプロトコルタイプ i16,s8,u16 をタイプ16にする
*/
bool ListDic(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    char *dirs;

    bufsize = Req16((Req16_t*)ch,&cxn,&dirs);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, bufsize=%hu, dir=%s\n",cxn,bufsize,dirs);
    return Reply2(ch->Major,ch->Minor,-1);
}

//1-08
bool Sync(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *dic;

    dic = Req15(ch,&mode,&cxn);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, mode=%d, dic=%s\n",cxn,mode,dic);
    return Reply2(ch->Major,ch->Minor,-1);
}

//1-09
bool ChmodDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *dic;

    dic = Req15(ch,&mode,&cxn);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, mode=%d, dic=%s\n",cxn,mode,dic);
    return Reply5(ch->Major,ch->Minor,-1);
}

//1-0a
bool CopyDic(CanHeader* ch,int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char *dir,*src,*dst;

    dst = Req21(ch,&mode,&cxn,&dir,&src);
    MSG("*** NOT IMPLIMENT ***\n");
    LOG("context=%hd, mode=%d, dir=%s, source=%s, destination=%s\n",cxn,mode,dir,src,dst);
    return Reply2(ch->Major,ch->Minor,-1);
}

/*
  要求：type2
	i16  ダイアログボックスの種類
		0=プロパティ
		1=単語／用例の登録
		2=ユーザー辞書の設定
  応答：type2
	i8   エラーの時=-1

  ??? IME_CONFIG_REGISTERWORDが失敗するのはなぜ？
*/
bool wm_wime_dialog(CanHeader* ch,int fd UNUSED)
{
    int modes[]={IME_CONFIG_GENERAL,IME_CONFIG_REGISTERWORD,IME_CONFIG_SELECTDICTIONARY};
    char st=-1;
    CannaContext_t* cx;
    uint16_t dialog_type = Req2(ch);
    REGISTERWORDA reg;

    LOG("dialog type code %d\n",dialog_type);

    //ImmConfigureIMEのhwndはグローバルコンテキストのものを使うことにする。
    reg.lpReading = reg.lpWord = NULL;
    cx = ArElem(&Context,0);

    if(dialog_type<3 && ImmConfigureIME(GetKeyboardLayout(0),cx->Win,modes[dialog_type],&reg)){
	st = 0;
    }
    return Reply2(ch->Major,ch->Minor,st);
}

#ifdef DEBUG
//lp=intの作業変数のアドレス,初期値０
BOOL CALLBACK EnumWin(HWND h,LPARAM lp)
{
    const char* hmdl(int mod)
    {
	static char buf[100];
	return GetModuleFileName((HMODULE)mod,buf,100) ? buf : "?";
    }

    int st,*counter=(int*)lp;
    char buf[100];
    LONG wl[][2]={{GWL_EXSTYLE,0},{GWL_STYLE,0},{GWL_WNDPROC,0},{GWL_HINSTANCE,0},{GWL_HWNDPARENT,0},{GWL_ID,0},{GWL_USERDATA,0}};
    DWORD cl[][2]={{GCW_ATOM,0},{GCL_HMODULE,0},{GCL_MENUNAME,0}};

    if((*counter)++ == 0)
	MSG("hwnd	class	exstyle	style	wndproc	instance	parent	id	userdata	atom	module	menu\n");

    for(st=0; st<7; ++st)
        wl[st][1] = GetWindowLong(h,wl[st][0]);
    for(st=0; st<3; ++st)
        cl[st][1] = GetClassLong(h,cl[st][0]);
    st = GetClassName(h,buf,100);

    MSG("%x\t%s\t",(unsigned)h,(st?buf:"(error)"));
    MSG("%x\t%x\t",wl[0][1],wl[1][1]); //exstyle,style
    MSG("%x\t",wl[2][1]); //wndproc
    MSG("%x(%s)\t%x\t",wl[3][1],hmdl(wl[3][1]),wl[4][1]); //instance,parent
    MSG("%x\t%x\t",wl[5][1],wl[6][1]); //id,userdata
    MSG("%x\t%x(%s)\t%x\n",cl[0][1],cl[1][1],hmdl(cl[1][1]),cl[2][1]); //atom,module,menu

    return TRUE;
}

void debug_window(HWND w)
{
    int dum=0;
    MSG("window listing...\n");
    EnumWindows(EnumWin,(LPARAM)&dum);
    MSG("...end,%d windows\n",dum);

    COMPOSITIONFORM cf;
    HIMC imc=ImmGetContext(w);
    ImmGetCompositionWindow(imc,&cf);
    ImmReleaseContext(w,imc);
    switch(cf.dwStyle){
    case CFS_DEFAULT:
	MSG("comp-form:default\n");
	break;
    case CFS_FORCE_POSITION:
	MSG("comp_form:force %d %d\n",cf.ptCurrentPos.x,cf.ptCurrentPos.y);
	break;
    case CFS_POINT:
	MSG("comp_form:point %d %d\n",cf.ptCurrentPos.x,cf.ptCurrentPos.y);
	break;
    case CFS_RECT:
	MSG("comp_form:rect %d %d %d %d\n",cf.rcArea.left,cf.rcArea.top,cf.rcArea.right,cf.rcArea.bottom);
	break;
    default:
	MSG("comp_form:??? (%x)\n",cf.dwStyle);
    }

    MSG("fg window:%x exist:%d visible:%d\n",(unsigned)GetForegroundWindow(),IsWindow(w),IsWindowVisible(w));
    MSG("fg window info...\n");
    dum=0;
    EnumWin(GetForegroundWindow(),(LPARAM)&dum);
    MSG("...end\n");
}
#endif

/*
  ImmSetCompositionWindow
  要求：type11
	i16=コンテキスト番号
	i16=style
	s16=WIME_POS_DEFAULT:なし
	    WIME_POS_{FORCE,POINT}:x,y
	    WIME_POS_RECT:x,y,w,h   windowsとは違うので注意
  応答：type2
	bool
*/
bool wm_wime_set_comp_win(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,style;
    uint16_t *params;
    COMPOSITIONFORM cf;
    CannaContext_t *cx;
    bool st=false;

    params = Req11r(ch,&cxn,&style);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	HIMC imc = ImmGetContext(cx->Win);

	switch(style){
	case WIME_POS_DEFAULT:
	    cf.dwStyle = CFS_DEFAULT;
	    break;
	case WIME_POS_FORCE:
	    cf.dwStyle = CFS_FORCE_POSITION;
	    break;
	case WIME_POS_POINT:
	    cf.dwStyle = CFS_POINT;
	    break;
	case WIME_POS_RECT:
	    cf.dwStyle = CFS_RECT;
	}

	switch(style){
	case WIME_POS_FORCE:
	case WIME_POS_POINT:
	    cf.ptCurrentPos.x = params[0];
	    cf.ptCurrentPos.y = params[1];
	    LOG("cxn=%hd pos=(%hd,%hd)\n",cxn,params[0],params[1]);
	    break;
	case WIME_POS_RECT:
	    cf.rcArea.left = params[0];
	    cf.rcArea.top = params[1];
	    cf.rcArea.right = params[0]+params[2];
	    cf.rcArea.bottom = params[1]+params[3];
	    //RECTのヘルプでは(r,b)は四角に含まれない？
	    LOG("cxn=%hd rect=(%hd,%hd)-(%hd,%hd)\n",cxn,cf.rcArea.left,cf.rcArea.top,cf.rcArea.right,cf.rcArea.bottom);
	}
	st = ImmSetCompositionWindow(imc,&cf);
	ImmReleaseContext(cx->Win,imc);
	LOG("cxn %hd,wnd %p,ret %d\n",cxn,cx->Win,st);
    }
    return Reply2(ch->Major,ch->Minor,st);
}

/*
  要求：type3
	i16=コンテキスト番号
	u16=winの仮想キーコード(VK_...とシフト状態(上8bit,cf. VkKeyScanEx))
  応答：type6
	i16=WIME_SENDKEY_XXX
	s8=imeに処理されたときの確定文字列(eucjp)(あれば送られる)
*/
bool wm_wime_send_key(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,st=WIME_SENDKEY_ERROR;
    uint16_t vk;
    CannaContext_t *cx;
    Array ej;
    bool rep_st;

    ArNew(&ej,1,NULL);
    Req3(ch,&cxn,&vk);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	HKL kl = GetKeyboardLayout(0);

	/*[r14]キー処理の後、変換キーではないのにWM_IME_NOTIFY,IMN_OPENCANDIDATEが飛んでくるときがある。
	  関係ないときにWIME_SENDKEY_OPENCANDを返してしまうので、先にフラグをクリアしておく*/
	cx->Flags &= ~(CATCH_OPEN_CAND|CATCH_CHG_CAND);

	if(proc_key_vk(vk,cx->Win,kl)){
	    HIMC imc = ImmGetContext(cx->Win);
	    st = WIME_SENDKEY_SUCCESS;
	    cx->Flags |= SEND_KEY; //wnd_proc()参照
	    GetClause(imc,cx,GCS_RESULTSTR,0,-1,&ej,NULL);
	    VERBOSE(DbgComp(imc,__func__));
	    ImmReleaseContext(cx->Win,imc);
	}else
	    st = WIME_SENDKEY_NO_PROC;

	/*???
	  再変換の時,この関数が終わる前にWM_IME_REQUESTが来る。
	  proc_key_vk()のImmProcessKey()でメッセージループが回されるようだ。
	  proc_key_vk()が通ったり通らなかったりよく分からないので、とにかくejは解放する。
	*/
	if(cx->Flags & PENDING_RECONV){
	    st = WIME_SENDKEY_RECONV;
	    cx->Flags &= ~PENDING_RECONV;
	    ArDelete(&ej);
	    LOG("reconvertion --> pending\n");
	}

	/*[r11]WM_IME_NOTIFY,IMN_OPENCANDIDATEはこの関数内では起こらず、次にメッセージループが回った
	  ときに処理されるみたい。CATCH_OPEN_CANDがセットされるのはそのときになるので、候補ウィンドウが
	  表示されたかがわかるのは次の呼び出しの時になってしまう。それでは1回余計に変換キーを押すことにな
	  るので、WM_IME_NOTIFYを処理させる。*/
	MsgLoopN(2,WM_IME_NOTIFY,IMN_OPENCANDIDATE,NULL,WM_IME_NOTIFY,IMN_CHANGECANDIDATE,NULL);
	/* ウィンドウプロシージャでWM_IME_NOTIFY,IMN_OPENCANDIDATEがきたらCATCH_OPEN_CANDがセットされる。
	   このフラグはGetCandiList()で消す */
	if(cx->Flags & CATCH_OPEN_CAND){
	    st = WIME_SENDKEY_OPENCAND;
	}
	if(cx->Flags & CATCH_CHG_CAND){
	    st = WIME_SENDKEY_CHGCAND;
	    cx->Flags &= ~CATCH_CHG_CAND; //これはここで消しても問題ない。
	}

	LOG("cxn %hd,wnd %p:vk 0x%hx --> proc_key status %hd\n",cxn,cx->Win,vk,st);
    }
    rep_st = Reply6s(ch->Major,ch->Minor,st,ArAdr(&ej));
    ArDelete(&ej);
    return rep_st;
}

/*
  変換候補ウィンドウの表示/非表示
  要求：type3
	i16=コンテキスト番号
	u16=bool
  応答：なし
  通常、候補ウィンドウは表示される。表示しないようにしたときは、WimeSendKey()で候補ウィンドウが表示されようとしたときにWIME_SENDKEY_OPENCANDが返されるので、GetCandiList()で変換候補を取得すること。

  候補ウィンドウが表示されるときにウィンドウプロシージャにWM_IME_NOTIFY,IMN_OPENCANDIDATEが送られるので、
  これがきたらcx->FlagsにCATCH_OPEN_CANDをセットする。
 */
bool wm_wime_show_candidate_window(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    uint16_t en;
    CannaContext_t* cx;
    bool st=false;
    Req3(ch,&cxn,&en);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	if(en)
	    cx->Flags &= ~TRAP_OPEN_CAND;
	else
	    cx->Flags |= TRAP_OPEN_CAND;
	st=true;
    }
    return Reply2(ch->Major,ch->Minor,st);
}

/*
  再変換
  要求：type11
	i16=コンテキスト番号
	i16=再変換文字列上のカーソル位置（文字単位）
	s16=再変換に使う文字列(u16)
  応答：type4
	i8=bool
	s32[0]=対象部分の開始位置(文字単位)
	s32[1]=対象部分の長さ（文字単位）
 */
bool wm_wime_reconv(CanHeader* ch,int fd UNUSED)
{
    uint16_t *reconv;
    int16_t cxn,cursor;
    CannaContext_t *cx;
    HIMC imc;
    int sz,bufsize;
    RECONVERTSTRING *rs;
    int32_t info[2];
    bool st;

    reconv = Req11r(ch,&cxn,&cursor);
    cx = ValidContext(cxn,__FUNCTION__);
    imc = ImmGetContext(cx->Win);
    sz = (WcLen(reconv)+1)*2;

    {
	Array x;ArNew(&x,1,NULL);
	if(reconv != NULL)
	    Dump2(" %04x",reconv,sz/2,&x);
	LOG("reconvert cursor:%hd, string:%s\n",cursor,ArAdr(&x));
	ArDelete(&x);
    }

    rs = calloc(bufsize = sizeof(RECONVERTSTRING)+sz,1);
    memcpy(rs+1,reconv,sz);
    rs->dwStrLen = sz-2;	//ヌル文字は除く
    rs->dwStrOffset = sizeof(*rs);
    rs->dwTargetStrOffset = cursor*2;	//バイト単位
    st = ImmSetCompositionStringW(imc,SCS_QUERYRECONVERTSTRING,rs,bufsize,NULL,0) && ImmSetCompositionStringW(imc,SCS_SETRECONVERTSTRING,rs,bufsize,NULL,0);
    info[0] = rs->dwCompStrOffset/2;	//??? atok08ではバイト数みたい
    info[1] = rs->dwCompStrLen;		//??? こっちは文字数みたい

    LOG("status %d, CompStrOffset %d, CompStrLen %d\n",st,rs->dwCompStrOffset,rs->dwCompStrLen);
    VERBOSE(DbgComp(imc,__func__));

    ImmReleaseContext(cx->Win,imc);
    free(rs);
    return Reply4(ch->Major,ch->Minor,st,info,2);
}

/*
  imcをもつ影ウィンドウの位置か大きさを変更する
  (x,y),(w,h)それぞれのどちらかが負であれば使用しない
  s16は文字として(バイトオーダーを変えずに)送ること
  要求：type11
	i16=コンテキスト番号
	i16=unused
	s16[0,1]=(x,y)
	s16[2,3]=(w,h)
  応答：type2
	bool
*/
bool wm_wime_move_shadow_win(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,dum,*ax;
    CannaContext_t *cx;
    bool st=false;

    ax = (int16_t*)Req11r(ch,&cxn,&dum);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	int flg = SWP_NOREDRAW; /*SWP_NOZORDER|SWP_HIDEWINDOW|*/
	if(ax[0]<0 || ax[1]<0)
	    flg |= SWP_NOMOVE;
	if(ax[2]<0 || ax[3]<0)
	    flg |= SWP_NOSIZE;
	st = SetWindowPos(cx->Win,HWND_TOP,ax[0],ax[1],ax[2],ax[3],flg);
	LOG("context %hd (%hd,%hd)-%hdx%hd --> status %d\n",cxn,ax[0],ax[1],ax[2],ax[3],st);
    }
    return Reply2(ch->Major,ch->Minor,st);
}

//xlfdのウェイト名を数値にする
//!!! mediumがFW_NORMALの400ではなく500になってしまうが、どうする？
int weight_value(const char* w,int wlen)
{
    struct{
	char *name;
	int value;
    } tab[]={
	{"thin",FW_THIN},{"extralight",FW_EXTRALIGHT}, {"ultraright",FW_ULTRALIGHT},{"light",FW_LIGHT},	{"normal",FW_NORMAL},{"regular",FW_REGULAR},{"medium",FW_MEDIUM},{"semibold",FW_SEMIBOLD}, {"demibold",FW_DEMIBOLD},{"bold",FW_BOLD},{"extrabold",FW_EXTRABOLD},{"ultrabold",FW_ULTRABOLD}, {"heavy",FW_HEAVY},{"black",FW_BLACK}
    };
    char wname[wlen+1];
    memcpy(wname,w,wlen);
    wname[wlen]=0;

    int v = FW_NORMAL;
    for(unsigned n=0; n<ITEMS(tab); ++n){
	if(strcasecmp(wname,tab[n].name)==0){
	    v=tab[n].value;
	    break;
	}
    }
    return v;
}

/*
  フォントセットのjisx0208からLOGFONTを設定する。
*/
bool fontset_to_logfont(LOGFONT* lf,const char* fs)
{
    char *pt[14],jx0208[]="jisx0208";
    bool st=false;

    memset(lf,0,sizeof(*lf));
    lf->lfCharSet=SHIFTJIS_CHARSET;
    lf->lfOutPrecision=OUT_DEFAULT_PRECIS;
    lf->lfClipPrecision=CLIP_DEFAULT_PRECIS;
    lf->lfWeight = FW_NORMAL;

    while(fs != NULL){
	//xlfdの要素の先頭アドレスをpt[]に入れる
	for(int n=0; n<14; ++n)
	    pt[n] = NULL;
	for(int n=0; n<14; ++n){
	    if((pt[n] = strchr(fs,'-'))==NULL || *(fs = ++pt[n])==0)
		break;
	}

	if(strncasecmp(pt[12],jx0208,sizeof(jx0208)-1) == 0){
	    int len = pt[2]-pt[0]-1;
	    memcpy(lf->lfFaceName,pt[0],len);
	    lf->lfFaceName[len] = 0;
	    lf->lfFaceName[pt[1]-pt[0]-1] = ' ';

	    if((lf->lfHeight = atoi(pt[6])) == 0)
		lf->lfHeight = 16; //!!! なんとかしなければ
	    lf->lfWeight = weight_value(pt[2],pt[3]-pt[2]-1);
	    if(*pt[3] == 'i')
		lf->lfItalic = TRUE;
	    st=true;
	    break;
	}

	fs = strchr(fs,',');
    }
    return st;
}

/*
  変換ウィンドウのフォントを指定する
  要求：type15
	i32=背景色
	i16=コンテキスト番号
	s8=フォントセット
  応答：type5
	i16=フォントの高さ。エラーの時0

  ??? 背景色の指定はどうやるか。
*/
bool wm_wime_set_comp_font(CanHeader* ch,int fd UNUSED)
{
    int16_t h=0,cxn;
    uint32_t bg;
    CannaContext_t *cx;
    char *fontname;

    MSG("*** PARTIAL IMPLIMENT ***\n"); 
    fontname = Req15(ch,(int32_t*)&bg,&cxn);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	LOGFONT lf;
	HIMC imc = ImmGetContext(cx->Win);

	LOG("fontset '%s'\n",fontname);
	if(fontset_to_logfont(&lf,fontname)){
	    LOG("alias name='%s',size %d,weight %d,italic=%d\n",lf.lfFaceName,lf.lfHeight,lf.lfWeight,lf.lfItalic);
	    if(!ImmSetCompositionFont(imc,&lf))
		MSG("fail ImmSetCompositionFont()\n");
	}else
	    MSG("fail fontset_to_logfont()\n");

	ImmGetCompositionFont(imc,&lf);
	h = abs(lf.lfHeight);
	LOG("facename '%s',height %d\n",lf.lfFaceName,lf.lfHeight);

	ImmReleaseContext(cx->Win,imc);
    }
    return Reply5(ch->Major,ch->Minor,h);
}

/*
  imeを使用する
  要求：type3
	i16	コンテキスト番号
	u16	bool,あるいは-1
  応答：type5
	i16	u16が-1のとき現在の状態,boolのときu16,エラーの時-1
*/
bool wm_wime_enable_ime(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,st=-1;
    uint16_t en_ime;
    CannaContext_t *cx;

    Req3(ch,&cxn,&en_ime);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	HIMC imc = ImmGetContext(cx->Win);
	if(en_ime == (uint16_t)-1){
	    st = ImmGetOpenStatus(imc);
	    LOG("cxn %hd open status %hd\n",cxn,st);
	}else{
	    /*[3.4.3]
	      候補ウィンドウを閉じる(候補ウィンドウを出しているかどうかはチェックしていない)。
	      cx->ImeWndのimcをImmNotifyIMEで使ってもウィンドウは閉じず、SendMessageだと閉じた。なぜ？
	      gtkのtreeウィジェットのインライン編集でESCを押したりフォーカスが外れたりした場合、
	      imwime_set_focus()でフォーカスアウトになった後直ぐにimwime_finalize()が呼ばれ、
	      候補ウィンドウが出たままになってしまう。
	      imwime_finalize()でウィンドウを閉じた方がいいか?
	      →ImmSetOpenStatus()でfalseにしないと新しくコンテキストをつくったときに候補ウィンドウが
	      復活してしまう(使い回ししているせいだろう)。
	    */
	    if(!en_ime)
		SendMessageW(cx->ImeWnd,WM_IME_NOTIFY,IMN_CLOSECANDIDATE,0);

	    st = ImmSetOpenStatus(imc,en_ime) ? en_ime : -1;
	    LOG("cxn %hd en_ime %hd\n",cxn,en_ime);
	}
	ImmReleaseContext(cx->Win,imc);
    }
    return Reply5(ch->Major,ch->Minor,st);
}

/*
  imeのツールバーを表示する
  要求：type7
	i16	コンテキスト番号
	i16	bool ツールバーを表示
	i16	bool 変換ウィンドウを使う
  応答：無し
*/
bool wm_wime_show_toolbar(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,show_tb,show_comp_win;
    CannaContext_t *cx;
    bool st=false;

    Req7(ch,&cxn,&show_tb,&show_comp_win);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	HIMC imc = ImmGetContext(cx->Win);
	LOG("cxn %hd, use-tb %hd, use-cw %hd, ime-win %p\n",cxn,show_tb,show_comp_win,cx->ImeWnd);
	if(show_tb){
	    BringWindowToTop(cx->Win); //影窓が重なっていると候補ウィンドウが隠されてしまうので一番上にする
	    cx->Flags &= ~PROC_COMP_MSG;
	    if(show_comp_win)
		cx->Flags |= PROC_COMP_MSG;
	    cx->Flags |= PROC_NOTIFY_MSG;
	    ImmSetConversionStatus(imc,CONV_MODE,IME_SMODE_PHRASEPREDICT);
	}else{
	    SendMessageW(cx->Win,WM_IME_NOTIFY,IMN_CLOSESTATUSWINDOW,0);
	    cx->Flags &= ~(PROC_COMP_MSG|PROC_NOTIFY_MSG);
	}
	ImmReleaseContext(cx->Win,imc);
	st = true;
    }
    return st;
}

/*
  フォーカスの移動を知らせる
  要求:	int p1	cxn
	int p2	bool(in=true,out=false)
  応答：なし
*/
bool wm_wime_set_focus(CanHeader* ch,int fd UNUSED)
{
    int *p = (typeof(p))(ch+1);
    CannaContext_t *cx;
    bool st=false;
#define CXN p[0]
#define FOCUS_IN p[1]

    if((cx = ValidContext(CXN,__FUNCTION__)) != NULL){
	LOG("cxn %d, wnd %p, ime-wnd %p:focus --> %s\n",CXN,cx->Win,cx->ImeWnd,FOCUS_IN?"in":"out");
	if(FOCUS_IN){
	    SendMessageW(cx->Win,WM_IME_NOTIFY,IMN_OPENSTATUSWINDOW,0);
	    SetFocus(cx->Win);
	}else{
	    SendMessageW(cx->Win,WM_IME_NOTIFY,IMN_CLOSESTATUSWINDOW,0);
	}
	st = true;
    }
    return st;

#undef CXN
#undef FOCUS_IN
}

/*
  変換中の文字列を得る
  要求：type2
	i16=コンテキスト番号
  応答：type10
	i8	エラー=0,変換中文字列がある=1,ない=-1
	s8	変換中文字列(euc-jp)
	s8	(nil)
	s32	WimeCompStrInfo
*/
bool wm_wime_get_comp_str(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    CannaContext_t *cx;
    WimeCompStrInfo si;
    char ret_code=0;
    Array comp_str;

    ArNew(&comp_str,1,NULL);
    memset(&si,0,sizeof(si));
    si.TargetClause = -1;
    cxn = Req2(ch);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	Array ej;
	char attr;
	int cln;
	HIMC imc = ImmGetContext(cx->Win);
	ArNew(&ej,1,NULL);

	//全文節をeucjpにしてつなげる
	for(cln=0; GetClause(imc,cx,GCS_COMPSTR,cln,cln,&ej,&attr)!=NULL; ++cln){
	    if(attr==ATTR_TARGET_CONVERTED || attr==ATTR_TARGET_NOTCONVERTED){
		si.TargetClause = EjLen(ArAdr(&comp_str));
		si.TargetClLen = EjLen(ArAdr(&ej));
	    }
	    ArAddAr(ArDec(&comp_str),&ej);
	}
	si.Length = EjLen(ArAdr(&comp_str));
	si.TargetNum = GetAttrCl(imc,ATTR_TARGET_CONVERTED,cx);

	//CursorPosとDeltaStartを取得
	INPUTCONTEXT *ic=(INPUTCONTEXT*)ImmLockIMC(imc);
	COMPOSITIONSTRING *cs=(COMPOSITIONSTRING*)ImmLockIMCC(ic->hCompStr);
	si.CursorPos = cs->dwCursorPos;
	si.DeltaStart = cs->dwDeltaStart;

	ImmUnlockIMCC(ic->hCompStr);
	ImmUnlockIMC(imc);

	ImmReleaseContext(cx->Win,imc);
	ArDelete(&ej);

	if(cln > 0){
	    LOG("'%s' %d %d %d %d %d\n",(char*)ArAdr(&comp_str),si.CursorPos,si.DeltaStart,si.TargetClause,si.TargetClLen,si.Length);
	    ret_code = 1;
	}else{
	    LOG("(none)\n");
	    ret_code = -1;
	}
    }

    bool st = Reply10(ch->Major,ch->Minor,ret_code,(ret_code>0 ? ArAdr(&comp_str):""),"",(int32_t*)&si,sizeof(si));
    ArDelete(&comp_str);
    return st;
}

/*
  変換ウィンドウの情報
  要求：type2
	i16=コンテキスト番号
  応答：type4
	i8	エラー=0
	s32[0]	スタイル(WIME_POS_xxx)
	s32[1]	x
	s32[2]	y
	s32[3]	w
	s32[4]	h

	使わない座標データは-1
*/
bool wm_wime_get_comp_win(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    CannaContext_t *cx;
    int32_t v[5],*vp;
    char st=0;

    cxn = Req2(ch);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	COMPOSITIONFORM cf;
	HIMC imc = ImmGetContext(cx->Win);
	if(ImmGetCompositionWindow(imc,&cf)){
	    vp = memset(v,-1,sizeof(v));
	    st = 1;
	    switch(cf.dwStyle){
	    case CFS_DEFAULT:
		*vp = WIME_POS_DEFAULT;
		break;
	    case CFS_FORCE_POSITION:
		*vp = WIME_POS_FORCE;
		break;
	    case CFS_POINT:
		*vp = WIME_POS_POINT;
		break;
	    case CFS_RECT:
		*vp = WIME_POS_RECT;
	    }
	    ++vp;
	    switch(cf.dwStyle){
	    case CFS_FORCE_POSITION:
	    case CFS_POINT:
		*(vp++) = cf.ptCurrentPos.x;
		*(vp++) = cf.ptCurrentPos.y;
		break;
	    case CFS_RECT:
		*(vp++) = cf.rcArea.left;
		*(vp++) = cf.rcArea.top;
		*(vp++) = cf.rcArea.right - cf.rcArea.left;
		*(vp++) = cf.rcArea.bottom - cf.rcArea.top;
	    }
	    VERBOSE(Array a;
		    ArNew(&a,1,NULL);
		    MSG("%s\n",ArAdr(Dump4("[%d]",v,ITEMS(v),&a)));
		    ArDelete(&a);
		);
	}else
	    MSG("fail ImmGetCompositionWindow\n");
	ImmReleaseContext(cx->Win,imc);
    }
    return Reply4(ch->Major,ch->Minor,st,v,ITEMS(v));
}

/*
  ImmSetCandidateWindow
  要求：type11
	i16=コンテキスト番号
	i16=style WIME_POS_POINT,WIME_POS_EXCLUDE
	s16 [0,1]=x,y
	    [2,3,4,5]=x,y,w,h WIME_POS_EXCLUDEのとき(WIME_POS_POINTでは無視)
  応答：type2
	bool
*/
bool wm_wime_set_cand_win(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn,style;
    uint16_t *params;
    CannaContext_t *cx;
    bool st=false;

    params = Req11r(ch,&cxn,&style);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	CANDIDATEFORM cf;
	HIMC imc = ImmGetContext(cx->Win);

	cf.dwIndex = 0;
	cf.ptCurrentPos.x = params[0];
	cf.ptCurrentPos.y = params[1];
	LOG("cxn=%hd pos=(%hd,%hd)\n",cxn,params[0],params[1]);
	switch(style){
	case WIME_POS_POINT:
	    cf.dwStyle = CFS_CANDIDATEPOS;
	    break;
	case WIME_POS_EXCLUDE:
	    cf.dwStyle = CFS_EXCLUDE;
	    cf.rcArea.left = params[2];
	    cf.rcArea.top = params[3];
	    cf.rcArea.right = cf.rcArea.left+params[4];
	    cf.rcArea.bottom = cf.rcArea.top+params[5];
	    LOG("rect=(%hd,%hd)-(%hd,%hd)\n",cf.rcArea.left,cf.rcArea.top,cf.rcArea.right,cf.rcArea.bottom);
	}
	st = ImmSetCandidateWindow(imc,&cf);
	ImmReleaseContext(cx->Win,imc);
	LOG("cxn %hd,wnd %p,ret %d\n",cxn,cx->Win,st);
    }
    return Reply2(ch->Major,ch->Minor,st);
}

/*
  cannaコンテキストとXウィンドウを関連づける。このウィンドウに何か情報を送る。
  atokのパレットツールからの入力の時にImAuxInput()で使っている。
  要求:PktRegXWin
  応答:なし
*/
bool wm_wime_reg_x_window(CanHeader* ch,int fd UNUSED)
{
    PktRegXWin *p = (typeof(p))(ch+1);
    CannaContext_t *cx;
    bool st=false;

    if((cx = ValidContext(p->cxn,__FUNCTION__)) != NULL){
	cx->XWin = p->xwin;
	st = true;
	LOG("cxn %hd window %x\n",p->cxn,p->xwin);
    }
    return st;
}

/*
  変換確定文字列を得る。
  要求：PktCxNum
  応答：確定文字列(ucs2le,ヌル文字付き)
*/
bool wm_wime_get_result_str(CanHeader* ch,int fd UNUSED)
{
    PktCxNum *p = (typeof(p))(ch+1);
    CannaContext_t *cx;
    bool st;
    Array scs[CS_MAX];

    CompNew(scs);
    if((cx = ValidContext(p->cxn,__FUNCTION__)) != NULL){
	int nul=0;
	HIMC imc = ImmGetContext(cx->Win);
	StoreComp(scs,imc,0,-1,EN_RESULT);
	ArAdd(&scs[CS_RESULT],&nul);
	ImmReleaseContext(cx->Win,imc);
	VERBOSE(Array d;ArNew(&d,2,NULL);MSG("result str(ucs2)=%s\n",ArAdr(Dump2(" 0x%04x",ArAdr(&scs[CS_RESULT]),ArUsing(&scs[CS_RESULT]),&d)));ArDelete(&d));
    }
    st = ReplyN(ch->Major,ch->Minor,ArAdr(&scs[CS_RESULT]),ArUsingBytes(&scs[CS_RESULT]));
    CompDelete(scs);
    return st;
}

/*
  単語登録に使う品詞の一覧を得る
  要求：なし
  応答：i32	数
       i32	品詞名の最大長（ヌル文字含む）
       i32[]	コードの配列
       s8[]	品詞名の並び。'0'区切り。'0'で終了。eucjp
*/
bool wm_wime_get_style_list(CanHeader* ch,int fd UNUSED)
{
    int count,len;
    STYLEBUFW *sty;
    char *desc;
    int32_t *code;
    bool st;
    HKL kl;
    PktStyleList *buf;

    kl = GetKeyboardLayout(0);
    count = ImmGetRegisterWordStyleW(kl,0,NULL);
    sty = malloc(sizeof(*sty)*count);
    buf = malloc(sizeof(int32_t)+sizeof(int32_t)*count+(sizeof(sty->szDescription)+1)*count+1);

    buf->count = count;
    buf->desc_max = 0;
    code = buf->code;
    desc = (char*)(code+count);
    ImmGetRegisterWordStyleW(kl,count,sty);
    LOG("%d items\n",count);

    while(--count >= 0){
	U16ToEj(desc,sty->szDescription,ITEMS(sty->szDescription));
	desc += (len = strlen(desc)+1);
	*(code++) = sty->dwStyle;
	if(len > buf->desc_max)
	    buf->desc_max = len;
	++sty;
    }
    *(desc++) = 0;

    st = ReplyN(ch->Major,ch->Minor,buf,desc-(char*)buf);
    free(sty);
    free(buf);
    return st;
}

/*
  設定ファイルを再読み込みする。
  imcを作り直す。
  要求：なし
  応答：int	0=成功
  ??? エラーコードでも返すか？
*/
bool wm_wime_reset(CanHeader* ch,int fd UNUSED)
{
    int st = ImReadSetting(&WimeData);
    RecreateWindow();
    LOG("reload setting file:%d\n",st);
    return ReplyN(ch->Major,ch->Minor,&st,sizeof(st));
}

/*
  メッセージループを回す
  要求：なし
  応答：int	0=全部処理した 1=途中で中断した
  imeが内部でメッセージを送ることがある(SendKeyなどでimeを操作している場合は特に)。
  それらのメッセージが処理される前に終了処理が行われると、ウィンドウなどが表示されたままになることがある。
  終了前にはこの関数を呼んでおいた方がいい。
  !!! ime offのタイミングで自動的に呼び出すようにするべきか？こんな処理は表に出すべきではないように思う。
*/
bool wm_wime_flush_msg(CanHeader* ch,int fd UNUSED)
{
    MSG msg;
    int st=0;

    while(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE)){
	if(GetMessage(&msg,NULL,0,0) <= 0){
	    st = 1;
	    break;
	}
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }
    return ReplyN(ch->Major,ch->Minor,&st,sizeof(st));
}

/*
  変換候補を選択する
  要求：type3
	i16	コンテキスト番号
	u16	候補番号
  応答：type2
	bool
  ??? 候補リストページ番号はどうしよう?
*/
bool wm_wime_select_candidate(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    bool st=false;
    uint16_t index;
    CannaContext_t* cx;

    Req3(ch,&cxn,&index);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	LOG("cxn %hd, index %hd\n",cxn,index);
	HIMC imc = ImmGetContext(cx->Win);
	if(ImmNotifyIME(imc,NI_SELECTCANDIDATESTR,0,index+WimeData.CandIndexStart)){
	    st=true;
	    VERBOSE(DbgComp(imc,__func__));
	}
	ImmReleaseContext(cx->Win,imc);
    }
    return Reply2(ch->Major,ch->Minor,st);
}

/*
  変換候補ウィンドウを閉じる。
  SendKeyで自動的に開いたウィンドウを閉じるために使う。
  ??? showもcloseもset_cand_winにまとめてしまったらどうだろう?
  要求：type2
	i16=コンテキスト番号
  応答:なし
*/
bool wm_wime_close_candidate_window(CanHeader* ch,int fd UNUSED)
{
    int16_t cxn;
    CannaContext_t *cx;
    cxn = Req2(ch);
    if((cx = ValidContext(cxn,__FUNCTION__)) != NULL){
	//wm_wime_enable_imeのコメント参照。
	SendMessageW(cx->ImeWnd,WM_IME_NOTIFY,IMN_CLOSECANDIDATE,0);
    }
    return true;
}

/*
  文字列を外部入力とする。
  要求：PktResultStr(eucjp)
  応答：なし
*/
bool wm_wime_set_result_str(CanHeader* ch,int fd UNUSED)
{
    PktResultStr *p = (typeof(p))(ch+1);
    CannaContext_t *cx;
    bool st=false;
    if((cx = ValidContext(p->cxn,__FUNCTION__)) != NULL){
	LOG("context %d,string='%s'\n",p->cxn,p->str);

	HIMC imc = ImmGetContext(cx->Win);
#ifdef SETCONTEXT_FAIL
	SetCurrentImc(imc,TRUE);
#endif
	ImmSetConversionStatus(imc,CONV_MODE,IME_SMODE_PHRASEPREDICT);
	uint16_t *uc = EjToU16(NULL,p->str);
	ImmSetCompositionStringW(imc,SCS_QUERYRECONVERTSTRING,uc,WcLen(uc)*2,NULL,0);
	DbgComp(imc,__func__);
	ImmSetCompositionStringW(imc,SCS_SETRECONVERTSTRING,uc,WcLen(uc)*2,NULL,0);
	DbgComp(imc,__func__);
	ImmNotifyIME(imc,NI_COMPOSITIONSTR,CPS_CONVERT,0);
	DbgComp(imc,__func__);


	//st = set_yomi_str(cx,IME_SMODE_PHRASEPREDICT,CPS_COMPLETE,p->str,0);

/*
	HIMC imc = ImmGetContext(cx->Win);
#ifdef SETCONTEXT_FAIL
	SetCurrentImc(imc,FALSE);
#else
	ImmSetOpenStatus(imc,FALSE);
#endif
	ImmReleaseContext(cx->Win,imc);
*/

/*
	if(st)
	    PostMessage(cx->Win,WM_IME_COMPOSITION,0,GCS_RESULTSTR|GCS_RESULTCLAUSE);
*/
    }
    return st;
}
