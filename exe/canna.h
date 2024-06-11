// -*- coding:euc-jp -*-
#pragma once

#define CANNA_NEW_WCHAR_AWARE
#ifdef __WINNT__
  #include <windows.h>
  #ifdef HAVE_IMMDEV
    #include <immdev.h>
  #else
    #include <ddk/imm.h>
  #endif
  #if defined(__FreeBSD__)
    #define _WCHAR_T_DECLARED
  #endif
#else
  typedef void* HWND;
  typedef void* HIMC;
  typedef void* COMPOSITIONSTRING;
  typedef void CANDIDATELIST;
  typedef int BOOL;
  typedef unsigned int DWORD;
  typedef const void* LPCVOID;
  #define WINAPI
#endif
#include <stdint.h>
#include <canna/RK.h>
#include "lib/array.h"

/* version.hへ移動
#define WIME_CANNA_MAJOR 3
#define WIME_CANNA_MINOR 6
*/
#define RETURN_VERSION_ERROR_STAT -2

#include "so/pkt.h"
#define CANNAHEADERSIZE sizeof(CanHeader)

typedef struct{
    HWND Win;
    int Conv;		//変換時に注目文節番号を入れる。それ以外の状態の時は-1
    int FerMode;	//RkBgnBunのモード
    RkStat RkSt;	//!!! 持っておく必要なしか？
    Array CandInfo;	//各文節の変換リストのページサイズ
    int Connection;	//通信先のファイルディスクリプタ
    unsigned SerialNum; //通し番号。 memoの'libcannaとの併用'を参照 !!!これも消す？
    unsigned Flags;
    unsigned XWin;	//KeyPressイベントを送るXのウィンドウ
    HIMC DefImc;	//デフォルトimc
    HWND ImeWnd;	//imcがつかっているime-window
    bool UseToolbar;	//ツールバーを使うときtrue
    bool ImeOpen;	//オープンしていればtrue。コンテキストごとに個別に状態を持つ。
    
    Array Dics;		//辞書名のリスト(char*)
    Array DicMode;	//辞書のモード(int32)

    int FixedNum;	//自動変換モードで勝手に確定された文節の数
    Array FixedStr;	//勝手に確定された文節の結果文字列のリスト(u16)
    Array FixedYomi;	//結果文字列の読み仮名のリスト(u16)
    int YomiBufStart;	//cannaの"読みバッファ"の開始位置(compread先頭からのオフセット)
    int ConvertedCl;	//自動変換されている文節の数
} CannaContext_t;

//CannaContext_t.Flags  wimectrlの'-x c'でフラグ名を使っている。
#define OPEN_STATUS_WINDOW	(1<<0)	//ステータスウィンドウを表示している
#define PROC_NOTIFY_MSG		(1<<1)	//WM_IME_NOTIFYをDefWindowProcにわたす(ステータスウィンドウを使う)
#define PROC_COMP_MSG		(1<<2)	//WM_IME_COMPOSITIONをDefWindowProcにわたす(変換ウィンドウを使う)
#define PENDING_RECONV		(1<<3)	//再変換のメッセージが来た
#define SEND_KEY		(1<<4)	//wm_wime_send_keyを使った
#define TRAP_OPEN_CAND		(1<<5)	//候補ウィンドウが開かれようとした(WM_IME_NOTIFY,IMN_OPENCANDIDATE)
#define CATCH_OPEN_CAND		(1<<6)	//TRAP_OPEN_CANDに引っかかったらこのフラグをセットする
#define CATCH_CHG_CAND		(1<<7)	//TRAP_OPEN_CANDに引っかかったらこのフラグをセットする
#define USE_UTF16LE		(1<<8)
#define USE_UTF16BE		(1<<9)
#define USE_UTF16		(USE_UTF16LE|USE_UTF16BE)
#define CATCH_FINISH		(1<<10) //WM_IME_COMPOSITIONで変換を確定したとき

//CandInfoの要素
#define CANDLISTMAX 4
typedef struct{
    int Size[CANDLISTMAX];	//各ページの候補数
    int Seq;			//候補ウィンドウが出ないときの候補数
} CandListPageInfo;

//クライアント情報
typedef struct{
    int Connection; //通信先のファイルディスクリプタ
    char* User;
    char* App;
    char* Group;
} ClientData_t;
    
//品詞コード
typedef struct{
    char* Ccode;	//かんなのコード。先頭の'#'はつかない。正規表現。
    int Wcode;		//STYLEBUF.dwStyleの値
} HinshiCor;

//設定ファイルのデータ,他
struct GlobalData_t{
    HinshiCor* HinshiTab;	//品詞コード

    //lookup_cand_win()で使用 ImmGetCandidateListを使って候補文字列を取得。
    void (*GetCandidate)(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb);

    //unicode=1,sjis=2
    int CharStep;

