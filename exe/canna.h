#ifndef WIME_EXE_CANNA
#define WIME_EXE_CANNA

#define CANNA_NEW_WCHAR_AWARE
#ifdef __WINNT__
  #include <windows.h>
  #include <ddk/imm.h>
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

    Array Dics;		//辞書名のリスト(char*)
    Array DicMode;	//辞書のモード(int32)

    int FixedNum;	//自動変換モードで勝手に確定された文節の数
    Array FixedStr;	//勝手に確定された文節の結果文字列のリスト
    Array FixedYomi;	//結果文字列の読み仮名のリスト
} CannaContext_t;

//CannaContext_t.Flags
#define OPEN_STATUS_WINDOW	1	//ステータスウィンドウを表示している
#define PROC_NOTIFY_MSG		2	//WM_IME_NOTIFYをDefWindowProcにわたす(ステータスウィンドウを使う)
#define PROC_COMP_MSG		4	//WM_IME_COMPOSITIONをDefWindowProcにわたす(変換ウィンドウを使う)
#define PENDING_RECONV		8	//再変換のメッセージが来た
#define SEND_KEY		16	//wm_wime_send_keyを使った

//CandInfoの要素
#define CANDLISTMAX 4
typedef struct{
    int Size[CANDLISTMAX];	//各ページの候補数
    int Seq;			//候補ウィンドウが出ないときの候補数
} CandListPageInfo;

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

    /* imeのプロパティに応じてSETSTRでImmSetCompositionStringA/Wを呼びだす。
       ImmSetCompositionString()が勝手にやってくれるはずであるが、出力がおかしい
       ことがあったので自前でやることにする。
       ()を使うと ej→sj→u16 と余計な変換をする必要があるので、この点でもやる
       意味がある、としておく。
       !!! ()で問題ないことがはっきりしたらこれはやめよう。でも余計な変換はどうする？
    */
    bool (*SetRead)(HIMC imc,const char* yomi);

    //imcのCompStr --> ej
    char* (*GetClause)(const COMPOSITIONSTRING* cs,int str_offset,int cl_offset,int n,int nlen);

    //lookup_cand_win
    void (*GetCandidate)(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb);

    //unicode=1,sjis=2
    int CharSize;

    //ImmSet/GetCompositionString
    //!!!自前でやる必要があるのか調べ直そう
    BOOL WINAPI (*SetCompStr)(HIMC,DWORD,LPCVOID,DWORD,LPCVOID,DWORD);
    void* (*GetCompStr)(HIMC imc,DWORD index);
};
extern struct GlobalData_t WimeData;

//WimeData.GetCandidate
void GetCandidateAtok(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb);
void GetCandidateA(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb);
void GetCandidateW(HIMC imc,const CannaContext_t* cx,Array* euclist,int clnum,unsigned listnum,CANDIDATELIST* cb);


typedef bool (*WMCANNAPROTO)(CanHeader*,int);

bool CannaInit(CanHeader* ch,int fd);
bool CannaFinalize(CanHeader* ch,int fd);
bool CannaCreateContext(CanHeader* ch,int fd);
bool CannaDupContext(CanHeader* ch,int fd);
bool CannaCloseContext(CanHeader* ch,int fd);
bool CannaGetDicList(CanHeader* ch,int fd);
bool CannaGetDirList(CanHeader* ch,int fd);
bool CannaMountDic(CanHeader* ch,int fd);
bool CannaUnmountDic(CanHeader* ch,int fd);
bool CannaBeginConv(CanHeader* ch,int fd);
bool CannaGetCandiList(CanHeader* ch,int fd);
bool CannaSetAppName(CanHeader* ch,int fd);
bool CannaNoticeGroup(CanHeader* ch,int fd);
bool CannaGetStatus(CanHeader* ch,int fd);
bool CannaEndConv(CanHeader* ch,int fd);
bool CannaResizePause(CanHeader* ch,int fd);
bool CannaDefineWord(CanHeader* ch,int fd);
bool CannaRemountDic(CanHeader* ch,int fd);
bool CannaMountDicList(CanHeader* ch,int fd);
bool CannaQueryDic(CanHeader* ch,int fd);
bool CannaDeleteWord(CanHeader* ch,int fd);
bool CannaGetYomi(CanHeader* ch,int fd);
bool CannaSubstYomi(CanHeader* ch,int fd);
bool CannaStoreYomi(CanHeader* ch,int fd);
bool CannaStoreRange(CanHeader* ch,int fd);
bool CannaGetLastYomi(CanHeader* ch,int fd);
bool CannaFlushYomi(CanHeader* ch,int fd);
bool CannaRemoveYomi(CanHeader* ch,int fd);
bool CannaGetSimpleKanji(CanHeader* ch,int fd);
bool CannaGetHinshi(CanHeader* ch,int fd);
bool CannaGetLex(CanHeader* ch,int fd);
bool CannaSetLocale(CanHeader* ch,int fd);
bool CannaAutoConv(CanHeader* ch,int fd);
bool CannaQueryExt(CanHeader* ch,int fd);
bool CannaKillServer(CanHeader* ch,int fd);

bool CannaGetServerInfo(CanHeader* ch,int fd);
bool CannaGetAcl(CanHeader* ch,int fd);
bool CannaCreateDic(CanHeader* ch,int fd);
bool CannaDeleteDic(CanHeader* ch,int fd);
bool CannaRenameDic(CanHeader* ch,int fd);
bool CannaGetWordTextDic(CanHeader* ch,int fd);
bool CannaListDic(CanHeader* ch,int fd);
bool CannaSync(CanHeader* ch,int fd);
bool CannaChmodDic(CanHeader* ch,int fd);
bool CannaCopyDic(CanHeader* ch,int fd);

bool wm_wime_dialog(CanHeader* ch,int fd);
bool wm_wime_set_comp_win(CanHeader* ch,int fd);
bool wm_wime_get_comp_win(CanHeader* ch,int fd);
bool wm_wime_send_key(CanHeader* ch,int fd);
bool wm_wime_enable_ime(CanHeader* ch,int fd);
bool wm_wime_move_shadow_win(CanHeader* ch,int fd);
bool wm_wime_set_comp_font(CanHeader* ch,int fd);
bool wm_wime_get_comp_str(CanHeader* ch,int fd);
bool wm_wime_set_cand_win(CanHeader* ch,int fd);
bool wm_wime_reg_x_window(CanHeader* ch,int fd);
bool wm_wime_get_result_str(CanHeader* ch,int fd);
bool wm_wime_set_result_str(CanHeader* ch,int fd);
bool wm_wime_reconv(CanHeader* ch,int fd);
bool wm_wime_set_focus(CanHeader* ch,int fd);
bool wm_wime_show_toolbar(CanHeader* ch,int fd);
bool wm_wime_get_style_list(CanHeader* ch,int fd);
bool wm_wime_reset(CanHeader* ch,int fd);
bool wm_wime_flush_msg(CanHeader* ch,int fd);

#endif