    //ImmSet/GetCompositionString
    //!!!自前でやる必要があるのか調べ直そう
    BOOL WINAPI (*SetCompStr)(HIMC,DWORD,LPCVOID,DWORD,LPCVOID,DWORD);
    uint16_t* (*GetCompStr)(HIMC imc,DWORD index);

    int CandIndexStart; //IME_PROP_CANDLIST_START_FROM_1 ???ちゃんと使われているんだろうか?

    int (*ImeVersion)(void); //imeのバージョンを返す。
};
extern struct GlobalData_t WimeData;

//WimeData.GetCandidate
void GetCandidateAtok(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb);
void GetCandidateW(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb);


typedef bool (*WMCANNAPROTO)(CanHeader*,int);

bool Init(CanHeader* ch,int fd);
bool Finalize(CanHeader* ch,int fd);
bool CreateContext(CanHeader* ch,int fd);
bool DupContext(CanHeader* ch,int fd);
bool CloseContext(CanHeader* ch,int fd);
bool GetDicList(CanHeader* ch,int fd);
bool GetDirList(CanHeader* ch,int fd);
bool MountDic(CanHeader* ch,int fd);
bool UnmountDic(CanHeader* ch,int fd);
bool BeginConv(CanHeader* ch,int fd);
bool GetCandiList(CanHeader* ch,int fd);
bool SetAppName(CanHeader* ch,int fd);
bool NoticeGroup(CanHeader* ch,int fd);
bool GetStatus(CanHeader* ch,int fd);
bool EndConv(CanHeader* ch,int fd);
bool ResizePause(CanHeader* ch,int fd);
bool DefineWord(CanHeader* ch,int fd);
bool RemountDic(CanHeader* ch,int fd);
bool MountDicList(CanHeader* ch,int fd);
bool QueryDic(CanHeader* ch,int fd);
bool DeleteWord(CanHeader* ch,int fd);
bool GetYomi(CanHeader* ch,int fd);
bool SubstYomi(CanHeader* ch,int fd);
bool StoreYomi(CanHeader* ch,int fd);
bool StoreRange(CanHeader* ch,int fd);
bool GetLastYomi(CanHeader* ch,int fd);
bool FlushYomi(CanHeader* ch,int fd);
bool RemoveYomi(CanHeader* ch,int fd);
bool GetSimpleKanji(CanHeader* ch,int fd);
bool GetHinshi(CanHeader* ch,int fd);
bool GetLex(CanHeader* ch,int fd);
bool SetLocale(CanHeader* ch,int fd);
bool AutoConv(CanHeader* ch,int fd);
bool QueryExt(CanHeader* ch,int fd);
bool KillServer(CanHeader* ch,int fd);

bool GetServerInfo(CanHeader* ch,int fd);
bool GetAcl(CanHeader* ch,int fd);
bool CreateDic(CanHeader* ch,int fd);
bool DeleteDic(CanHeader* ch,int fd);
bool RenameDic(CanHeader* ch,int fd);
bool GetWordTextDic(CanHeader* ch,int fd);
bool ListDic(CanHeader* ch,int fd);
bool Sync(CanHeader* ch,int fd);
bool ChmodDic(CanHeader* ch,int fd);
bool CopyDictionary(CanHeader* ch,int fd);

bool OpenDialog(CanHeader* ch,int fd);
bool SetCompWin(CanHeader* ch,int fd);
bool GetCompWin(CanHeader* ch,int fd);
bool SendKey(CanHeader* ch,int fd);
bool EnableIme(CanHeader* ch,int fd);
bool MoveShadowWin(CanHeader* ch,int fd);
bool SetCompFont(CanHeader* ch,int fd);
bool GetCompStr(CanHeader* ch,int fd);
bool SetCandWin(CanHeader* ch,int fd);
bool RegXWin(CanHeader* ch,int fd);
bool GetResultStr(CanHeader* ch,int fd);
bool SetResultStr(CanHeader* ch,int fd);
bool Reconvert(CanHeader* ch,int fd);
bool SetImeFocus(CanHeader* ch,int fd);
bool ShowToolbar(CanHeader* ch,int fd);
bool GetStyleList(CanHeader* ch,int fd);
bool ReloadConf(CanHeader* ch,int fd);
bool FlushMsg(CanHeader* ch,int fd);
bool ShowCandWin(CanHeader* ch,int fd);
bool SelectCand(CanHeader* ch,int fd);
bool CloseCandWin(CanHeader* ch,int fd);
bool DumpContext(CanHeader* ch,int fd);
bool SetDebugChannel(CanHeader* ch,int fd);
bool GetColor(CanHeader* ch,int fd);
bool GetCandWin(CanHeader* ch,int fd);
bool CandIndex(CanHeader* ch,int fd);

//(C) 2008 thomas
