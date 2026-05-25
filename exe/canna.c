
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include "so/wimeapi.h"
#include "io/wimeio.h"
#include "lib/ut.h"
#include "canna.h"
#include "apisup.h"
#include "lib/list.h"
#include "lib/log.h"
#include "lib/version.h"
#if defined(__FreeBSD__)
#include "lib/freebsd.h"
#endif

bool wm_ime_composition(CannaContext_t* cx, char mj, int clstart);
extern Array InputWins;

/*
  return: true=全部処理した false=途中で中断した
*/
bool flush_msg_loop()
{
    MSG msg;
    bool st = true;
    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) {
        if (GetMessage(&msg, NULL, 0, 0) <= 0) {
            st = false;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return st;
}

/*01 初期化処理
要求パケット(Type 0)
        i32	Initialize MSBfirst固定
        i32	データ長 MSBfirst固定
        s8	バージョン番号 'メジャー番号．マイナー番号:'
        s8	ユーザ名
応答パケット(Type 0)
        u16	サーバマイナーバージョン MSBfirst
        i16	コンテクスト番号 MSBfirst
クライアントのプロトコルメジャー番号がサーバのそれより大きい場合などのエラー時には，
        i32	エラー状態 MSBfirst
                エラー時: －1/バージョンミスマッチ時:RETURN_VERSION_ERROR_STAT
以後，整数型のデータを送受信する場合のバイトオーダはMSBfirst で行わなければならない．

Initializeフィールドにはbeで1が入っている。そのままCannaHeaderにするとmajor=0,minor=0,length=1となる。
これが呼ばれた時点ではinitializeフィールドしか読み込んでいない。
s8は２つの文字列が別々にあるような記述だが、実際は１つの文字列。一つのs8として"メジャー.マイナー:ユーザー名"という文字列が来る。
 */
bool Init(CanHeader* ch, int fd)
{
    int32_t len;
    union {
        uint16_t res[2];
        uint32_t res32;
    } res;

    ImRead(&len, 4);
    len = Swap4(len);
    char data[len];
    ImRead(data, len);
    DEBUGLOG(CH_CANNA, "header %4D, data '%s', fd %d\n", ch, data, fd);

    if (FindClient(fd) != NULL) {
        //複数回の初期化
        res.res[0] = res.res[1] = -1;
    }
    else {
        int client_major, client_minor;
        int n = sscanf(data, "%d.%d", &client_major, &client_minor);
        char* user = strchr(data, ':');
        if (user != NULL)
            ++user;
        if (n != 2 || client_major > WIME_CANNA_MAJOR || user == NULL) {
            /* 送られたデータがおかしい
               メジャーバージョンがあわないときはRETURN_VERSION_ERROR_STATを返すことになっているが、これはかんなソースのIRproto.hにあり、libcannaには含まれていない。これのためだけにかんなソースを持ってくるのも面倒なので、ヘッダファイルに書いておく。
               マイナーバージョンはとりあえず無視する。
               ユーザー名がないのは別のエラーにするべきだが面倒なので。
            */
            res.res32 = Swap4(RETURN_VERSION_ERROR_STAT);
            FATALLOG(CH_CANNA, "illegal data\n");
        }
        else {
            res.res[0] = Swap2(WIME_CANNA_MINOR);
            res.res[1] = Swap2(OpenConnection(fd, user));
            DEBUGLOG(CH_CANNA, "context %hd, fd %d, user '%s'\n", Swap2(res.res[1]), fd, user);
        }
    }
    return ImWrite(&res, 4);
}

/*02 終了処理
要求パケット(Type 1)
応答パケット(Type 2)
        i8	終了状態	正常時: 0 / エラー時: －1
 */
bool Finalize(CanHeader* ch, int fd)
{
    char st = (CloseConnection(fd) ? 0 : -1);
    DEBUGLOG(CH_CANNA, "fd %d, status %hhd\n", fd, st);
    return Reply2(ch->Major, ch->Minor, st);
}

/*03 コンテクスト作成
  要求パケット(Type 1)
  応答パケット(Type 5)
        i16	コンテクスト番号 エラー時: －1

コンテキスト番号は０からだが、連番とは限らない。
 */
bool CreateContext(CanHeader* ch, int fd)
{
    int16_t cxn;
    if (OpenCannaContext(fd, &cxn) == NULL)
        cxn = -1;
    DEBUGLOG(CH_CANNA, "context %hd, fd %d\n", cxn, fd);
    return Reply5(ch->Major, ch->Minor, cxn);
}

/*04 コンテクスト複写:指定された変換コンテクストを複製し，新しい変換コンテクストを生成しそれを表すコンテクスト番号を返す．
  要求パケット(Type 2)
        i16	コンテクスト番号
  応答パケット(Type 5)
        i16	コンテクスト番号 エラー時: －1
 */
bool DupContext(CanHeader* ch, int fd)
{
    int16_t dstn = -1;
    int16_t srcn = Req2(ch);
    DEBUGLOG(CH_CANNA, "context %hd, fd %d\n", srcn, fd);
    if (ValidContext(srcn, __FUNCTION__) != NULL) {
        DupWinParam params;
        CannaContext_t* cxd = OpenCannaContext(fd, &dstn);
        CannaContext_t* cxs = ValidContext(srcn, __FUNCTION__); //cxdを作った後で改めてアドレスを得る
        SetWinParam(cxd->Win, GetWinParam(cxs->Win, &params));
        ArCopy(&cxd->Dics, &cxs->Dics);
        ArCopy(&cxd->DicMode, &cxs->DicMode);

        DEBUGLOG(CH_CANNA, "%hd --> %hd\n", srcn, dstn);
    }
    return Reply5(ch->Major, ch->Minor, dstn);
}

/*05 コンテクスト削除
  要求パケット(Type 2)
        i16	コンテクスト番号
  応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
 */
bool CloseContext(CanHeader* ch, int fd UNUSED)
{
    char st = -1;
    int16_t cxn = Req2(ch);
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL) {
        CloseCannaContext(cx);
        st = 0;
    }
    return Reply2(ch->Major, ch->Minor, st);
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
/*06 辞書テーブル一覧 [atok]
辞書テーブル(辞書リスト(マウントリスト)に登録可能な辞書群)に登録されている辞書一覧を取得する．
要求パケット(Type 3)
        i16	コンテクスト番号
        u16	辞書名リストのバッファサイズ
応答パケット(Type 6)
        i16	辞書数  エラー時: －1
        s8	辞書名リスト '辞書名@...@辞書名@@'
 */
bool GetDicList(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    Req3(ch, &cxn, &bufsize);
    DEBUGLOG(CH_CANNA, "context %hd, bufsize %hd\n", cxn, bufsize);
    DEBUGLOG(CH_CANNA, "*** NOT IMPLIMENT *** I DO NOTHING ***\n");
    return Reply6(ch->Major, ch->Minor, 0, NULL, 0); //リストなしで正常終了
}

/*07 辞書ディレクトリ一覧 [atok]
辞書ディレクトリ(辞書テーブル(dics.dir)を持ったディレクトリ)にある辞書の一覧を取得する．
要求パケット(Type 3)
        i16	コンテクスト番号
        u16	バッファサイズ
応答パケット(Type 6)
        i16	辞書数  エラー時: －1
        s8	辞書ディレクトリ名リスト  '辞書名@...@ 辞書名@@'
 */
bool GetDirList(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    Req3(ch, &cxn, &bufsize);
    DEBUGLOG(CH_CANNA, "context %hd, bufsize %hd\n", cxn, bufsize);
    DEBUGLOG(CH_CANNA, "*** NOT IMPLIMENT *** I DO NOTHING ***\n");
    return Reply6(ch->Major, ch->Minor, 0, NULL, 0);
}

/*08 辞書リスト追加  指定された辞書をかな漢字変換で利用されるようにする。
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	辞書名 '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 /  エラー時: －1
辞書名を記録するだけ
 */
bool MountDic(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char st = -1;

    char* name = Req15(ch, &mode, &cxn);
    DEBUGDO(CH_CANNA, { MESG("mode 0x%x, context %hd, dic-name '%s'\n",mode,cxn,name);
            MESG("*** I DO NOTHING ***\n"); });
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL) {
        if (cx->Dics.use > 0)
            --cx->Dics.use; //リストの終了マークをとる
        ArAddN(&cx->Dics, name, strlen(name) + 1);
        ArAddChar(&cx->Dics, 0);
        ArAdd1(&cx->DicMode, &mode);
        st = 0;
    }
    return Reply2(ch->Major, ch->Minor, st);
}

/*09 辞書リスト削除:指定された辞書がかな漢字変換で利用されないようにする。
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	辞書名 '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
辞書名を削除するだけ
 */
bool UnmountDic(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char st = -1;
    Array dicname;

    ArNew(&dicname, 1, NULL);
    ArPrint(&dicname, "%s", Req15(ch, &mode, &cxn));
    DEBUGDO(CH_CANNA, { MESG("mode 0x%x, context %hd, dic-name '%s'\n",mode,cxn,(char*)ArAdr(&dicname));
            MESG("*** I DO NOTHING ***\n"); });
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL) {
        int dn = ListFind(&cx->Dics, &dicname);
        if (dn > 0) {
            ListRemove(&cx->Dics, dn);
            ArRemove(&cx->DicMode, dn, 1);
            st = 0;
        }
        else
            INFOLOG(CH_CANNA, "not found dictionary '%s'\n", (char*)ArAdr(&dicname));
    }
    ArDelete(&dicname);
    return Reply2(ch->Major, ch->Minor, st);
}

/*0a 辞書リスト変更:使用辞書の辞書リストの順番を変更する．
要求パケット(Type 15)
        i32	優先度
        i16	コンテクスト番号
        s8	辞書名 '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
???  唐突に優先度が使われるが、これはどういう数値なのか？
  よくわからないので、指定された辞書をリストの先頭に持ってくる。
 */
bool RemountDic(CanHeader* ch, int fd UNUSED)
{
    int32_t pr;
    int16_t cxn;
    char st = -1;
    Array dicname;

    ArNew(&dicname, 1, NULL);
    ArPrint(&dicname, "%s", Req15(ch, &pr, &cxn));
    DEBUGDO(CH_CANNA, { MESG("context %hd, priority %d, dic-name '%s'\n",cxn,pr,(char*)ArAdr(&dicname));
            MESG("*** I DO NOTHING ***\n"); });
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL) {
        int dn = ListFind(&cx->Dics, &dicname);
        if (dn >= 0) {
            int mode = *(int32_t*)ArElem(&cx->DicMode, dn);
            ArInsert(ArRemove(&cx->DicMode, dn, 1), 0, 1, &mode);
            ListRemove(&cx->Dics, dn);
            ListInsert(&cx->Dics, 0, &dicname);
            st = 0;
        }
        else
            INFOLOG(CH_CANNA, "not mount dictionary\n");
    }
    ArDelete(&dicname);
    return Reply2(ch->Major, ch->Minor, st);
}

/*0b 辞書リスト一覧:辞書テーブルに登録されている辞書リスト
要求パケット(Type 3)
        i16	コンテクスト番号
        u16	辞書名リストのバッファサイズ
応答パケット(Type 6)
        i16	辞書数  エラー時: －1
        s8	辞書名リスト  '辞書名@...@ 辞書名@@'
*/
bool MountDicList(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    int n = -1, len = 0;
    char* p = NULL;

    Req3(ch, &cxn, &bufsize);
    DEBUGLOG(CH_CANNA, "context %hd, buffer size %hd\n", cxn, bufsize);
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL && (len = ArUsing(&cx->Dics)) <= bufsize) {
        p = ArAdr(&cx->Dics);
        n = ListCount(&cx->Dics);
    }
    return Reply6(ch->Major, ch->Minor, n, p, len);
}

/*0c 登録可能辞書の問い合わせ:指定した辞書の情報を得る
要求パケット(Type 19)
        i32	モード(0)
        i16	コンテクスト番号
        s8	ユーザ名  'ユーザ名@'
        s8	辞書名  '辞書名@'
応答パケット(Type 10)
        i8	終了状態  正常時: 0 / エラー時: －1
        s8	辞書名 '辞書名@'
        s8	辞書ファイル名 '辞書ファイル名@'
        i32[]	辞書情報  ???28バイト?(struct DicInfoが28バイトらしい)
*/
bool QueryDic(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* user;
    char* dic = Req19(ch, &mode, &cxn, &user);
    DEBUGLOG(CH_CANNA, "context %hd, mode 0x%x, user '%s', dic '%s'\n", cxn, mode, user, dic);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    return Reply2(ch->Major, ch->Minor, -1);
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

    if ((hc = WimeData.HinshiTab) == NULL) {
        ERRORLOG(CH_GLOBAL, "not found hinshi table\n");
        return 0;
    }
    for (; hc->Ccode != NULL; ++hc) {
        if (regcomp(&reg, hc->Ccode, REG_EXTENDED) == 0) {
            regmatch_t m;
            if (regexec(&reg, can_code, 1, &m, 0) == 0)
                break;
            regfree(&reg);
        }
    }
    if (hc->Ccode != NULL)
        regfree(&reg);
    else {
        ERRORLOG(CH_GLOBAL, "unknown part code:%s\n", can_code);
        hc = WimeData.HinshiTab; //先頭にあるコードを返す
    }
    return hc->Wcode;
}

/*
  単語情報は'読み 品詞コード 登録文字列'
  ??? 区切りはスペースだけか？ 登録文字列は１つだけとしていいのか？
 */
bool reg_or_unreg_word(CanHeader* ch, BOOL WINAPI(*proc)(HKL, LPCWSTR, DWORD, LPCWSTR))
{
    int16_t cxn;
    uint16_t* wordrec;
    bool st = true;

    char* dicname = Req12((Req12_t*)ch, &cxn, &wordrec);
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    FromClientToU16(cx, wordrec);
    DEBUGLOG(CH_CANNA, "context %hd, wordinfo '%W', dic-name '%s'\n", cxn, wordrec, dicname);

    uint16_t* yomi = U16Tok(&wordrec);
    char* hinshi = U16ToEj(NULL, NULL, U16Tok(&wordrec), -1); //品詞コード文字列
    int sty = canna_hinshi_to_win(hinshi + 1);
    if (sty == 0) { //品詞テーブルがない
        ERRORLOG(CH_CANNA, "not exist hinshi table\n");
        st = false;
    }
    else {
        uint16_t* word = U16Tok(&wordrec); //登録する漢字
        DEBUGLOG(CH_CANNA, "reading [%W], style 0x%x, word [%W]\n", yomi, sty, word);
        if (!proc(GetKeyboardLayout(0), (LPCWSTR)yomi, sty, (LPCWSTR)word)) {
            ERRORLOG(CH_CANNA, "fail Imm(Un)RegisterWordW\n");
            st = false;
        }
    }
    free(hinshi);
    return Reply2(ch->Major, ch->Minor, (st ? 0 : -1));
}

/*0d 単語登録:辞書に新しい単語を登録する
要求パケット(Type 12)
        i16	コンテクスト番号
        s16	単語情報 '文字列@@'
        s8	辞書名 '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
*/
bool DefineWord(CanHeader* ch, int fd UNUSED)
{
    DEBUGLOG(CH_CANNA, "\n"); //関数名だけ表示
    return reg_or_unreg_word(ch, ImmRegisterWordW);
}

/*0e 単語削除:辞書から単語を削除する．
要求パケット(Type 12)
        i16	コンテクスト番号
        s16	単語情報 '文字列@@'
        s8	辞書名 '辞書名@'
応答パケット(Type 2)
        i8	終了状態 正常時: 0 / エラー時: －1
*/
bool DeleteWord(CanHeader* ch, int fd UNUSED)
{
    DEBUGLOG(CH_CANNA, "\n"); //関数名だけ表示
    return reg_or_unreg_word(ch, ImmUnregisterWordW);
}

bool set_yomi_str(CannaContext_t* cx, HIMC imc, unsigned sentence_mode, unsigned notify_cmd, uint16_t* yomi, int32_t fer_mode)
{
#ifdef SETCONTEXT_FAIL
    SetCurrentImc(imc, TRUE);
#else
    ImmSetOpenStatus(imc, TRUE);
    ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0); //きちんと終了処理をしない場合新しいHWNDでも以前のデータが残っているときがある。
#endif

    /* ???
       '￣'(FULLWIDTH MACRON)としてe-a1b1をu16にするとU-ffe3になるが、これを読み文字列にすると
       ImmNotifyIME()が失敗する。これがあれば'~'に書き換える。
       '＼'(e-a1c0,u-ff3c)も同様だった。なぜだろう？
    */
    for (uint16_t* p = yomi; *p != 0; ++p) {
        switch (*p) {
        case 0xffe3: *p = '~'; break;
        case 0xff3c: *p = '\\'; break;
        }
    }
    int r = ImmSetCompositionStringW(imc, SCS_SETSTR, yomi, WcLen(yomi) * 2, NULL, 0);

    if (r) {
        if ((r = ImmNotifyIME(imc, NI_COMPOSITIONSTR, notify_cmd, 0))) {
            cx->FerMode = fer_mode;
            cx->Conv = 0;
        }
        else
            INFOLOG(CH_CANNA, "fail ImmNotifyIME()\n");
    }
    else
        ERRORLOG(CH_CANNA, "fail ImmSetCompositionStringW()\n");
    return r != 0;
}

/*0f 変換開始:読みのかな文字列に対し，連文節変換モードでかな漢字変換を行う．
要求パケット(Type 14)
        i32	モード  RkwBgnBunのmode
        i16	コンテクスト番号
        s16	読み '文字列@@'
応答パケット(Type 7)
        i16	文節数  エラー時: －1
        s16	各文節の最優先候補のリスト
*/
bool BeginConv(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    bool r = false;
    HIMC imc;

    uint16_t* yomi = Req14(ch, &mode, &cxn);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        FromClientToU16(cx, yomi);
        DEBUGLOG(CH_CANNA, "mode 0x%x, context %hd, '%W' [%*.2D] u16 %d fd %d\n", mode, cxn, yomi, WcLen(yomi), yomi, ((cx->Flags & USE_UTF16) != 0), fd);
        r = set_yomi_str(cx, imc, IME_SMODE_PHRASEPREDICT, CPS_CONVERT, yomi, mode);
        ImmReleaseContext(cx->Win, imc);
    }
    else {
        DEBUGLOG(CH_CANNA, "context %hd NOT FOUND [%*.2D]\n", cxn, WcLen(yomi), yomi);
    }
    return r ? wm_ime_composition(cx, ch->Major, 0) : (cx->Conv = -1, Reply5(ch->Major, ch->Minor, -1));
}

//変換モードで追加される候補の数を返す
//append_fer_cand()も参照
int fer_mode_num(int32_t mode)
{
    int count;
    for (count = 0; (mode & RK_XFERMASK) != 0; mode >>= RK_XFERBITS) {
        //モード0x0fは除外する。 ???これはなんだろう
        if ((mode & RK_XFERMASK) != RK_XFERMASK)
            ++count;
    }
    return count;
}

/*
  現在の変換結果のリストを作る。リスト終了のヌル文字を追加する。
  文字コードはu16
  戻り値：文節数
*/
int current_cand_list(int clstart, Array* lst, const CannaContext_t* cx, HIMC imc)
{
    int st = cx->FixedNum; //ResizePauseに返す文節数は注目文節以降ではなく全文節数
    Array u16;

    //ArDec(ArCopy(lst,&cx->FixedStr)); //リスト終了マークを削除する。

    ArNew(&u16, 2, NULL);
    for (; ClauseStr(imc, cx, GCS_COMPSTR, clstart, clstart + 1, ArClear(&u16), false) != NULL; ++clstart) {
        char at = GetAttr(imc, clstart, cx);
        if (at != ATTR_TARGET_CONVERTED && at != ATTR_CONVERTED && at != ATTR_FIXEDCONVERTED)
            break; //まだ変換されてなければそこで終わる
        ArAddAr(lst, &u16);
        ++st; //有効な文節の数を数える
    }
    if (st > 0) {
        ArAdd1(lst, &(uint16_t){0}); //リスト終了マーク
    }
    ArDelete(&u16);
    return st;
}

void dump_cand_list(int num, const Array* ws)
{
    uint16_t* str;
    for (int index = 0; (str = ListInc(ws, index)) != NULL; ++index) {
        MESG("'%W' [%*D]\n", str, WcLen(str) * 2, str);
    }
    MESG("list=%d\n", num);
}

/*
  begin_convert,resize_pauseの続き
  WM_IME_COMPOSITIONの処理
  Context[cx].Convに注目文節番号

  ??? 1.5.1までは回ってきたメッセージを捕まえて処理していたが、1.6.0では変換処理関数を
  呼んだ後メッセージループに行かずに直接これを呼ぶことにした。atok08では問題なさそうだが、
  ほかのimeではどうだろう？ メッセージが送られるタイミングは変換処理がすべて終わった後として
  かまわないのか？
  特に自動変換の場合、明示的に変換をしているわけではない。キーを送った後すぐにこの関数を呼んでいるが、大丈夫だろうか。文字処理が非同期に行われれば問題が起こりそうな気がする。
  しばらくこれでやってみて、おかしければ元に戻そう。

  clstart=候補リストを作る先頭文節の番号。
*/
bool wm_ime_composition(CannaContext_t* cx, char mj, int clstart)
{
    HIMC imc = ImmGetContext(cx->Win);
    DEBUGDO(CH_CANNA, { MESG("major code 0x%hhx, target clause number %d\n",mj,cx->Conv);
            DbgComp(imc,__func__); });

    SaveFixedClause(imc, cx); //変換が起こるたびに固定文節情報は上書きされてしまうので保存する。

    Array candlist;
    int st = current_cand_list(clstart, ArNew(&candlist, 2, NULL), cx, imc);
    DEBUGDO(CH_CANNA, dump_cand_list(st ? st + clstart : st, & candlist));
    /*ResizePauseで返す候補リストはカレント文節からだが、文節数は先頭文節から最終文節まで。
      current_cand_listが返す値はカレント文節以降の文節数なので、clstartを加算して全文節数にする。 */
    if (st > 0) {
        FromU16ToClient(cx, &candlist);
        st += clstart;
    }
    bool ret = Reply7(mj, 0, st, ArAdr(&candlist), ArUsing(&candlist));
    cx->Conv = -1;
    ImmReleaseContext(cx->Win, imc);
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
int page_index(int cln, Array* candlistpage, int index, int* page)
{
    if (index > 0) { //先頭候補以外
        if (cln >= candlistpage->use) {
            *page = CANDLISTMAX; //この文節で候補一覧は出していない
        }
        else {
            CandListPageInfo* pi = ArElem(candlistpage, cln);
            if (pi->Seq > 0)
                *page = -1; //候補ウィンドウなし
            else {
                for (*page = 0; *page < CANDLISTMAX; ++ * page) {
                    if (pi->Size[*page] == 0) { //候補リストがない？
                        MESG("clause %d:candidate list page %d is none\n", cln, *page);
                        index = -1;
                        break;
                    }
                    if (index < pi->Size[*page])
                        break;
                    index -= pi->Size[*page];
                }
            }
        }
        if (*page == CANDLISTMAX) {
            DEBUGLOG(CH_CANNA, "clause %d:candidate page not found\n", cln);
            index = -1;
        }
    }
    else {
        DEBUGLOG(CH_CANNA, "clause %d:first candidate word\n", cln);
        index = -1;
    }
    DEBUGLOG(CH_CANNA, "clause %d --> page %d,index %d\n", cln, *page, index);
    return index;
}

//最終的な変換候補をimeに反映させる
void update_cand(HIMC imc, const int16_t* candnum, int len, Array* pi, const CannaContext_t* cx)
{
    for (int clnum = cx->FixedNum; clnum < len; ++clnum, ++candnum) {
        int cn, page;
        if ((cn = page_index(clnum, pi, *candnum, &page)) >= 0) {
            switch (SetTarget(imc, clnum, cx)) {
            case ChangeTargetSuccess:
                if (page >= 0) {
                    if (ImmNotifyIME(imc, NI_OPENCANDIDATE, page, 0) &&
                        ImmNotifyIME(imc, NI_SELECTCANDIDATESTR, page, cn + WimeData.CandIndexStart)) {
                        DEBUGLOG(CH_CANNA, "candidate page %d, index %d\n", page, cn);
                    }
                    else
                        INFOLOG(CH_CANNA, "fail ImmNotifyIME\n");
                }
                else {
                    while (--cn >= 0) {
                        DEBUGLOG(CH_CANNA, "cand loop counter %d:\n", cn);
                        ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
                    }
                }
            case ChangeTargetFixed:
                break;
            case ChangeTargetFail:
                ERRORLOG(CH_CANNA, "fail SetTarget\n");
            }
        }
    }
}

/*10 変換終了:現在のかな漢字変換作業を終了し，必要に応じて学習を行う．
要求パケット(Type 10)
        i16	コンテクスト番号
        i16	文節数
        i32	モード  0なら学習しない  RkwEndBunのmode
        i16[]	各文節のカレント候補番号リスト
応答パケット(Type 2)
        i8	終了状態  正常時: 0/エラー時: －1
文節数が０なら現在の候補で確定する。
*/
bool EndConv(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, clses;
    int32_t mode;
    char st = -1;
    HIMC imc;

    int16_t* candnums = Req10((Req10_t*)ch, &cxn, &clses, &mode);
    DEBUGLOG(CH_CANNA, "context %hd, clauses %hd, mode %d, candidate list {%#*.2D}\n", cxn, clses, mode, (int)clses, candnums);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        //??? mode!=1はキャンセルとしていいのか？
        if (mode == 1) {
            update_cand(imc, candnums, clses, &cx->CandInfo, cx);
            st = ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
            if (st)
                DEBUGDO(CH_CANNA, DbgComp(imc, __FUNCTION__));
        }
        else {
            ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_REVERT, 0);
            st = ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
        }
        if (!st)
            INFOLOG(CH_CANNA, "fail ImmNotifyIME\n");
        ImmNotifyIME(imc, NI_CLOSECANDIDATE, 0, 0); //???各文節に必要か？
#ifdef SETCONTEXT_FAIL
        SetCurrentImc(imc, FALSE);
#else
        ImmSetOpenStatus(imc, FALSE);
#endif
        ImmReleaseContext(cx->Win, imc);

        //ResetContext()でフラグが全部クリアされてしまうがUSE_UTF16は残しておく。
        unsigned save_flag = (cx->Flags & USE_UTF16);
        ResetContext(cx)->Flags |= save_flag;
        st = 0;
    }
    return Reply2(ch->Major, ch->Minor, st);
}

/*
  モードにしたがってyomi(u16,半角カナ)を変換して候補リストに追加する。
  追加した数を返す。
  ??? モードが0xf(たぶんRK_CTRLHENKAN)なのは何だ？
*/
int append_fer_cand(int mode, Array* lb, const Array* yomi)
{
    int count = 0;
    for (; (mode & RK_XFERMASK) != 0; mode >>= RK_XFERBITS) {
        uint16_t* zk = NULL;
        switch (mode & RK_XFERMASK) {
        case RK_HFER: //半角文字
            DEBUGLOG(CH_CANNA, "Hankaku\n");
            ArAddAr(lb, yomi);
            break;
        case RK_KFER: //カタカナ
            DEBUGLOG(CH_CANNA, "Katakana\n");
            zk = U16HanToZenKata(NULL, NULL, ArAdrC(yomi), -1);
            ArAddN(lb, zk, WcLen(zk) + 1);
            break;
        case RK_XFER: //ひらがな ??? RK_ZFERとの違いは？
            DEBUGLOG(CH_CANNA, "Hiragana\n");
            zk = U16HanToZenHira(NULL, NULL, ArAdrC(yomi), -1);
            ArAddN(lb, zk, WcLen(zk) + 1);
            break;
        case RK_ZFER: //全角文字
            DEBUGLOG(CH_CANNA, "Zenkaku\n");
            zk = U16HanToZenHira(NULL, NULL, ArAdrC(yomi), -1);
            ArAddN(lb, zk, WcLen(zk) + 1);
            break;
        default:
            DEBUGLOG(CH_CANNA, "unknown mode %d(0x%x)\n", mode & RK_XFERMASK, mode & RK_XFERMASK);

        }
        ++count;
        free(zk);
    }
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
*/
void GetCandidateAtok(HIMC imc, const CannaContext_t* cx, Array* clist, int clnum, unsigned listnum, CANDIDATELIST* cb)
{
    for (unsigned cannum = 0; cannum < cb->dwCount; ++cannum) {
        if (!ImmNotifyIME(imc, NI_SELECTCANDIDATESTR, listnum, cannum + WimeData.CandIndexStart)) {
            INFOLOG(CH_CANNA, "fail ImmNotifyIME(NI_SELECTCANDIDATESTR)\n");
            break;
        }
        ClauseStr(imc, cx, GCS_COMPSTR, clnum, clnum + 1, clist, false);
    }
}
/* cbから候補リスト(u16)を作る。リストの終了マークはつかない。
   clist==NULLのときは何も返さない。
*/
void GetCandidateW(HIMC /*imc*/, const CannaContext_t* /*cx*/, Array* clist, int /*clnum*/, unsigned /*listnum*/, CANDIDATELIST* cb)
{
    for (unsigned cannum = 0; cannum < cb->dwCount; ++cannum) {
        uint16_t* u16 = (uint16_t*)((char*)cb + cb->dwOffset[cannum]);
        ArAddN(clist, u16, WcLen(u16) + 1); //u16のままclistにいれる。
    }
}

/*
  変換候補をu16にしてリストに追加する。リスト終了マークはつかない。
  clistのblocksize:2(wchar) NULLのときは何も返さない。
  リストの数(0～)を返す。
*/
int lookup_cand_win(HIMC imc, Array* clist, CandListPageInfo* pi, int clnum, const CannaContext_t* cx)
{
    Array candpage;
    ArNew(&candpage, 1, NULL);
    int cand_count = 0;
    unsigned listnum = 0;
    do {
        unsigned n;
        if ((n = ImmGetCandidateListW(imc, listnum, NULL, 0)) == 0) {
            DEBUGLOG(CH_CANNA, "page %d has no candidate list\n", listnum);
            break;
        }
        DEBUGLOG(CH_CANNA, "ImmGetCandidateList:page %d, size %u\n", listnum, n);
        ArAlloc(&candpage, n);
        ImmGetCandidateListW(imc, listnum, ArAdr(&candpage), ArUsingBytes(&candpage));

        CANDIDATELIST* cb = ArAdr(&candpage);
        cand_count += cb->dwCount;
        pi->Size[listnum] = cb->dwCount;
        (*WimeData.GetCandidate)(imc, cx, clist, clnum, listnum, cb);
    } while (++listnum < CANDLISTMAX && ImmNotifyIME(imc, NI_CHANGECANDIDATELIST, listnum, 0));

#if 1
    //第１候補に戻す
    if (listnum > 0)
        ImmNotifyIME(imc, NI_CHANGECANDIDATELIST, 0, 0);
    if (cand_count > 0)
        ImmNotifyIME(imc, NI_SELECTCANDIDATESTR, 0, WimeData.CandIndexStart);
#else
    ImmNotifyIME(imc, NI_CLOSECANDIDATE, 0, 0);
    ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
    //??? 未確定に戻ってしまうので、再度変換する。なんで？
#endif

    ArDelete(&candpage);
    return cand_count;
}

//[3.4.4,r9] 現在選択されている候補の番号(0から)を返す。
int cand_index(HIMC imc)
{
    int idx = -1;
    int sz = ImmGetCandidateList(imc, 0, NULL, 0);
    if (sz != 0) {
        Array candpage;
        ArNew(&candpage, 1, NULL);
        CANDIDATELIST* cb = ArAlloc(&candpage, sz);
        ImmGetCandidateList(imc, 0, cb, ArUsingBytes(&candpage));
        idx = cb->dwSelection;
        ArDelete(&candpage);
    }
    return idx;
}

/*
  変換候補をu16にしてリストに追加する。リスト終了マークはつかない。
  euclistのblocksize:2(wchar) NULLのときは何も返さない。
  リストの数(0～)を返す。
*/
int make_cand_list(HIMC imc, Array* clist, CandListPageInfo* pi, int clnum, CannaContext_t* cx)
{
    int count = 0;
    bool open_cand_win;

    /*!!!
      候補数≦"候補ウィンドウ表示までに必要な変換回数"のとき、ImmGetCandidateListは０を返す。ImmNotifyIMEでNI_OPENCANDIDATEを指定しても候補ウィンドウは表示されない。なんともならないので、imcは１回目の変換状態であるとして、未変換状態になるまでImmNotifyIMEで変換しそのたびに変換結果を記録する。
      この方法で変換候補リストを作った場合、候補へのランダムアクセスができない。２０番目の候補で確定したら再度２０回変換しなければならない。なので、変換したらメッセージを調べ、WM_IME_NOTIFY(IMN_OPENCANDIDATE)が来たら処理をやめてこれまでと同じやり方で変換候補を取得する。
      全ての文節で最終候補を選択する、という状況はまず無いだろうし、この方法だけにすれば処理は簡単だし(これまでの方法でも結局全候補を選択している)、CandListPageInfoも必要なくなるのだが。どうするか？
      uimは何かするたびに全文節の候補を調べるんだった。めちゃくちゃ遅くなるな。だめか。
      [3.4.4,r9]ImmNotifyIMEで変換していって一周したとき属性が未変換に戻らないときがある(send_keyで変換しているときか?)。現在の候補の番号が0に戻ったどうかを調べることにする。
    */
    do {
        ClauseStr(imc, cx, GCS_COMPSTR, clnum, clnum + 1, clist, false);
        //DEBUGLOG(CH_CANNA,"cand:%W\n",ArAdr(&u16));
        ++count;
        cx->Flags &= ~CATCH_OPEN_CAND;
        ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
        flush_msg_loop();
        open_cand_win = ((cx->Flags & CATCH_OPEN_CAND) != 0);
    } while (!open_cand_win &&
        GetAttr(imc, clnum, cx) != ATTR_TARGET_NOTCONVERTED &&
        !(count > 1 && cand_index(imc) == 0));

    if (open_cand_win) {
        //リセットしてやり直し
        DEBUGLOG(CH_CANNA, "retry call lookup_cand_win()\n");
        count = lookup_cand_win(imc, ArClear(clist), pi, clnum, cx);
    }
    else {
        //１回目の変換状態に戻す
        if (GetAttr(imc, clnum, cx) == ATTR_TARGET_NOTCONVERTED) {
            DEBUGLOG(CH_CANNA, "reset first condition\n");
            ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
        }
        else {
            DEBUGLOG(CH_CANNA, "already first condition,index %d\n", cand_index(imc));
        }
        pi->Seq = count;
    }
    return count;
}

static inline Array* han2zenhira(Array* str) {
    return ArSetUsing(str, WcLen(U16HanToZenHira((uint16_t*)ArAdr(str), NULL, (uint16_t*)ArAdr(str), -1)) + 1);
}

/*11 候補要求:指定された文節のすべての候補文字列と読みを取得する．
要求パケット(Type 6)
        i16	コンテクスト番号
        i16	カレント文節番号
        u16	バッファサイズ
応答パケット(Type 7)
        i16	候補数  エラー時: －1
        s16	候補文字列のリスト+読み '候補@@...@@候補@@読み@@@@'
返される候補数は (変換候補の数+modeによる読みの数)。最後に追加される読みは含まれていない。
*/
bool GetCandiList(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, cls_num, cand_count = -1;
    uint16_t bufsize;
    Array clist;
    HIMC imc;

    Req6(ch, &cxn, &cls_num, &bufsize);
    DEBUGLOG(CH_CANNA, "context %hd, clause-number %hd, buffer size %hu\n", cxn, cls_num, bufsize);
    ArNew(&clist, 2, NULL);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        switch (SetTarget(imc, cls_num, cx)) { //注目文節を変更
        case ChangeTargetSuccess:
            if (cls_num < ArUsing(&cx->CandInfo))
                //念のため以前の情報は消しておく
                *(CandListPageInfo*)ArElem(&cx->CandInfo, cls_num) = (CandListPageInfo){ 0 };
            else
                ArAlloc(&cx->CandInfo, cls_num + 1); //この文節までは確保する

            /*[r11]send_keyで変換されているときはすでに何番目かの候補になっているので、最初の候補に戻す。*/
            if (cx->Flags & CATCH_OPEN_CAND) {
                cx->Flags &= ~CATCH_OPEN_CAND; //???memo0222
                ImmNotifyIME(imc, NI_SELECTCANDIDATESTR, 0, WimeData.CandIndexStart);
            }
            cand_count = make_cand_list(imc, &clist, ArElem(&cx->CandInfo, cls_num), cls_num, cx);
#if 0
            //[1.8.5]読みが候補になるのでcand_countが0になることは無いと思う
            if (cand_count == 0) {
                /* 変換結果以外に候補がなければ変換結果を入れる */
                ClauseStr(imc, cx, GCS_COMPSTR, cls_num, cls_num + 1, &clist, false);
                ++cand_count;
            }
#endif
            //読みを追加
            Array u16;
            ClauseStr(imc, cx, GCS_COMPREADSTR, cls_num, cls_num + 1, ArNew(&u16, 2, NULL), false);
            cand_count += append_fer_cand(cx->FerMode, &clist, &u16); //モードにしたがって候補リストに追加する
            ArAddAr(&clist, han2zenhira(&u16)); //読みを追加 ???これは候補数にカウントしないのか？
            ArAdd1(&clist, &(uint16_t){0});	//リスト終了を示すヌル文字
            ArDelete(&u16);

            DEBUGDO(CH_CANNA, dump_cand_list(cand_count, &clist));
            FromU16ToClient(cx, &clist); //リスト全体を一気に変換する。

            if (ArUsingBytes(&clist) > bufsize) {
                ERRORLOG(CH_CANNA, "bufsize too small,need %d\n", ArUsingBytes(&clist));
#if 0
                ArClear(&euclist);
                cand_count = -1;
                /*???バッファアドレスを渡されたわけでもないのにバッファサイズを確認することに意味あるのか?
                  [180222]候補数が多いとき渡されたバッファサイズ以上の大きさになるときがあるが、
                  これでエラーを返すとuimは固まってしまう。エラーにすべきなのかもよく分からないが、
                  uimはバッファサイズ以上のデータを返しても動くようなので、ログだけにしておく。
                */
#endif
            }
            break;
        case ChangeTargetFixed:
            DEBUGLOG(CH_CANNA, "this clause is fixed\n");
            break;
        case ChangeTargetFail:
            INFOLOG(CH_CANNA, "fail SetTarget\n");
        }
        ImmReleaseContext(cx->Win, imc);
    }
    bool ret = Reply7(ch->Major, ch->Minor, cand_count, ArAdr(&clist), ArUsing(&clist));
    ArDelete(&clist);
    return ret;
}

/*12 読みがな取得:カレント文節の読みがなを取得する．
要求パケット(Type 6)
        i16	コンテクスト番号
        i16	カレント文節番号
        u16	読みのバッファサイズ
応答パケット(Type 7)
        i16	読みの長さ  エラー時: －1
        s16	読み  '文字列@@'
*/
bool GetYomi(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, cln;
    uint16_t bufsize;
    bool st = false;
    Array yomi;
    HIMC imc;

    ArNew(&yomi, 2, NULL);
    Req6(ch, &cxn, &cln, &bufsize);
    DEBUGLOG(CH_CANNA, "context %hd, clause %hd, bufsize %hd\n", cxn, cln, bufsize);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        if (ClauseStr(imc, cx, GCS_COMPREADSTR, cln, cln + 1, &yomi, true) != NULL) {
            if (ArUsingBytes(&yomi) <= bufsize) {
                DEBUGLOG(CH_CANNA, "yomi:[%W] %d\n", ArAdr(&yomi), ArUsing(&yomi) - 1);
                st = true;
            }
            else {
                INFOLOG(CH_CANNA, "buffer too small\n");
            }
            FromU16ToClient(cx, &yomi);
        }
        else {
            DEBUGLOG(CH_CANNA, "fail GetClause\n");
        }
        ImmReleaseContext(cx->Win, imc);
    }
    st = Reply7(ch->Major, ch->Minor, (st ? ArUsing(&yomi) - 1 : -1), ArAdr(&yomi), ArUsing(&yomi));
    ArDelete(&yomi);
    return st;
}

//シフトキーの状態をセットする
void set_state(unsigned char state, uint16_t vk, unsigned convmode)
{
    if ((vk & 0xff00) != 0 || (convmode & IME_CMODE_ROMAN) == 0) {
        unsigned char keytab[256];
        GetKeyboardState(keytab);

        if ((vk & VKMODKEY(WINMODKEY_SHIFT)) != 0)
            keytab[VK_SHIFT] = state;
        if ((vk & VKMODKEY(WINMODKEY_CTRL)) != 0)
            keytab[VK_CONTROL] = state;
        if ((vk & VKMODKEY(WINMODKEY_ALT)) != 0)
            keytab[VK_MENU] = state;
        if ((vk & VKMODKEY(WINMODKEY_LOCK)) != 0)
            keytab[VK_CAPITAL] = state ? 1 : 0; //??? VK_OEM_ATTNだろうか？
        if ((convmode & IME_CMODE_ROMAN) == 0)
            keytab[VK_KANA] = state ? 1 : 0;

        SetKeyboardState(keytab);
    }
}

#define KEYUP (1<<31)

/*??? '>','?'などシフトキーが必要な場合、SetKeyboardState()で無理矢理キー状態を変更している。 SendInput()などを使うべきか？ そもそも根本的にアプローチが間違ってる気がする。
  vk=仮想キーコード。上８ビットにはシフト状態をセットする。
*/
bool proc_key_vk(uint16_t vk, HWND wh, HKL kl, unsigned convmode)
{
    bool st = false;
    uint32_t vkch = vk & 0xff; //仮想キーコード(シフトなし)
    uint32_t sc = MapVirtualKeyEx(vkch, MAPVK_VK_TO_VSC, kl); //仮想キーコード→スキャンコード
    uint32_t pk = (sc << 16) | 1; //ImmProcessKey用のスキャンコード
    UINT msg = WM_NULL;

    DEBUGLOG(CH_CANNA, "vk 0x%hx, sc 0x%x, wh %p, convmode 0x%x(roman %d)\n", vk, sc, wh, convmode, (convmode & IME_CMODE_ROMAN) != 0);
#if 0
    //!!! wm_wime_set_focus()でSetFocus()するようにした。
    if (GetFocus() != wh) //???調べずにいきなりSetFocusはだめか？
        SetFocus(wh);
#endif
    set_state(0x80, vk, convmode);
    if (ImmProcessKey(wh, kl, vkch, pk, 0))
        msg = WM_KEYDOWN;
    else if (ImmProcessKey(wh, kl, vkch, pk | KEYUP, 0))
        msg = WM_KEYUP;
    if (msg != WM_NULL) {
        if (ImmTranslateMessage(wh, msg, VK_PROCESSKEY, pk)) {
            st = true;
        }
        else {
            /*???atok2017ではImmTranslateMessageが常にfailを返す？
             failでも処理はされているようだ。2017のときはとりあえずtrueを返しておく。*/
            FATALLOG(CH_CANNA, "fail ImmTranslateMessage(), vkey 0x%hx, scancode 0x%x\n", vk, (unsigned)sc);
            if ((WimeData.ImeVersion)() == 30) {
                DEBUGLOG(CH_CANNA, "return true\n");
                st = true;
            }
        }
    }
    else {
        DEBUGLOG(CH_CANNA, "fail ImmProcessKey(), vkey 0x%hx, scancode 0x%x\n", vk, (unsigned)sc);
    }
    set_state(0, vk, convmode);
    return st;
}

//変換モードはローマ字固定なのでwimeから呼ばないように。
bool proc_key_ch(char ch, HWND wh, HKL kl)
{
    return proc_key_vk(VkKeyScanEx(ch, kl), wh, kl, CONV_MODE); //文字コード→仮想キーコード
}

/*
  utf16の全角ひらがなをローマ字にしてHWNDに送る。
  yomiは全角ひらがなに限定できるのだからZen2Roman()はもっと簡単になるのだが、asciiも処理できるようにしているので
  めんどくさくなっている。簡略化するか？
*/
bool send_roman(HWND wh, HKL kl, const uint16_t* yomi, int len)
{
    bool st = true;
    if (yomi != NULL) {
        for (; (len > 0 || (len < 0 && *yomi != 0)) && st; ++yomi, --len) {
            for (const char* rmj = U16Zen2Romaji(*yomi); *rmj != 0; ++rmj) {
                if (!proc_key_ch(*rmj, wh, kl)) {
                    st = false;
                    break;
                }
            }
        }
    }
    return st;
}

//beg<=...<endまでの読みを全角にしたときの長さ。濁点合成で半角カナより短くなる可能性がある。
//str!=NULLなら文字列を返す。クリアしておくこと。
int combined_yomi_len(CannaContext_t* cx, HIMC imc, int beg, int end, Array* str)
{
    Array compread;
    ArNew(&compread, 2, NULL);
    if (str == NULL)
        str = &compread;
    int len = ArUsing(ClauseStr(imc, cx, GCS_COMPREADSTR, beg, end, str, true)) - 1;
    if (len < 0)
        len = 0;
    ArDelete(&compread);
    return len;
}

/*YomiBufStartを更新する。
  SubstYomiで自動変換が起きて文節が返されると"cannaの読みバッファ"はクリアされる。
  imcではクリアはされないので、変換された読みの次からをcanna読みバッファの開始位置とする。
 */
void update_buf_start(CannaContext_t* cx, HIMC imc)
{
    int conved = 0;
    for (int cln = ImcClauseInfo(imc, GCS_COMPSTR, NULL); --cln >= 0;) {
        char at = GetAttr(imc, cln, cx);
        if (at == ATTR_TARGET_CONVERTED || at == ATTR_CONVERTED)
            ++conved;
    }
    if (conved != cx->ConvertedCl) {
        cx->ConvertedCl = conved;
        cx->YomiBufStart = combined_yomi_len(cx, imc, 0, conved, NULL);
        DEBUGLOG(CH_CANNA, "new bufstart %d\n", cx->YomiBufStart);
    }
}

/*0x13 自動変換:自動変換モード時に読みバッファ(自動変換変換で用いられる読みがなを保存する領域)の内容を変更し，再度変換を行う．
要求パケット(Type 4)
        i16	コンテクスト番号
        u16	開始オフセット
        u16	終了オフセット
        u16	読みの長さ
        s16	読み  '文字列@@'
応答パケット(Type 7)
        i16	文節数  エラー時: －1
        s16	各文節の最優先候補  '候補@@...@@候補@@@@'
*/
bool SubstYomi(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    uint16_t beg, end, len;
    bool st;
    HIMC imc;

    uint16_t* yomi = Req4(ch, &cxn, &beg, &end, &len);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        /*ローマ字／自動変換にする。*/
        DWORD cv_mode, sen_mode;
        st = ImmGetConversionStatus(imc, &cv_mode, &sen_mode);
        if (st && ((cv_mode & IME_CMODE_ROMAN) == 0 || (sen_mode & IME_SMODE_AUTOMATIC) == 0)) {
            ImmSetConversionStatus(imc, cv_mode | IME_CMODE_ROMAN, sen_mode | IME_SMODE_AUTOMATIC);
        }
        else {
            cv_mode = sen_mode = (DWORD)-1;
        }

        st = false;
        Array compread;
        HKL kl = GetKeyboardLayout(0);
        FromClientToU16(cx, yomi);
        DEBUGLOG(CH_CANNA, "context %hd, begin %hd, end %hd, length %hd, '%W'\n", cxn, beg, end, len, yomi);
        int cur_yomi_len = combined_yomi_len(cx, imc, 0, -1, ArNew(&compread, 2, NULL));
        DEBUGLOG(CH_CANNA, "bufstart %d read %d\n", cx->YomiBufStart, cur_yomi_len);
        beg += cx->YomiBufStart; //imc内読み文字列でのオフセットにする。
        end += cx->YomiBufStart;
        if (beg >= cur_yomi_len) {
            //最後尾に対する操作
            if (len == 0 || yomi == NULL) {
                //強制変換
                DEBUGLOG(CH_CANNA, "force conv\n");
                st = Reply5(ch->Major, ch->Minor, 0); //文節数０を返すとFlushYomiが来る。
            }
            else {
                //後ろに追加
                DEBUGLOG(CH_CANNA, "append [%*.2D]\n", len, yomi);
                send_roman(cx->Win, kl, yomi, -1);
                st = wm_ime_composition(cx, ch->Major, 0);
            }
        }
        else {
            DEBUGLOG(CH_CANNA, "replace\n");
            //beg<= ... <end までを置き換える。
            if (end == 0) {
                //begin=end=0なら全体の置き換え。
                ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
                send_roman(cx->Win, kl, yomi, -1);
            }
            else {
                //一部分を置き換え。
                DEBUGLOG(CH_CANNA, "delete %d\n", cur_yomi_len - beg);
                for (int count = cur_yomi_len - beg; count > 0; --count)
                    proc_key_ch('\b', cx->Win, kl); //begまでを消去
                send_roman(cx->Win, kl, yomi, -1);
                if (cur_yomi_len > end) {
                    //end以降
                    DEBUGLOG(CH_CANNA, "rest [%*.2D]\n", cur_yomi_len - end, ArElem(&compread, end));
                    send_roman(cx->Win, kl, ArElem(&compread, end), cur_yomi_len - end);
                }
            }
            st = wm_ime_composition(cx, ch->Major, 0);
        }
        update_buf_start(cx, imc);
        ImmReleaseContext(cx->Win, imc);
        ArDelete(&compread);

        //入力モードを元に戻す。
        if (!(cv_mode == (DWORD)-1 && sen_mode == (DWORD)-1))
            ImmSetConversionStatus(imc, cv_mode, sen_mode);
    }
    else {
        ERRORLOG(CH_CANNA, "context %hd NOT FOUND, begin %hd end %hd length %hd [%*.2D]\n", cxn, beg, end, len, len, yomi);
        st = Reply5(ch->Major, ch->Minor, -1);
    }
    return st;
}

/*
  指定文節を新しい読みに置き換えて再変換する。
  clindex 対象文節
  conv_cl 文節調整の対象になる最終文節
 */
bool store_yomi(CannaContext_t* cx, HIMC imc, int clindex, const uint16_t* yomi_u16)
{
    Array* yomi_all = ArNew(NULL, 2, NULL);
    if (clindex > 0) {
        //clindexより前の変換済み文節(ヌル文字は削除しておく)
        ArDec(ClauseStr(imc, cx, GCS_COMPREADSTR, 0, clindex, yomi_all, false));
    }

    //カレント文節の新しい読みを追加
    uint16_t* new_cur_han = U16ZenToHan(NULL, NULL, yomi_u16, -1);
    ArAddN(yomi_all, new_cur_han, WcLen(new_cur_han));
    free(new_cur_han);

    //残りの文節はもとの読み文字列。
    if (ClauseStr(imc, cx, GCS_COMPREADSTR, clindex + 1, -1, yomi_all, false) != NULL) {
        ArDec(yomi_all); //ヌル文字を削除しておく
    }
    ArDec(ArAdd1(yomi_all, &(uint16_t){0})); //バッファにヌル文字を書き込んでおくが文字数には含めない
    //DEBUGLOG(CH_CANNA,"'%W'\n",ArAdr(yomi_all));

    ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    bool st = true;
    if (!ImmSetCompositionStringW(imc, SCS_SETSTR, NULL, 0, ArAdr(yomi_all), ArUsingBytes(yomi_all))
        || !ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0)) {
        ERRORLOG(CH_CANNA, "fail set new yomi or convert:'%W'\n", ArAdr(yomi_all));
        st = false;
    }
    free(ArDelete(yomi_all));
    return st;
}

/*
  指定文節の読みの長さにdiffを加える。
 */
bool resize_clause(CannaContext_t* cx, HIMC imc, int cl_index, int diff)
{
    Array yomi_cl;
    int clnum = ImcClauseInfo(imc, GCS_COMPREADSTR, ArNew(&yomi_cl, 4, NULL));

    //操作対象が選択されていないとImmSetCompositionStringが失敗する。
    SetTarget(imc, cl_index, cx);

    int id_in_imc = cl_index - cx->FixedNum; //imc内の文節番号
    if (id_in_imc<0 || id_in_imc > clnum - 1) //文節番号が範囲外
        return false;

    int target_begin = ARVAL(int32_t, &yomi_cl, id_in_imc);
    int right_begin = ARVAL(int32_t, &yomi_cl, id_in_imc + 1);
    int new_length = right_begin - target_begin + diff;
    DEBUGLOG(CH_CANNA, "diff %d index %d clnum %d new-len %d\n", diff, id_in_imc, clnum, new_length);

    if (diff < 0) {
        //短くする。
        if (new_length <= 0) //これ以上縮めようとした
            return true;
        //最後の２文字が濁音だったときはさらに１文字短くする。
        Array yomi;
        ArDec(ClauseStr(imc, cx, GCS_COMPREADSTR, cl_index, cl_index + 1, ArNew(&yomi, 2, NULL), false));//ヌル文字は削除しておく。
        if (new_length >= 2 && U16CombineHanHira(ARELEM(uint16_t, &yomi, new_length - 1)) > 0) {
            --new_length;
        }
        ArDelete(&yomi);
    }
    else {
        //長くする。
        Array yomi;
        ArDec(ClauseStr(imc, cx, GCS_COMPREADSTR, cl_index, cl_index + 2, ArNew(&yomi, 2, NULL), false));	//右の文節まで取得する。
        DEBUGLOG(CH_CANNA, "total %d new-len %d\n", ArUsing(&yomi), new_length);
        if (id_in_imc == clnum - 1) /*これが最終文節ならこれ以上長くできない。*/
            return true;
        //濁点合成されるならもう１文字伸ばす。
        if (new_length < ArUsing(&yomi) && U16CombineHanHira(ARELEM(uint16_t, &yomi, new_length))>0)
            ++new_length;
        ArDelete(&yomi);
    }
    ARVAL(int32_t, &yomi_cl, id_in_imc + 1) = target_begin + new_length;
    bool st = (ImmSetCompositionStringW(imc, SCS_CHANGECLAUSE, NULL, 0, ArAdr(&yomi_cl), ArUsingBytes(&yomi_cl)) != 0);
    if (st && cl_index < ArUsing(&cx->CandInfo)) {
        //この文節と右の文節が影響を受ける→CandInfoを０に戻す
        CandListPageInfo* clp = ArElem(&cx->CandInfo, cl_index);
        *clp = (CandListPageInfo){ 0 };
        if (cl_index + 1 < ArUsing(&cx->CandInfo))
            *(clp + 1) = (CandListPageInfo){ 0 };
    }
    ArDelete(&yomi_cl);
    return st;
}

/*
  begin以上end未満の範囲の文節長さをold_yomi_clと同じにする。
 */
bool adj_clause(CannaContext_t* cx, HIMC imc, int begin, int end, Array* old_yomi_cl)
{
    bool st = true;
    Array new_yomi_cl;
    ArNew(&new_yomi_cl, 4, NULL);
    DEBUGDO(CH_CANNA, DbgComp(imc, "starting cluase"));
    for (int index = begin; index < end; ++index) {
        ImcClauseInfo(imc, GCS_COMPREADSTR, ArClear(&new_yomi_cl));
        int id_in_imc = index - cx->FixedNum;
        int diff = ARVAL(int32_t, old_yomi_cl, id_in_imc) - ARVAL(int32_t, &new_yomi_cl, id_in_imc);
        if (diff != 0) {
            DEBUGLOG(CH_CANNA, "current cl:%#*.4D,change #%d,add %d\n", ArUsing(&new_yomi_cl), ArAdr(&new_yomi_cl), id_in_imc, diff);
            st = resize_clause(cx, imc, index - 1, diff);
            if (!st) {
                DEBUGLOG(CH_CANNA, "fail ImmSetCompositionStringW:%#*.4D\n", ArUsing(&new_yomi_cl), ArAdr(&new_yomi_cl));
                break;
            }
        }
    }
    ArDelete(&new_yomi_cl);
    return st;
}

/*0x14 読みがな変更:カレント文節の読みがなを変更し，それ以降の文節を再変換する．
要求パケット(Type 11)
        i16	コンテクスト番号
        i16	カレント文節番号
        s16	読み '文字列@@'
応答パケット(Type 7)
        i16	文節数  エラー時: －1
        s16	各文節の最優先候補リスト  '候補@@...@@候補@@@@'
読みがNULLのときは文節を削除する。
*/
bool StoreYomi(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, clindex;
    HIMC imc;
    bool st = false;

    uint16_t* new_cur_yomi = Req11(ch, &cxn, &clindex);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        FromClientToU16(cx, new_cur_yomi);
        DEBUGLOG(CH_CANNA, "context %hd, clause %hd, '%W'\n", cxn, clindex, new_cur_yomi);

        Array old_yomi_cl;
        int clnum = ImcClauseInfo(imc, GCS_COMPREADSTR, ArNew(&old_yomi_cl, 4, NULL)); //元の文節情報
        if (clnum == 1 && new_cur_yomi == NULL) {
            //文節が１つなら全部キャンセルする。
            st = (ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0) != 0);
        }
        else {
            if (store_yomi(cx, imc, clindex, new_cur_yomi) && adj_clause(cx, imc, 0, clindex, &old_yomi_cl)) {
                st = true;
                cx->Conv = clindex;
            }
        }
        ArDelete(&old_yomi_cl);
        update_buf_start(cx, imc);
        ImmReleaseContext(cx->Win, imc);
    }
    else {
        ERRORLOG(CH_CANNA, "context %hd NOT FOUND, clause %hd, [%*.2D]\n", cxn, clindex, WcLen(new_cur_yomi), new_cur_yomi);
    }
    return st ? wm_ime_composition(cx, ch->Major, 0) : Reply5(ch->Major, ch->Minor, -1);
}

//ひらがなのu16を半角カナにしたときの文字数。
int han_len(const uint16_t* u16)
{
    int len;
    free(U16ZenToHan(NULL, &len, u16, -1));
    return len - 1;
}

/*0x15 カレント文節のみの単文節変換:カレント文節の読みがなを変更し，カレント文節のみを単文節変換する．
要求パケット(Type 11)
        i16	コンテクスト番号
        i16	カレント文節番号
        s16	読み  '文字列@@'
応答パケット(Type 7)
        i16	読みの長さ  エラー時: －1
        s16	単文節変換した最優先候補 '候補@@'
??? どういうときに呼ばれるんだろう？ 応答p1は要求p3の長さでいいのか？
wime:読みがNULLのとき、指定した文節(-1のとき現在の注目文節)を変換する。変換後の文節の文字列を返す。
*/
bool StoreRange(CanHeader* ch, int fd)
{
    int16_t cxn, clindex;
    Array cstr;
    HIMC imc;

    ArNew(&cstr, 2, NULL);
    uint16_t* yomi = Req11(ch, &cxn, &clindex);
    int16_t yomi_len = WcLen(yomi);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    DEBUGLOG(CH_CANNA, "StoreRange:cxn %hd index %hd yomi '%W'\n", cxn, clindex, yomi);
    if (cx != NULL) {
        if (yomi == NULL) {
            //wime用
            if (clindex < 0)
                clindex = GetAttrCl(imc, ATTR_TARGET_CONVERTED, cx);
            else
                SetTarget(imc, clindex, cx);
            if (ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0)) {
                ClauseStr(imc, cx, GCS_COMPSTR, clindex, clindex + 1, &cstr, false);
                FromU16ToClient(cx, &cstr);
                DEBUGDO(CH_CANNA, DbgComp(imc, __FUNCTION__));
                DEBUGLOG(CH_CANNA, "target-cl-num %hd\n", clindex);
                yomi_len = ArUsing(&cstr) - 1;
            }
            else {
                DEBUGLOG(CH_CANNA, "fail NotifyIME\n");
                ArDelete(&cstr);
                yomi_len = -1;
            }
        }
        else {
            FromClientToU16(cx, yomi);
            DEBUGLOG(CH_CANNA, "context %hd, clause %hd, yomi '%W'\n", cxn, clindex, yomi);

            Array old_yomi_cl;
            ImcClauseInfo(imc, GCS_COMPREADSTR, ArNew(&old_yomi_cl, 4, NULL)); //元の文節情報
            DEBUGLOG(CH_CANNA, "orig cl:%#*.4D\n", ArUsing(&old_yomi_cl), ArAdr(&old_yomi_cl));

            int id_in_imc = clindex - cx->FixedNum;
            //新しい読みとカレント文節の読みの長さの差。
            int diff = han_len(yomi) - (ARVAL(int32_t, &old_yomi_cl, id_in_imc + 1) - ARVAL(int32_t, &old_yomi_cl, id_in_imc));
            //カレント文節より右にある文節の開始位置はdiffだけずれる。
            for (int index = id_in_imc + 1; index < ArUsing(&old_yomi_cl); ++index) {
                ARVAL(int32_t, &old_yomi_cl, index) += diff;
            }
            DEBUGLOG(CH_CANNA, "adj  cl:%#*.4D\n", ArUsing(&old_yomi_cl), ArAdr(&old_yomi_cl));

            /*カレント文節は新しい読みで置き換え、文節長は元と同じにする。
              新しい読みは１つの文節にすることで単文節変換とする。
            */
            if (store_yomi(cx, imc, clindex, yomi) && adj_clause(cx, imc, 0, ArUsing(&old_yomi_cl), &old_yomi_cl)) {
                ClauseStr(imc, cx, GCS_COMPSTR, clindex, clindex + 1, &cstr, false);
                FromU16ToClient(cx, &cstr);
                DEBUGDO(CH_CANNA, DbgComp(imc, __FUNCTION__));
            }
            else
                yomi_len = -1;
            update_buf_start(cx, imc);
        }
    }
    else {
        ERRORLOG(CH_CANNA, "context %hd NOT FOUND, clause %hd [%*.2D]\n", cxn, clindex, yomi_len, yomi);
    }
    bool st = Reply7(ch->Major, ch->Minor, yomi_len, ArAdr(&cstr), ArUsing(&cstr));
    ArDelete(&cstr);
    return st;
}

/*0x16 未決文節取得:未決文節の読みを取得する．
要求パケット(Type 3)
        i16	コンテクスト番号
        u16	読みのバッファサイズ
応答パケット(Type 7)
        i16	読みの長さ  エラー時: －1
        s16	未決文節の読み '文字列@@'
未決文節というより未変換文節のようだ。
*/
bool GetLastYomi(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, st = -1;
    uint16_t bufsize;
    Array yomi;
    HIMC imc;

    ArNew(&yomi, 2, NULL);
    Req3(ch, &cxn, &bufsize);
    DEBUGLOG(CH_CANNA, "context %hd, bufsize %hd\n", cxn, bufsize);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        int n;
        if ((n = GetAttrCl(imc, ATTR_INPUT, cx)) >= 0) {
            ClauseStr(imc, cx, GCS_COMPREADSTR, n, n + 1, &yomi, true);
            DEBUGLOG(CH_CANNA, "cl-num %d, string '%W'\n", n, ArAdr(&yomi));
            FromU16ToClient(cx, &yomi);
            st = ArUsing(&yomi) - 1;
        }
        else {
            //??? 未決文節がないときはエラーなのか？ とりあえず０で正常終了する。
            st = 0;
            DEBUGLOG(CH_CANNA, "noting\n");
        }
        ImmReleaseContext(cx->Win, imc);
    }
    bool ret = Reply7(ch->Major, ch->Minor, st, ArAdr(&yomi), ArUsing(&yomi));
    ArDelete(&yomi);
    return ret;
}

/*0x17  強制変換:未決定文節を強制的に変換する．
要求パケット(Type 10)
        i16	コンテクスト番号
        i16	カレント文節番号
        i32	モード(0)
        i16[]	各文節のカレント候補番号(カレント文節番号+1)
応答パケット(Type 7)
        i16	文節数  エラー時: －1
        s16	最優先候補リスト  '候補@@...@@候補@@@@'
*/
bool FlushYomi(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, cl;
    int32_t mode;
    bool st = false;
    HIMC imc;

    int16_t* cand = Req10((Req10_t*)ch, &cxn, &cl, &mode);
    DEBUGLOG(CH_CANNA, "context %hd, clause %hd, mode %d, candidate [%#*.2D]\n", cxn, cl, mode, (ch->Length - 8) / 2, cand);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        cx->Conv = 0;
        cx->FerMode = mode;
        if (GetAttrCl(imc, ATTR_INPUT, cx) >= 0 || GetAttrCl(imc, ATTR_TARGET_NOTCONVERTED, cx) >= 0) {
            //未変換文節がある
            if (ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0)) {
                st = true; //あとはWM_IME_COMPOSITIONメッセージの処理に任せる
                update_buf_start(cx, imc);
            }
            else
                INFOLOG(CH_CANNA, "fail ImmNotifyIME\n");
        }
        else {
            //すでに全部変換済み
            DEBUGLOG(CH_CANNA, "already convert\n");
            st = true; //??? また変換？
        }
        ImmReleaseContext(cx->Win, imc);
    }
    return st ? wm_ime_composition(cx, ch->Major, 0) : Reply5(ch->Major, ch->Minor, -1);
}

/*0x18  読みバッファ削除:先頭文節からカレント文節まで読みを読みバッファから取り除く．
要求パケット(Type 10)
        i16	コンテクスト番号
        i16	カレント文節番号
        i32	モード  0なら学習しない   RkwRemoveBunのmode
        i16[]	各文節のカレント候補番号(カレント文節番号+1)
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
???  StoreRangeと同じくいつ使われるかわからない。仮変換文節数が一定以上になったら呼ばれるのか?
  応答パケットの文節数は全文節数でいいのか？カレント文節以降の残りの文節数か？
*/
bool RemoveYomi(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, cl;
    int32_t mode;
    HIMC imc;
    char clall = -1; //全文節数

    int16_t* cand = Req10((Req10_t*)ch, &cxn, &cl, &mode);
    DEBUGLOG(CH_CANNA, "context %hd, clause %hd, mode %d, candidate [%#*.2D]\n", cxn, cl, mode, (ch->Length - 8) / 2, cand);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        //先頭からclまでの文節を取り除く。
        bool st = true;
        if (mode && cl >= cx->FixedNum)
            update_cand(imc, cand, cl - cx->FixedNum + 1, &cx->CandInfo, cx);
        if (cl < cx->FixedNum - 1) {
            //固定文節のみ削除する(固定文節は１つ以上残る)
            ListRemoveRange(&cx->FixedStr, 0, cl);
            ListRemoveRange(&cx->FixedYomi, 0, cl);
            cx->FixedNum -= cl + 1;
            DEBUGLOG(CH_CANNA, "fixed clauses %d\n", cx->FixedNum);
        }
        else {
            //固定文節は全部削除。
            ArClear(&cx->FixedStr);
            ArClear(&cx->FixedYomi);
            cl -= cx->FixedNum;
            //imcの先頭からclまでの文節を取り除く→clより後ろのみにする。
            Array yomi;
            ClauseStr(imc, cx, GCS_COMPREADSTR, cl + 1, -1, ArNew(&yomi, 2, NULL), false);
            ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmSetCompositionStringW(imc, SCS_SETSTR, NULL, 0, ArAdr(&yomi), ArUsingBytes(&yomi));
            ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
            cx->FixedNum = 0;
            ArDelete(&yomi);
        }
        if (st) {
            clall = cx->FixedNum + ImcClauseInfo(imc, GCS_COMPSTR, NULL);

            /* clまでの候補情報があれば消して前に詰める */
            if (cl + 1 <= ArUsing(&cx->CandInfo)) {
                ArRemove(&cx->CandInfo, 0, cl + 1);
                DEBUGLOG(CH_CANNA, "new candinfo length %d\n", ArUsing(&cx->CandInfo));
            }
        }
        update_buf_start(cx, imc);
        ImmReleaseContext(cx->Win, imc);
    }
    //??? とりあえず残りの文節数を返してみる
    return Reply2(ch->Major, ch->Minor, clall);
}

/*19  限定候補取得:指定された辞書からその辞書に含まれている候補のみを取得する．
要求パケット(Type 13)
        i16	コンテクスト番号
        s8	辞書名  '辞書名@'
        s16	読み  '読み@@'
        u16	読みの長さ
        u16	候補バッファサイズ
        u16	品詞バッファサイズ
応答パケット(Type 8)
        i16	候補数  エラー時: －1
        s16	候補文字列  '候補@@...@@候補@@@@'
        s16	品詞文字列  '品詞情報@@...@@品詞情報@@@@'
*/
bool GetSimpleKanji(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    uint16_t yomi_len, cand_bufsize, hinshi_bufsize, * yomi;

    char* dic = Req13((Req13_t*)ch, &cxn, &yomi, &yomi_len, &cand_bufsize, &hinshi_bufsize);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, dic '%s', yomi '%S', yomi-len %hd, cand-bufsize %hd, hinshi-bufsize %hd\n", cxn, dic, yomi, yomi_len, cand_bufsize, hinshi_bufsize);
    free(yomi);
    return Reply5(ch->Major, ch->Minor, -1);
}

/*0x1a  区切り変更:指定された文節を，指定された長さに区切り直して，再度かな漢字変換する．
要求パケット(Type 7)
        i16	コンテクスト番号
        i16	文節番号
        i16	読みの長さ 読みの長さ / －2では文節縮め / －1では文節伸ばし
応答パケット(Type 7)
        i16	文節数  正常時:先頭文節から最終文節までの文節数  エラー時:－1
        s16	各文節の最優先候補リスト  '候補@@...@@候補@@@@'
各文節の最優先候補リストは，*カレント文節*から最終文節までの最優先候補を返す．
文節番号は０から
*/
bool ResizePause(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, clnum, count;
    bool st = false;
    HIMC imc;

    Req7(ch, &cxn, &clnum, &count);
    DEBUGLOG(CH_CANNA, "context %hd, clause %hd, count %hd\n", cxn, clnum, count);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL && clnum >= cx->FixedNum) {
        st = true;
        switch (count) {
        case -1:
            count = 1;
        case 0: //[r26]文節文字数以上に伸ばそうとするとこれが来る。
            break;
        case -2:
            count = -1;
            break;
        default:
            if (count > 0) {
                count -= combined_yomi_len(cx, imc, clnum, clnum + 1, NULL); //何文字伸ばすかをcountに入れる。
            }
            else {
                DEBUGLOG(CH_CANNA, "invalid count:%hd\n", count);
                st = false;
            }
        }
        if (st && count != 0) {
            DEBUGLOG(CH_CANNA, " --> count %hd\n", count);
            st = resize_clause(cx, imc, clnum, count);
        }
    }
    if (imc != NULL)
        ImmReleaseContext(cx->Win, imc);
    return st ? (cx->Conv = clnum, wm_ime_composition(cx, ch->Major, clnum)) : Reply5(ch->Major, ch->Minor, -1);
}

/*1b 品詞情報:カレント候補に対する品詞情報を文字列で取得する．
要求パケット(Type 8)
        i16	コンテクスト番号
        i16	カレント文節番号
        i16	カレント候補番号
        u16	読みのバッファサイズ
応答パケット(Type 7)
        i16	読みの長さ  エラー時: －1
        s16	品詞情報文字列  '文字列@@'
*/
bool GetHinshi(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, cl, cand;
    uint16_t bufsize;

    Req8(ch, &cxn, &cl, &cand, &bufsize);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, clause %hd, candidate %hd, bufsize %hu\n", cxn, cl, cand, bufsize);
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1c  形態素情報:カレント文節の形態素情報を取得する．
要求パケット(Type 9)
        i16	コンテクスト番号
        i16	カレント文節番号
        i16	カレント候補番号
        i16	形態素情報のバッファサイズ
応答パケット(Type 9)
        i16	単語数  エラー時: －1
        i32[]	形態素情報  全部で{20(struct RkLexの大きさ)×単語数}バイト
*/
bool GetLex(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, cl, cand, bufsize;

    Req9(ch, &cxn, &cl, &cand, &bufsize);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, clause %hd, candidate %hd, bufsize %hu\n", cxn, cl, cand, bufsize);
    return Reply5(ch->Major, ch->Minor, -1);
}

/*1d  解析情報:カレント候補に関する解析情報を求める．
要求パケット(Type 7)
        i16	コンテクスト番号
        i16	カレント文節番号
        i16	カレント候補番号
応答パケット(Type 4)
        i8	終了状態  正常時: 0 / エラー時: －1
        i32[]	解析情報(全部で28バイト)  struct RkStatの大きさが28バイト
*/
bool GetStatus(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, clnum, cand;
    int datalen = 0;
    char st = -1;
    HIMC imc;

    Req7(ch, &cxn, &clnum, &cand);
    DEBUGLOG(CH_CANNA, "context %hd, clause number %hd, candidate number %hd\n", cxn, clnum, cand);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        cx->RkSt.bunnum = clnum;
        cx->RkSt.candnum = cand;

        /* [1.8.4]uimでは候補リストを出す前にこのapiが呼ばれるらしいので、
           候補数だけ調べる */
        int cand_count = -1;
        CandListPageInfo* pi = ArElem(&cx->CandInfo, clnum);
        if (clnum >= ArUsing(&cx->CandInfo) || (pi->Seq == 0 && pi->Size[0] == 0)) {
            /* この文節で候補リストを出していないとき。
               候補がなかったときもSize[0]は0になる。候補リストを調べたかどうかが
               わからないので、このときはあきらめて再度調べる。このためにフラグを追加
               するのもうっとうしいし。*/
            ArAlloc(&cx->CandInfo, clnum + 1);
            pi = ArElem(&cx->CandInfo, clnum);
            switch (SetTarget(imc, clnum, cx)) {
            case ChangeTargetSuccess:
                cand_count = make_cand_list(imc, NULL, pi, clnum, cx);
                break;
            case ChangeTargetFixed:
                DEBUGLOG(CH_CANNA, "this clause is fixed\n");
                break;
            case ChangeTargetFail:
                INFOLOG(CH_CANNA, "fail SetTarget\n");
            }
        }
        else { //pi!=NULLのはず
            cand_count = pi->Seq;
        }
        if (cand_count >= 0) {
            cx->RkSt.diccand = cand_count; //pi->Seq; //[r12]cand_countが反映されていなかった?
            for (int n = 0; n < CANDLISTMAX; ++n)
                cx->RkSt.diccand += pi->Size[n];
            Array u16;
            ArNew(&u16, 2, NULL);
            /*[1.8.5]候補が無くてもpi->Seqは1になる(はず)
              if(cand_count == 0)     // 変換結果以外に候補がなければ、変換結果を足す
              cx->RkSt.diccand++;
            */
            cx->RkSt.maxcand = cx->RkSt.diccand + fer_mode_num(cx->FerMode);
            // ylen,klenはバイト数ではなく文字数！(ヌル文字の分1引いている)
            //ylenは濁点合成で半角カナ表現より文字数が減る可能性があるので、いったん全角に変換して文字数を調べる。
            cx->RkSt.ylen = combined_yomi_len(cx, imc, clnum, clnum + 1, NULL);
            cx->RkSt.klen = ArUsing(ClauseStr(imc, cx, GCS_COMPSTR, clnum, clnum + 1, &u16, false)) - 1;
            cx->RkSt.tlen = 1; //カレント候補の構成単語数。1でいいのか？
            datalen = sizeof(RkStat) / 4;
            st = 0;
            ArDelete(&u16);
            DEBUGLOG(CH_CANNA, "bunnum %d, candnum %d, maxcand %d, diccand %d, ylen %d, klen %d, tlen %d\n", cx->RkSt.bunnum, cx->RkSt.candnum, cx->RkSt.maxcand, cx->RkSt.diccand, cx->RkSt.ylen, cx->RkSt.klen, cx->RkSt.tlen);
        }
        ImmReleaseContext(cx->Win, imc);

    }
    return Reply4(ch->Major, ch->Minor, st, (int32_t*)(&cx->RkSt), datalen);
}

/*1e  locale情報セット
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	locale情報  '文字列@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー: －1
*/
bool SetLocale(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* loc = Req15(ch, &mode, &cxn);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, mode %d, locale %s\n", cxn, mode, loc);
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1f  自動変換開始:自動変換モードでかな漢字変換を行う．
要求パケット(Type 5)
        i16	コンテクスト番号
        u16	バッファサイズ
        i32	モード
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
??? どう実装しよう？ IME_SMODE_AUTOMATICにして読み仮名をセットしても何も起こらない。
  そこからImmNotifyIME()で変換できるが、それでは普通の連文節変換と変わらない。
  クライアントから送られてくる読み仮名をローマ字に戻し、キー入力としてimeに送ることにする。
  しかしこれはあんまりだろう。もうちょっとましな方法はないか？
*/
bool AutoConv(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    uint16_t bufsize;
    int32_t mode;
    HIMC imc;
    char st = -1;

    Req5(ch, &cxn, &bufsize, &mode);
    DEBUGLOG(CH_CANNA, "context %hd, bufsize %hd, mode 0x%x\n", cxn, bufsize, mode);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
#ifdef SETCONTEXT_FAIL
        SetCurrentImc(imc, TRUE);
#else
        ImmSetOpenStatus(imc, TRUE);
        ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
#endif
        ImmReleaseContext(cx->Win, imc);
        cx->FerMode = mode;
        cx->YomiBufStart = cx->ConvertedCl = 0;
        st = 0;
    }
    return Reply2(ch->Major, ch->Minor, st);
}

/*0x20  拡張プロトコル問い合わせ
要求パケット(Type 17)
        s8	リクエスト名リスト  'リクエスト名@...@リクエスト名@@'
応答パケット(Type 2)
        i8	プロトコル番号の起点－1 エラー時:－1
  リクエスト名リストには、1 つの拡張をなす一連のリクエストの名前を指定する。サーバから
  返される値は、その拡張の最初のリクエストの番号から 1 を減じたものである。拡張プロトコル
  が見つからない場合、－1 が返される。
*/
bool QueryExt(CanHeader* ch, int fd UNUSED)
{
    static const char* extnames[] = {
        "GetServerInfo\0"	"GetAccessControlList\0"
        "CreateDictionary\0"	"DeleteDictionary\0"
        "RenameDictionary\0"	"GetWordTextDictionary\0"
        "ListDictionary\0"	"Sync\0"
        "ChmodDictionary\0"	"CopyDictionary\0",

        //so/pkt.hのプロトコル番号,main.cのinit_cb()も変更すること
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
        "WimeShowCandWin\0"
        "WimeSelectCand\0"
        "WimeCloseCandWin\0"
        "WimeDumpContext\0"
        "WimeSetDebugChannel\0"
        "WimeGetColor\0"
        "WimeGetCandWin\0"
        "WimeCandIndex\0"
    };

    int id;
    Array names, reqs;
    char* reqnames = ((Req17_t*)ch)->p1;
    ArNew(&names, 1, NULL);
    ArNew(&reqs, 1, NULL);
    for (int ext = 0; ext < 2; ++ext) {
        ListRaw(ArClear(&names), extnames[ext]);
        ListRaw(ArClear(&reqs), reqnames);
        if ((id = SubList(&names, &reqs)) >= 0)
            break;
        /*返されるのはリスト内の位置なので０からだが、プロトコル番号としては１からで、QueryExtは
          {プロトコル番号-1}を返すのでリスト内番号をそのまま返す。
         */
    }
    ArDelete(&names);
    ArDelete(&reqs);
    return Reply2(ch->Major, ch->Minor, (char)id);
}

/*21  アプリケーション名登録:クライアントのアプリケーション名をそのサーバに通知し，登録する．
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	アプリケーション名  'アプリケーション名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
??? モードとコンテキストはどういったときに使う？
  コンテキストが指定されるということは、コンテキストごとにアプリ名が指定されるということ？
*/
bool SetAppName(CanHeader* ch, int fd)
{
    int32_t mode;
    int16_t cxn;
    char* name = Req15(ch, &mode, &cxn);
    DEBUGLOG(CH_CANNA, "mode %d, context %hd, name %s, IGNORE mode and context\n", mode, cxn, name);
    ClientData_t* cdt = FindClient(fd);
    free(cdt->App);
    cdt->App = strdup(name);
    return Reply2(ch->Major, ch->Minor, 0);
}

/*22  グループ名の通知:クライアントのグループ名をサーバに通知し、登録する。
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	グループ名  'グループ名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: 負の値
*/
bool NoticeGroup(CanHeader* ch, int fd)
{
    int32_t mode;
    int16_t cxn;
    char* name = Req15(ch, &mode, &cxn);
    DEBUGLOG(CH_CANNA, "mode %d, context %hd, group %s, IGNORE mode and context\n", mode, cxn, name);
    ClientData_t* cdt = FindClient(fd);
    free(cdt->Group);
    cdt->Group = strdup(name);
    return Reply2(ch->Major, ch->Minor, 0);
}

/*24  サーバを終了させる
要求パケット(Type 1)
応答パケット(Type 2)
        i8	ステータス  正常時: 0 / エラー時: 負の値
現在、エラー時の値として定義されているのは、以下の通りである。
OLDSRV 『かんな』Version 3.2 以前のサーバにプロトコルを送った場合
NOTUXSRV サーバを起動しているマシンからプロトコルが送られていない場合
NOTOWSRV サーバを起動したユーザ名と一致していない場合
!!! エラーの処理をしていない。
*/
bool KillServer(CanHeader* ch, int fd UNUSED)
{
    DEBUGLOG(CH_CANNA, "kill wime\n");
    Reply2(ch->Major, ch->Minor, 0);
    ImCloseAll();
    PostQuitMessage(0);
    return true;
}

/*1-01 サーバ情報取得:サーバに接続しているクライアント数などのサーバ情報を取得する．
要求パケット(Type 1)
応答パケット(Type 1)
        u8	終了状態(0)
        u8	メジャーサーババージョン
        u8	マイナーサーババージョン
        u32	サーバが動作しているマシンの現在時刻  ctimeの値
        u16	プロトコル数  認識できるプロトコルの数
        u16	プロトコル名リスト長(2バイトでcard8となっているが16bitの間違いだろう)
        s8	プロトコル名リスト  '文字列@文字列@...@@'
        u32[]	プロトコル使用頻度  {4×プロトコル数}バイト
        u16	接続しているクライアント数
        u16	コンテクスト数
        u16	クライアント情報リスト長
        i8[]	クライアント情報リスト  (情報長+情報) の繰り返し
各クライアントの情報は以下の通りである．
        i32	ソケット番号
        i32	ユーザ管理番号
        u32	ユーザ消費時間  秒単位
        u32	アイドル時間  秒単位
        u32	コネクト時間  秒単位
        u32[]	プロトコル使用頻度  {4×プロトコル数}バイト
        u16	ユーザ名長
        s8	ユーザ名  2バイト境界に合うようパディングする．
        u16	ホスト名長  2バイトのcard8となっているが16bitの間違いだろう。
        s8	ホスト名  2バイト境界に合うようパディングする．
        s8	クライアント名
        u8[]	コンテクスト管理フラグ
*/
bool GetServerInfo(CanHeader* ch, int fd UNUSED)
{
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1-02 サーバ使用許可:サーバの使用許可の参照および設定を行う．
要求パケット(Type 1)
応答パケット(Type 6)
        i16	ホスト数  エラー時: －1
        s8	ホスト名リスト  'ホスト名@ユーザ名@...@ユーザ名@@ホスト名@ユーザ名@...@@'
*/
bool GetAcl(CanHeader* ch, int fd UNUSED)
{
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    return Reply5(ch->Major, ch->Minor, -1);
}

/*1-03 辞書作成:テキスト辞書を作成し， dics.dir の内容を更新する．
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	辞書名  '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
*/
bool CreateDic(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* dic = Req15(ch, &mode, &cxn);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, mode %d, dic %s\n", cxn, mode, dic);
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1-04  辞書削除:テキスト辞書を削除し， dics.dir の内容を更新する．
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	辞書名  '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
*/
bool DeleteDic(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* dic = Req15(ch, &mode, &cxn);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, mode %d, dic %s\n", cxn, mode, dic);
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1-05 辞書名変更:ユーザ辞書の辞書名を変更する．
要求パケット(Type 16)
        i32	モード
        i16	コンテクスト番号
        s8	現在の辞書名  '辞書名@'
        s8	新しい辞書名  '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: －1
Req16となっているがReq19(QueryDic)とする。それともQueryDicが間違っているのか？
*/
bool RenameDic(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* cur_dic;
    char* new_dic = Req19(ch, &mode, &cxn, &cur_dic);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, mode %d, current-dic %s, new-dic %s\n", cxn, mode, cur_dic, new_dic);
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1-06 単語情報取得:テキスト辞書の単語情報を取得する．
要求パケット(Type 18)
        i16	コンテクスト番号
        s8	ディレクトリ名 'ディレクトリ名@'
        s8	辞書名 '辞書名@'
        u16	バッファサイズ
応答パケット(Type 7)
        i16	単語情報長  エラー時: －1
        s16	文字列リスト '文字列@@'
*/
bool GetWordTextDic(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    char* dir, * dic;

    uint16_t bufsize = Req18((Req18_t*)ch, &cxn, &dir, &dic);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, directory %s, dic %s, bufsize %hu\n", cxn, dir, dic, bufsize);
    return Reply5(ch->Major, ch->Minor, -1);
}

/*1-07 辞書テーブル取得:指定した辞書ディレクトリにある辞書テーブルを取得する．
要求パケット(Type 18)
        i16	コンテクスト番号
        s8	辞書ディレクトリ名 'ディレクトリ名[:ディレクトリ名]..'
        u16	バッファサイズ
応答パケット(Type 6)
        i8	辞書数  エラー時: －1
        s8	辞書リスト '辞書名@...@辞書名@@'
ドキュメントでは要求タイプ18になっているが、18は i16,s8,s8,u16
このプロトコルタイプ i16,s8,u16 をタイプ16にする。
*/
bool ListDic(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    char* dirs;
    uint16_t bufsize = Req16((Req16_t*)ch, &cxn, &dirs);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, bufsize %hu, dir %s\n", cxn, bufsize, dirs);
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1-08 リソースの更新:メモリ内の保持している辞書情報を辞書に書き込む．
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	辞書名 '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: 負の値
*/
bool Sync(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* dic = Req15(ch, &mode, &cxn);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, mode %d, dic %s\n", cxn, mode, dic);
    return Reply2(ch->Major, ch->Minor, -1);
}

/*1-09 辞書のREAD/WRITE 権の変更
要求パケット(Type 15)
        i32	モード
        i16	コンテクスト番号
        s8	辞書名 '辞書名@'
応答パケット(Type 5)
        i16	終了状態  正常時: chmoddicの戻り値 / エラー時: 負の値
*/
bool ChmodDic(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* dic = Req15(ch, &mode, &cxn);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, mode %d, dic %s\n", cxn, mode, dic);
    return Reply5(ch->Major, ch->Minor, -1);
}

/*1-0a 辞書の複写
要求パケット(Type 21)
        i32	モード
        i16	コンテクスト番号
        s8	複写元のディレクトリ名 'ディレクトリ名@'
        s8	複写元の辞書名 '辞書名@'
        s8	複写先の辞書名 '辞書名@'
応答パケット(Type 2)
        i8	終了状態  正常時: 0 / エラー時: 負の値
*/
bool CopyDictionary(CanHeader* ch, int fd UNUSED)
{
    int32_t mode;
    int16_t cxn;
    char* dir, * src;

    char* dst = Req21(ch, &mode, &cxn, &dir, &src);
    ERRORLOG(CH_GLOBAL, "*** NOT IMPLIMENT ***\n");
    DEBUGLOG(CH_CANNA, "context %hd, mode %d, dir %s, source %s, destination %s\n", cxn, mode, dir, src, dst);
    return Reply2(ch->Major, ch->Minor, -1);
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
bool OpenDialog(CanHeader* ch, int fd UNUSED)
{
    unsigned modes[] = { IME_CONFIG_GENERAL,IME_CONFIG_REGISTERWORD,IME_CONFIG_SELECTDICTIONARY };
    char st = -1;
    int16_t dialog_type = Req2(ch);

    DEBUGLOG(CH_CANNA, "dialog type code %hu\n", dialog_type);

    //ImmConfigureIMEのhwndはグローバルコンテキストのものを使うことにする。
    REGISTERWORDA reg = { .lpReading = NULL, .lpWord = NULL };
    CannaContext_t* cx = ArElem(&Context, 0);

    if (dialog_type < 3 && ImmConfigureIME(GetKeyboardLayout(0), cx->Win, modes[dialog_type], &reg)) {
        st = 0;
    }
    return Reply2(ch->Major, ch->Minor, st);
}

#ifdef DEBUG_MODE
int get_mod_name(void* buf, int bufsize, void* data/* LONG* */)
{
    sz = GetModuleFileName((HMODULE) * (LONG*)data, buf, bufsize);
    return sz < bufsize ? 0 : sz;
}
int get_cl_name(void* buf, int bufsize, void* data/* HWND* */)
{
    sz = GetClassName(*(HWND*)data, buf, bufsize);
    return sz < bufsize ? 0 : sz;
}

//lp=intの作業変数のアドレス,初期値０
BOOL CALLBACK EnumWin(HWND h, LPARAM lp)
{
    int st, * counter = (int*)lp;
    LONG wl[][2] = { {GWL_EXSTYLE,0},{GWL_STYLE,0},{GWL_WNDPROC,0},{GWL_HINSTANCE,0},{GWL_HWNDPARENT,0},{GWL_ID,0},{GWL_USERDATA,0} };
    DWORD cl[][2] = { {GCW_ATOM,0},{GCL_HMODULE,0},{GCL_MENUNAME,0} };

    if ((*counter)++ == 0)
        MESG("hwnd	class	exstyle	style	wndproc	instance	parent	id	userdata	atom	module	menu\n");

    for (st = 0; st < 7; ++st)
        wl[st][1] = GetWindowLong(h, wl[st][0]);
    for (st = 0; st < 3; ++st)
        cl[st][1] = GetClassLong(h, cl[st][0]);

    ArNew(&buf, 1, NULL);
    ArBuf(&buf, get_cl_name, &h);
    MESG("%x\t%s\t", (unsigned)h, (char*)ArAdr(&buf));
    MESG("%x\t%x\t", wl[0][1], wl[1][1]); //exstyle,style
    MESG("%x\t", wl[2][1]); //wndproc
    ArBuf(&buf, get_mod_name, &(wl[3][1]));
    MESG("%x(%s)\t%x\t", wl[3][1], (char*)ArAdr(&buf), wl[4][1]); //instance,parent
    MESG("%x\t%x\t", wl[5][1], wl[6][1]); //id,userdata
    ArBuf(&buf, get_mod_name, &(LONG)(cl[1][1]));
    MESG("%x\t%x(%s)\t%x\n", cl[0][1], cl[1][1], (char*)ArAdr(&buf), cl[2][1]); //atom,module,menu
    ArDelete(&buf);
    return TRUE;
}

void debug_window(HWND w)
{
    int dum = 0;
    MESG("window listing...\n");
    EnumWindows(EnumWin, (LPARAM)&dum);
    MESG("...end,%d windows\n", dum);

    COMPOSITIONFORM cf;
    HIMC imc = ImmGetContext(w);
    ImmGetCompositionWindow(imc, &cf);
    ImmReleaseContext(w, imc);
    switch (cf.dwStyle) {
    case CFS_DEFAULT:
        MESG("comp-form:default\n");
        break;
    case CFS_FORCE_POSITION:
        MESG("comp_form:force %d %d\n", cf.ptCurrentPos.x, cf.ptCurrentPos.y);
        break;
    case CFS_POINT:
        MESG("comp_form:point %d %d\n", cf.ptCurrentPos.x, cf.ptCurrentPos.y);
        break;
    case CFS_RECT:
        MESG("comp_form:rect %d %d %d %d\n", cf.rcArea.left, cf.rcArea.top, cf.rcArea.right, cf.rcArea.bottom);
        break;
    default:
        MESG("comp_form:??? (%x)\n", cf.dwStyle);
    }

    MESG("fg window:%x exist:%d visible:%d\n", (unsigned)GetForegroundWindow(), IsWindow(w), IsWindowVisible(w));
    MESG("fg window info...\n");
    dum = 0;
    EnumWin(GetForegroundWindow(), (LPARAM)&dum);
    MESG("...end\n");
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
bool SetCompWin(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, style;
    HIMC imc;
    bool st = false;

    uint16_t* params = Req11(ch, &cxn, &style);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        COMPOSITIONFORM cf;
        switch (style) {
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

        switch (style) {
        case WIME_POS_FORCE:
        case WIME_POS_POINT:
            cf.ptCurrentPos.x = params[0];
            cf.ptCurrentPos.y = params[1];
            DEBUGLOG(CH_CANNA, "context %hd, pos (%hd,%hd)\n", cxn, params[0], params[1]);
            break;
        case WIME_POS_RECT:
            cf.rcArea.left = params[0];
            cf.rcArea.top = params[1];
            cf.rcArea.right = params[0] + params[2];
            cf.rcArea.bottom = params[1] + params[3];
            //RECTのヘルプでは(r,b)は四角に含まれない？
            DEBUGLOG(CH_CANNA, "context %hd, rect (%d,%d)-(%d,%d)\n", cxn, cf.rcArea.left, cf.rcArea.top, cf.rcArea.right, cf.rcArea.bottom);
        }
        st = ImmSetCompositionWindow(imc, &cf);
        ImmReleaseContext(cx->Win, imc);
        DEBUGLOG(CH_CANNA, "context %hd, wnd %p, ret %d\n", cxn, cx->Win, st);
    }
    return Reply2(ch->Major, ch->Minor, st);
}

/* 変換モードを返す。
   imcがNULLのときはデフォルトの設定を返す。
 */
unsigned conv_mode(HIMC imc)
{
    bool default_status = false;
    if (!imc) {
        imc = ImmCreateContext();
        default_status = true;
    }
    DWORD c, s;
    if (!ImmGetConversionStatus(imc, &c, &s)) {
        c = CONV_MODE;
        DEBUGLOG(CH_CANNA, "fail ImmGetConversionStatus,force CONV_MODE.\n");
    }
    DEBUGLOG(CH_CANNA, "roman mode=%d\n", ((c & IME_CMODE_ROMAN) != 0));
    if (default_status) {
        ImmDestroyContext(imc);
    }
    return c;
}

#include <X11/X.h>
#include <X11/keysym.h>
static int KeyMap[][2] = {
    {XK_yen,VK_OEM_5},		/* ￥| */
    {XK_BackSpace,VK_BACK},	/* Back space, back char */
    {XK_Tab,VK_TAB},
    {XK_Clear,VK_CLEAR},
    {XK_Return,VK_RETURN},	/* Return, enter */
    {XK_Pause,VK_PAUSE},	/* Pause, hold */
    {XK_Escape,VK_ESCAPE},
    {XK_Delete,VK_DELETE},	/* Delete, rubout */
    {XK_Home,VK_HOME},
    {XK_Left,VK_LEFT},		/* Move left, left arrow */
    {XK_Up,VK_UP},		/* Move up, up arrow */
    {XK_Right,VK_RIGHT},	/* Move right, right arrow */
    {XK_Down,VK_DOWN},		/* Move down, down arrow */
    {XK_Page_Up,VK_PRIOR},
    {XK_Page_Down,VK_NEXT},
    {XK_End,VK_END},		/* EOL */
    {XK_Begin,VK_HOME},		/* BOL */
    {XK_Select,VK_SELECT},	/* Select, mark */
    {XK_Print,VK_PRINT},
    {XK_Execute,VK_EXECUTE},	/* Execute, run, do */
    {XK_Insert,VK_INSERT},	/* Insert, insert here */
    {XK_Menu,VK_MENU},
    {XK_Cancel,VK_CANCEL},	/* Cancel, stop, abort, exit */
    {XK_Help,VK_HELP},		/* Help */
    {XK_Num_Lock,VK_NUMLOCK},
    {XK_KP_Multiply,VK_MULTIPLY},
    {XK_KP_Add,VK_ADD},
    {XK_KP_Separator,VK_SEPARATOR},	/* Separator, often comma */
    {XK_KP_Subtract,VK_SUBTRACT},
    {XK_KP_Decimal,VK_DECIMAL},
    {XK_KP_Divide,VK_DIVIDE},
    {XK_KP_0,VK_NUMPAD0},
    {XK_KP_1,VK_NUMPAD1},
    {XK_KP_2,VK_NUMPAD2},
    {XK_KP_3,VK_NUMPAD3},
    {XK_KP_4,VK_NUMPAD4},
    {XK_KP_5,VK_NUMPAD5},
    {XK_KP_6,VK_NUMPAD6},
    {XK_KP_7,VK_NUMPAD7},
    {XK_KP_8,VK_NUMPAD8},
    {XK_KP_9,VK_NUMPAD9},
    {XK_F1,VK_F1},
    {XK_F2,VK_F2},
    {XK_F3,VK_F3},
    {XK_F4,VK_F4},
    {XK_F5,VK_F5},
    {XK_F6,VK_F6},
    {XK_F7,VK_F7},
    {XK_F8,VK_F8},
    {XK_F9,VK_F9},
    {XK_F10,VK_F10},
    {XK_F11,VK_F11},
    {XK_F12,VK_F12},
    {XK_F13,VK_F13},
    {XK_F14,VK_F14},
    {XK_F15,VK_F15},
    {XK_F16,VK_F16},
    {XK_F17,VK_F17},
    {XK_F18,VK_F18},
    {XK_F19,VK_F19},
    {XK_F20,VK_F20},
    {XK_F21,VK_F21},
    {XK_F22,VK_F22},
    {XK_F23,VK_F23},
    {XK_F24,VK_F24},
    {XK_Shift_L,VK_SHIFT},	/* Left shift */
    {XK_Shift_R,VK_SHIFT},	/* Right shift */
    {XK_Control_L,VK_CONTROL},	/* Left control */
    {XK_Control_R,VK_CONTROL},	/* Right control */
    {XK_Caps_Lock,VK_CAPITAL},	/* Caps lock */
    {XK_Eisu_toggle,VK_OEM_ATTN}, /*英数*/
    {XK_Alt_L,VK_MENU},		/* Left alt */
    {XK_Alt_R,VK_MENU},		/* Right alt */
    {XK_Super_L,VK_LWIN},	/* Left super */
    {XK_Super_R,VK_RWIN},	/* Right super */
    {XK_Zenkaku_Hankaku,VK_OEM_AUTO},	/*半角/全角*/
    {XK_Muhenkan,VK_NONCONVERT},	/*無変換*/
    {XK_Henkan_Mode,VK_CONVERT},	/*変換*/
    {XK_Hiragana_Katakana,VK_OEM_COPY},	/*カタカナ ひらがな*/
    {XK_Kanji,VK_KANJI},		/*Alt+半角/全角*/
    {XK_Romaji,VK_OEM_BACKTAB},		/*Alt+カタカナひらがな*/
    {XK_VoidSymbol,0}
};

static uint8_t* Xk2Vk[256];//vk上8bitの配列。各要素は下8bitの配列。ただし英数記号以外。

__attribute__((constructor))
void initkeymap(void)
{
    for (int n = 0; KeyMap[n][0] != XK_VoidSymbol; ++n) {
        uint8_t** p = Xk2Vk + (KeyMap[n][0] >> 8);
        if (*p == NULL)
            *p = calloc(256, 1);
        *(*p + (KeyMap[n][0] & 0xff)) = KeyMap[n][1];
    }
}

/*
  Xのkeysymをwinの仮想キーコードにする。上8ビットには修飾キーの状態が入る。
  keysym1=group2のkeysym。かなモードの時こちらを使う。
*/
unsigned xk_to_vk(unsigned keysym0, unsigned keysym1, unsigned state, HKL kl, unsigned convmode)
{
    unsigned vk = 0;
    unsigned keysym = ((convmode & IME_CMODE_ROMAN) != 0 || keysym1 == 0) ? keysym0 : keysym1;
    unsigned up = keysym >> 8;

    if (up <= 0xff && Xk2Vk[up] != NULL)
        vk = Xk2Vk[up][keysym & 0xff]; //主に制御用のkeysym
    if (vk == 0) {
        //かなのkeysymであればunicodeに直す。
        if (keysym >= XK_kana_fullstop && keysym <= XK_semivoicedsound)
            keysym = keysym - XK_kana_fullstop + 0xff61; //keysym→unicode
        vk = VkKeyScanExW(keysym, kl);
    }
    if (state & ShiftMask)
        vk |= VKMODKEY(WINMODKEY_SHIFT);
    if (state & ControlMask)
        vk |= VKMODKEY(WINMODKEY_CTRL);
    if (state & Mod1Mask)
        vk |= VKMODKEY(WINMODKEY_ALT);
    if (state & LockMask)
        vk |= VKMODKEY(WINMODKEY_LOCK);
    DEBUGLOG(CH_CANNA, "keysym 0x%x/0x%x-0x%x --> vk 0x%x\n", keysym0, keysym1, state, vk);
    return vk;
}

/*
  要求：type64
        [0]=コンテキスト番号
        [1]=keysym(group1)
        [2]=keysym(group2)
        [3]=Xの修飾キーの状態
  応答：type6
        i16=WIME_SENDKEY_XXX
        s8=imeに処理されたときの確定文字列(utf8)(あれば送られる)
*/
bool SendKey(CanHeader* ch, int fd UNUSED)
{
    int32_t* p = (typeof(p))(ch + 1);
    int16_t st = WIME_SENDKEY_ERROR;
    char* u8 = NULL;
    HIMC imc;
#define CXN p[0]
#define KEYSYM0 p[1]
#define KEYSYM1 p[2]
#define KEYSTATE p[3]

    CannaContext_t* cx = GetContext(CXN, &imc, __FUNCTION__);
    if (cx != NULL) {
        HKL kl = GetKeyboardLayout(0);
        unsigned convmode = conv_mode(imc);
        uint16_t vk = xk_to_vk(KEYSYM0, KEYSYM1, KEYSTATE, kl, convmode);

        /*ローマ字/かなの切り替えがImmProcessKeyでもSendInputでもできないので、ImmSetConversionStatusで
         変更する。*/
        if ((vk & 0xff) == VK_OEM_BACKTAB) {
            convmode ^= IME_CMODE_ROMAN;
            ImmSetConversionStatus(imc, convmode, IME_SMODE_PHRASEPREDICT);
            st = WIME_SENDKEY_SUCCESS; //[r268]
        }

        /*[r14]キー処理の後、変換キーではないのにWM_IME_NOTIFY,IMN_OPENCANDIDATEが飛んでくるときがある。
          関係ないときにWIME_SENDKEY_OPENCANDを返してしまうので、先にフラグをクリアしておく*/
        cx->Flags &= ~(CATCH_OPEN_CAND | CATCH_CHG_CAND | CATCH_FINISH);

        if (st == WIME_SENDKEY_SUCCESS || proc_key_vk(vk, cx->Win, kl, convmode)) { //[r268]
            st = WIME_SENDKEY_SUCCESS;
            cx->Flags |= SEND_KEY; //wnd_proc()参照
            Array u16;
            ClauseStr(imc, cx, GCS_RESULTSTR, 0, -1, ArNew(&u16, 2, NULL), false);
            u8 = U16ToU8(NULL, NULL, ArAdr(&u16), -1);
            ArDelete(&u16);
            DEBUGDO(CH_CANNA, DbgComp(imc, __func__));
        }
        else
            st = WIME_SENDKEY_NO_PROC;

        /*???
          再変換の時,この関数が終わる前にWM_IME_REQUESTが来る。
          proc_key_vk()のImmProcessKey()でメッセージループが回されるようだ。
          proc_key_vk()が通ったり通らなかったりよく分からないので、とにかくu8は解放する。
        */
        if (cx->Flags & PENDING_RECONV) {
            st = WIME_SENDKEY_RECONV;
            cx->Flags &= ~PENDING_RECONV;
            free(u8);
            u8 = NULL;
            DEBUGLOG(CH_CANNA, "reconvertion --> pending\n");
        }

        /*[r11]WM_IME_NOTIFY,IMN_OPENCANDIDATEはこの関数内では起こらず、次にメッセージループが回った
          ときに処理されるみたい。CATCH_OPEN_CANDがセットされるのはそのときになるので、候補ウィンドウが
          表示されたかがわかるのは次の呼び出しの時になってしまう。それでは1回余計に変換キーを押すことにな
          るので、WM_IME_NOTIFYを処理させる。*/
        cx->Flags &= ~(CATCH_OPEN_CAND | CATCH_CHG_CAND);
        flush_msg_loop();
        /* ウィンドウプロシージャでWM_IME_NOTIFY,IMN_OPENCANDIDATEがきたらCATCH_OPEN_CANDがセットされる。
           このフラグはGetCandiList()で消す */
        if (cx->Flags & CATCH_OPEN_CAND) {
            st = WIME_SENDKEY_OPENCAND;
        }
        if (cx->Flags & CATCH_CHG_CAND) {
            st = WIME_SENDKEY_CHGCAND;
            cx->Flags &= ~CATCH_CHG_CAND; //これはここで消しても問題ない。
        }

        /*'変換'や'無変換'を押して入力モードを変えるとキーはimeに処理されるが、imcの内容はそのまま。
          何もせずにいると前回のresult-strを返してしまう。メッセージループが回ると
          WM_IME_NOTIFY/IMN_SETCONVERSIONMODEが来るが、shiftを押しながら入力したときの一時英数モード
          でもこのメッセージが来る。一時英数モードではcomp-strにデータが入るが、'変換'を押したときは
          comp-strは空なのでこのときはresult-strを返さないようにする。
          一時英数モードで確定したときと区別がつかないので、WM_IME_COMPOSITIONでGCS_RESULT...を
          チェックする。
        */
        if ((cx->Flags & CATCH_FINISH) == 0 && ImcClauseInfo(imc, GCS_COMPSTR, NULL) < 0) {
            free(u8);
            u8 = NULL;
            DEBUGLOG(CH_CANNA, "catch composition-finish\n");
        }

        ImmReleaseContext(cx->Win, imc);
        DEBUGLOG(CH_CANNA, "context %d, wnd %p, vk 0x%hx --> proc_key status %hd\n", CXN, cx->Win, vk, st);
    }
    bool rep_st = Reply6s(ch->Major, ch->Minor, st, u8);
    free(u8);
    return rep_st;

#undef CXN
#undef KEYSYM0
#undef KEYSYM1
#undef KEYSTATE
}

/*
  変換候補ウィンドウの表示/非表示
  要求：type3
        i16=コンテキスト番号
        u16=bool
  応答：type2 成功したらtrue
  表示しないようにしたときは、WimeSendKey()で候補ウィンドウが表示されようとしたときにWIME_SENDKEY_OPENCANDが返されるので、GetCandiList()で変換候補を取得すること。

  候補ウィンドウが表示されるときにウィンドウプロシージャにWM_IME_NOTIFY,IMN_OPENCANDIDATEが送られるので、
  これがきたらcx->FlagsにCATCH_OPEN_CANDをセットする。
 */
bool ShowCandWin(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    uint16_t en;
    bool st = false;
    Req3(ch, &cxn, &en);
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL) {
        if (en)
            cx->Flags &= ~TRAP_OPEN_CAND;
        else
            cx->Flags |= TRAP_OPEN_CAND;
        st = true;
    }
    return Reply2(ch->Major, ch->Minor, st);
}

/*
  再変換
  要求：type11
        i16=コンテキスト番号
        i16=再変換文字列上のカーソル位置（文字単位）
        s16=再変換に使う文字列(u16)
  応答：type4
        i8=bool
        i32[0]=対象部分の開始位置(文字単位)
        i32[1]=対象部分の長さ（文字単位）
 */
bool Reconvert(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, cursor;
    HIMC imc;
    int32_t info[2] = { 0,0 };
    bool st = false;

    uint16_t* reconv = Req11(ch, &cxn, &cursor);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        DEBUGLOG(CH_CANNA, "cursor %hd, string '%W'\n", cursor, reconv);
        unsigned sz = (WcLen(reconv) + 1) * 2;
        unsigned bufsize = sizeof(RECONVERTSTRING) + sz;
        RECONVERTSTRING* rs = calloc(bufsize, 1);
        memcpy(rs + 1, reconv, sz);
        rs->dwStrLen = sz - 2;	//ヌル文字は除く
        rs->dwStrOffset = sizeof(*rs);
        rs->dwTargetStrOffset = cursor * 2;	//バイト単位
        st = ImmSetCompositionStringW(imc, SCS_QUERYRECONVERTSTRING, rs, bufsize, NULL, 0) && ImmSetCompositionStringW(imc, SCS_SETRECONVERTSTRING, rs, bufsize, NULL, 0);
        info[0] = rs->dwCompStrOffset / 2;	//??? atok08ではバイト数みたい
        info[1] = rs->dwCompStrLen;		//??? こっちは文字数みたい

        DEBUGDO(CH_CANNA, {
                MESG("status %d, CompStrOffset %d, CompStrLen %d\n",st,rs->dwCompStrOffset,rs->dwCompStrLen);
                DbgComp(imc,__func__); });

        ImmReleaseContext(cx->Win, imc);
        free(rs);
    }
    return Reply4(ch->Major, ch->Minor, st, info, 2);
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
bool MoveShadowWin(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, dum;
    bool st = false;

    int16_t* ax = (int16_t*)Req11(ch, &cxn, &dum);
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL) {
        unsigned flg = SWP_NOREDRAW; /*SWP_NOZORDER|SWP_HIDEWINDOW|*/
        if (ax[0] < 0 || ax[1] < 0)
            flg |= SWP_NOMOVE;
        if (ax[2] < 0 || ax[3] < 0)
            flg |= SWP_NOSIZE;
        st = SetWindowPos(cx->Win, HWND_TOP, ax[0], ax[1], ax[2], ax[3], flg);
        DEBUGLOG(CH_CANNA, "context %hd (%hd,%hd)-%hdx%hd --> status %d\n", cxn, ax[0], ax[1], ax[2], ax[3], st);
    }
    return Reply2(ch->Major, ch->Minor, st);
}

//xlfdのウェイト名を数値にする
//!!! mediumがFW_NORMALの400ではなく500になってしまうが、どうする？
int weight_value(const char* w, unsigned wlen)
{
    struct {
        char* name;
        int value;
    } tab[] = {
        {"thin",FW_THIN},{"extralight",FW_EXTRALIGHT}, {"ultraright",FW_ULTRALIGHT},{"light",FW_LIGHT},	{"normal",FW_NORMAL},{"regular",FW_REGULAR},{"medium",FW_MEDIUM},{"semibold",FW_SEMIBOLD}, {"demibold",FW_DEMIBOLD},{"bold",FW_BOLD},{"extrabold",FW_EXTRABOLD},{"ultrabold",FW_ULTRABOLD}, {"heavy",FW_HEAVY},{"black",FW_BLACK}
    };
    char wname[wlen + 1];
    memcpy(wname, w, wlen);
    wname[wlen] = 0;

    int v = FW_NORMAL;
    for (unsigned n = 0; n < ITEMS(tab); ++n) {
        if (strcasecmp(wname, tab[n].name) == 0) {
            v = tab[n].value;
            break;
        }
    }
    return v;
}

/*
  フォントセットのjisx0208からLOGFONTを設定する。
*/
bool fontset_to_logfont(LOGFONT* lf, const char* fs)
{
    char* pt[14] = { NULL };
    char jx0208[] = "jisx0208";
    bool st = false;

    *lf = (typeof(*lf)){ 0 };
    lf->lfCharSet = SHIFTJIS_CHARSET;
    lf->lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf->lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf->lfWeight = FW_NORMAL;

    while (fs != NULL) {
        //xlfdの要素の先頭アドレスをpt[]に入れる
        for (int n = 0; n < ITEMS(pt); ++n) {
            if ((pt[n] = strchr(fs, '-')) == NULL || *(fs = ++pt[n]) == 0)
                break;
        }

        if (strncasecmp(pt[12], jx0208, sizeof(jx0208) - 1) == 0) {
            size_t len = pt[2] - pt[0] - 1;
            memcpy(lf->lfFaceName, pt[0], len);
            lf->lfFaceName[len] = 0;
            lf->lfFaceName[pt[1] - pt[0] - 1] = ' ';

            if ((lf->lfHeight = atoi(pt[6])) == 0)
                lf->lfHeight = 16; //!!! なんとかしなければ
            lf->lfWeight = weight_value(pt[2], pt[3] - pt[2] - 1);
            if (*pt[3] == 'i')
                lf->lfItalic = TRUE;
            st = true;
            break;
        }

        fs = strchr(fs, ',');
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
bool SetCompFont(CanHeader* ch, int fd UNUSED)
{
    int16_t h = 0, cxn;
    uint32_t bg;
    HIMC imc;

    ERRORLOG(CH_CANNA, "*** PARTIAL IMPLIMENT ***\n");
    char* fontname = Req15(ch, (int32_t*)&bg, &cxn);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        LOGFONT lf;

        DEBUGLOG(CH_CANNA, "fontset '%s'\n", fontname);
        if (fontset_to_logfont(&lf, fontname)) {
            DEBUGLOG(CH_CANNA, "alias name '%s', size %d, weight %d, italic=%d\n", lf.lfFaceName, lf.lfHeight, lf.lfWeight, lf.lfItalic);
            if (!ImmSetCompositionFont(imc, &lf))
                ERRORLOG(CH_CANNA, "fail ImmSetCompositionFont()\n");
        }
        else
            ERRORLOG(CH_CANNA, "fail fontset_to_logfont()\n");

        ImmGetCompositionFont(imc, &lf);
        h = abs(lf.lfHeight);
        DEBUGLOG(CH_CANNA, "facename '%s', height %d\n", lf.lfFaceName, lf.lfHeight);

        ImmReleaseContext(cx->Win, imc);
    }
    return Reply5(ch->Major, ch->Minor, h);
}

/*
  imeを使用する
  要求：type3
        i16	コンテキスト番号
        u16	bool(設定),あるいは-1(問い合わせ)
  応答：type5
        i16	u16が-1のとき現在の状態(0/1),boolのとき1,エラーの時-1
*/
bool EnableIme(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, st = -1;
    uint16_t en_ime;
    HIMC imc;

    Req3(ch, &cxn, &en_ime);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        if (en_ime == (uint16_t)-1) {
            st = ImmGetOpenStatus(imc);
            DEBUGLOG(CH_CANNA, "context %hd, open status %hd\n", cxn, st);
        }
        else {
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
            if (!en_ime)
                SendMessageW(cx->ImeWnd, WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 0);

            st = ImmSetOpenStatus(imc, en_ime) ? 1 : -1;
            DEBUGLOG(CH_CANNA, "context %hd, en_ime %hd, status %hd\n", cxn, en_ime, st);
        }
        ImmReleaseContext(cx->Win, imc);
    }
    return Reply5(ch->Major, ch->Minor, st);
}

//ツールバーを使っている別のコンテキストを探す。
static int another_tb_user(const void* elem, const void* val)
{
    const CannaContext_t* c = elem;
    return c->Win != NULL && c->UseToolbar && c != (CannaContext_t*)val;
}

void hide_toolbar(CannaContext_t* cx)
{
    //ツールバーを使っている別のコンテキストがなくなったらツールバーを非表示にする。
    if (ArFindIf(&Context, 0, another_tb_user, cx) < 0) {
        SendMessageW(cx->Win, WM_IME_NOTIFY, IMN_CLOSESTATUSWINDOW, 0);
    }
    cx->UseToolbar = false;
}

/*
  imeのツールバーを表示する
  要求：type7
        i16	コンテキスト番号
        i16	bool ツールバーを表示
        i16	bool 変換ウィンドウを使う
  応答：無し
*/
bool ShowToolbar(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, show_tb, show_comp_win;
    HIMC imc;
    bool st = false;

    Req7(ch, &cxn, &show_tb, &show_comp_win);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        DEBUGLOG(CH_CANNA, "context %hd, use-tb %hd, use-cw %hd, ime-win %p\n", cxn, show_tb, show_comp_win, cx->ImeWnd);
        if (show_tb) {
            BringWindowToTop(cx->Win); //影窓が重なっていると候補ウィンドウが隠されてしまうので一番上にする
            cx->Flags &= ~PROC_COMP_MSG;
            if (show_comp_win)
                cx->Flags |= PROC_COMP_MSG;
            cx->Flags |= PROC_NOTIFY_MSG;
            SendMessageW(cx->Win, WM_IME_NOTIFY, IMN_OPENSTATUSWINDOW, 0);
            cx->UseToolbar = true;
        }
        else {
            hide_toolbar(cx);//SendMessageW(cx->Win,WM_IME_NOTIFY,IMN_CLOSESTATUSWINDOW,0);
            cx->Flags &= ~(PROC_COMP_MSG | PROC_NOTIFY_MSG);
        }
        ImmReleaseContext(cx->Win, imc);
        st = true;
    }
    return st;
}

/*
  フォーカスの移動を知らせる
  要求:type64
        i32 p0	cxn
        i32 p1	bool(in=true,out=false)
  応答：なし
*/
bool SetImeFocus(CanHeader* ch, int fd UNUSED)
{
    int32_t* p = (typeof(p))(ch + 1);
    HIMC imc;
    bool st = false;
#define CXN p[0]
#define FOCUS_IN p[1]

    CannaContext_t* cx = GetContext(CXN, &imc, __FUNCTION__);
    if (cx != NULL) {
        DEBUGLOG(CH_CANNA, "context %d, wnd %p, ime-wnd %p, imc %p, focus --> %s\n", CXN, cx->Win, cx->ImeWnd, imc, FOCUS_IN ? "in" : "out");
        if (FOCUS_IN) {
            DEBUGLOG(CH_CANNA, "open=%d\n", cx->ImeOpen);
            ImmSetOpenStatus(imc, cx->ImeOpen);
            SetFocus(cx->Win);
            CreateCaret(cx->Win, NULL, 0, 0);
            cx->UseToolbar = true;
        }
        else {
            DestroyCaret();
            cx->ImeOpen = ImmGetOpenStatus(imc);
            DEBUGLOG(CH_CANNA, "open=%d\n", cx->ImeOpen);
        }
        ImmReleaseContext(cx->Win, imc);
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
  応答：type64
        p1		エラー=0,変換中文字列がある=1,ない=-1
        databytes	sizeof(WimeCompStrInfo)
        bindata		WimeCompStrInfo
        str		変換中文字列(utf8)
*/
bool GetCompStr(CanHeader* ch, int fd UNUSED)
{
    HIMC imc;
    WimeCompStrInfo si = { .TargetClause = -1 };
    int ret_code = 0;
    char* cs_u8 = NULL;
    int16_t cxn = Req2(ch);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        Array comp_str;
        ClauseStr(imc, cx, GCS_COMPSTR, 0, -1, ArNew(&comp_str, 2, NULL), false);
        int targetcl = si.TargetNum = GetAttrCl(imc, ATTR_TARGET_CONVERTED, cx);
        if (targetcl < 0)
            targetcl = GetAttrCl(imc, ATTR_TARGET_NOTCONVERTED, cx);
        if (targetcl >= 0) {
            Array cl_info;
            ImcClauseInfo(imc, GCS_COMPSTR, ArNew(&cl_info, 4, NULL));
            si.TargetClause = ARVAL(int32_t, &cl_info, targetcl);
            si.TargetClLen = ARVAL(int32_t, &cl_info, targetcl + 1) - si.TargetClause;
            ArDelete(&cl_info);
        }
        si.Length = ArUsing(&comp_str) - 1;

        //CursorPosとDeltaStartを取得
        INPUTCONTEXT* ic = ImmLockIMC(imc);
        COMPOSITIONSTRING* cs = ImmLockIMCC(ic->hCompStr);
        si.CursorPos = cs->dwCursorPos;
        si.DeltaStart = cs->dwDeltaStart;
        ImmUnlockIMCC(ic->hCompStr);
        ImmUnlockIMC(imc);

        ImmReleaseContext(cx->Win, imc);

        if (si.Length > 0) {
            cs_u8 = U16ToU8(NULL, NULL, ArAdr(&comp_str), -1);
            ret_code = 1;
            DEBUGLOG(CH_CANNA, "'%U' cursor %d delta %d target-cl %d cl-len %d len %d target# %d\n", cs_u8, si.CursorPos, si.DeltaStart, si.TargetClause, si.TargetClLen, si.Length, si.TargetNum);
        }
        else {
            ret_code = -1;
            DEBUGLOG(CH_CANNA, "(none)\n");
        }
        ArDelete(&comp_str);
    }

    bool st = Reply64(ch->Major, ch->Minor, ret_code, &si, sizeof(si), cs_u8, -1);
    free(cs_u8);
    return st;
}

/*
  変換ウィンドウの情報
  要求：type2
        i16=コンテキスト番号
  応答：type4
        i8	エラー=0
        i32[0]	スタイル(WIME_POS_xxx)
        i32[1]	x
        i32[2]	y
        i32[3]	w
        i32[4]	h

        使わない座標データは-1
*/
bool GetCompWin(CanHeader* ch, int fd UNUSED)
{
    HIMC imc;
    int32_t v[5] = { -1 };
    char st = 0;
    int16_t cxn = Req2(ch);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        COMPOSITIONFORM cf;
        if (ImmGetCompositionWindow(imc, &cf)) {
            int32_t* vp = v;
            st = 1;
            switch (cf.dwStyle) {
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
            switch (cf.dwStyle) {
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
            DEBUGLOG(CH_CANNA, "data [%#*.4D]\n", ITEMS(v), v);
        }
        else
            ERRORLOG(CH_CANNA, "fail ImmGetCompositionWindow\n");
        ImmReleaseContext(cx->Win, imc);
    }
    return Reply4(ch->Major, ch->Minor, st, v, ITEMS(v));
}

/*
  ImmSetCandidateWindow
  要求：type64
        [0]=コンテキスト番号
        [1]=style WIME_POS_DEFAULT,WIME_POS_POINT,WIME_POS_EXCLUDE
        [2,3]=x,y  (POINT,EXCLUDE)
        [4,5,6,7]=x,y,w,h (EXCLUDE。r,bではなくw,hで指定する)
  応答：type2
        bool
  キャレットの移動も行う。
*/
bool SetCandWin(CanHeader* ch, int fd UNUSED)
{
    HIMC imc;
    bool st = false;
    int32_t* p = (typeof(p))(ch + 1);
#define CXN p[0]
#define STYLE p[1]
#define POSX p[2]
#define POSY p[3]
#define EX p[4]
#define EY p[5]
#define EW p[6]
#define EH p[7]
    CannaContext_t* cx = GetContext(CXN, &imc, __FUNCTION__);
    if (cx != NULL) {
        CANDIDATEFORM cf = { .dwIndex = 0,.dwStyle = CFS_DEFAULT };
        if (STYLE == WIME_POS_DEFAULT) {
            DEBUGLOG(CH_CANNA, "CFS_DEFAULT\n");
        }
        else {
            cf.ptCurrentPos = (POINT){ POSX,POSY };
            DEBUGLOG(CH_CANNA, "pos (%d,%d)\n", cf.ptCurrentPos.x, cf.ptCurrentPos.y);
            switch (STYLE) {
            case WIME_POS_POINT:
                cf.dwStyle = CFS_CANDIDATEPOS;
                break;
            case WIME_POS_EXCLUDE:
                cf.dwStyle = CFS_EXCLUDE;
                cf.rcArea = (RECT){ EX,EY, EX + EW - 1,EY + EH - 1 };
                DEBUGLOG(CH_CANNA, "rect (%d,%d)-(%d,%d)\n", cf.rcArea.left, cf.rcArea.top, cf.rcArea.right, cf.rcArea.bottom);
            }
        }
        st = ImmSetCandidateWindow(imc, &cf);
        ImmReleaseContext(cx->Win, imc);
        if (STYLE != WIME_POS_DEFAULT)
            SetCaretPos(cf.ptCurrentPos.x, cf.ptCurrentPos.y);
        DEBUGLOG(CH_CANNA, "context %d, wnd %p, ret %d\n", CXN, cx->Win, st);
    }
    return Reply2(ch->Major, ch->Minor, st);
#undef CXN
#undef STYLE
#undef POSX
#undef POSY
#undef EX
#undef EY
#undef EW
#undef EH
}

/*
  cannaコンテキストとXウィンドウを関連づける。このウィンドウに何か情報を送る。
  atokのパレットツールからの入力の時にImAuxInput()で使っている。
  要求:PktRegXWin
  応答:なし
*/
bool RegXWin(CanHeader* ch, int fd UNUSED)
{
    bool st = false;
    PktRegXWin* p = (typeof(p))(ch + 1);
    CannaContext_t* cx = ValidContext(p->cxn, __FUNCTION__);
    if (cx != NULL) {
        cx->XWin = p->xwin;
        st = true;
        DEBUGLOG(CH_CANNA, "context %d, window %x\n", p->cxn, p->xwin);
    }
    return st;
}

/*
  変換確定文字列を得る。
  要求：type2
        i16	コンテクスト番号
  応答：確定文字列(u16le,ヌル文字付き)
*/
bool GetResultStr(CanHeader* ch, int fd UNUSED)
{
    HIMC imc;
    Array str;

    ArNew(&str, 2, NULL);
    CannaContext_t* cx = GetContext(Req2(ch), &imc, __FUNCTION__);
    if (cx != NULL) {
        ClauseStr(imc, cx, GCS_RESULTSTR, 0, -1, &str, false);
        ImmReleaseContext(cx->Win, imc);
        DEBUGLOG(CH_CANNA, "result str(u16)=%W\n", ArAdr(&str));
    }
    bool st = ReplyN(ch->Major, ch->Minor, ArAdr(&str), ArUsingBytes(&str));
    ArDelete(&str);
    return st;
}

/*
  単語登録に使う品詞の一覧を得る
  要求：なし
  応答：type64
        p1		データの数
        databytes	配列のバイト数(p1*sizeof(int))
        bindata		コードの配列
        str		品詞名のリスト(utf8)
*/
bool GetStyleList(CanHeader* ch, int fd UNUSED)
{
    HKL kl = GetKeyboardLayout(0);
    int count = ImmGetRegisterWordStyleW(kl, 0, NULL);
    STYLEBUFW* sty = calloc(sizeof(*sty), count);
    int* code = calloc(sizeof(int), count);
    Array desclist;
    ArNew(&desclist, 1, NULL);
    ImmGetRegisterWordStyleW(kl, count, sty);

    DEBUGLOG(CH_CANNA, "%d items\n", count);
    for (int index = 0; index < count; ++index) {
        code[index] = sty[index].dwStyle;
        char* desc = U16ToU8(NULL, NULL, (uint16_t*)sty[index].szDescription, -1);
        ArAddStr(&desclist, desc);
        free(desc);
    }
    ArAddChar(&desclist, 0);

    bool st = Reply64(ch->Major, ch->Minor, count, code, sizeof(int) * count, ArAdr(&desclist), ArUsing(&desclist));
    free(sty);
    free(code);
    ArDelete(&desclist);
    return st;
}

/*
  設定ファイルを再読み込みする。
  imcを作り直す。
  要求：なし
  応答：int	0=成功
  ??? エラーコードでも返すか？
*/
bool ReloadConf(CanHeader* ch, int fd UNUSED)
{
    int st = ImReadSetting(&WimeData);
    ReplaceWindow();
    DEBUGLOG(CH_CANNA, "reload setting file %d\n", st);
    return ReplyN(ch->Major, ch->Minor, &st, sizeof(st));
}

/*
  メッセージループを回す
  要求：なし
  応答：int	0=全部処理した 1=途中で中断した
  imeが内部でメッセージを送ることがある(SendKeyなどでimeを操作している場合は特に)。
  それらのメッセージが処理される前に終了処理が行われると、ウィンドウなどが表示されたままになることがある。
  終了前にはこの関数を呼んでおいた方がいい。
  !!! ime offのタイミングで自動的に呼び出すようにするべきか？こんな処理は表に出すべきではない。
*/
bool FlushMsg(CanHeader* ch, int fd UNUSED)
{
    int st = flush_msg_loop() ? 0 : 1;
    return ReplyN(ch->Major, ch->Minor, &st, sizeof(st));
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
bool SelectCand(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn;
    bool st = false;
    uint16_t index;
    HIMC imc;

    Req3(ch, &cxn, &index);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        DEBUGLOG(CH_CANNA, "context %hd, index %hd\n", cxn, index);
        if (ImmNotifyIME(imc, NI_SELECTCANDIDATESTR, 0, index + WimeData.CandIndexStart)) {
            st = true;
            DEBUGDO(CH_CANNA, DbgComp(imc, __func__));
        }
        ImmReleaseContext(cx->Win, imc);
    }
    return Reply2(ch->Major, ch->Minor, st);
}

/*
  変換候補ウィンドウを閉じる。
  SendKeyで自動的に開いたウィンドウを閉じるために使う。
  ??? showもcloseもset_cand_winにまとめてしまったらどうだろう?
  要求：type2
        i16=コンテキスト番号
  応答:なし
*/
bool CloseCandWin(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn = Req2(ch);
    CannaContext_t* cx = ValidContext(cxn, __FUNCTION__);
    if (cx != NULL) {
        //wm_wime_enable_imeのコメント参照。
        SendMessageW(cx->ImeWnd, WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 0);
    }
    return true;
}

/*
  文字列を外部入力とする。
  要求：type11
        i16	コンテクスト番号
        i16	未使用
        s16	文字列(utf16)
  応答：なし
*/
bool SetResultStr(CanHeader* ch, int fd UNUSED)
{
    int16_t cxn, dum;
    HIMC imc;
    bool st = false;

    uint16_t* extstr = Req11(ch, &cxn, &dum);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        DEBUGLOG(CH_CANNA, "context %hd, string '%W'\n", cxn, extstr);
#ifdef SETCONTEXT_FAIL
        SetCurrentImc(imc, TRUE);
#endif
        ImmSetCompositionStringW(imc, SCS_QUERYRECONVERTSTRING, extstr, WcLen(extstr) * 2, NULL, 0);
        DEBUGDO(CH_CANNA, DbgComp(imc, __func__));
        ImmSetCompositionStringW(imc, SCS_SETRECONVERTSTRING, extstr, WcLen(extstr) * 2, NULL, 0);
        DEBUGDO(CH_CANNA, DbgComp(imc, __func__));
        ImmNotifyIME(imc, NI_COMPOSITIONSTR, CPS_CONVERT, 0);
        DEBUGDO(CH_CANNA, DbgComp(imc, __func__));
        ImmReleaseContext(cx->Win, imc);
        st = true;
    }
    return st;
}

/*
  CannaContext_t.Flagsを出力する(後で変わるかも)。デバッグ用。
  要求：type6
        i16	true=set false=get
        i16	setのときコンテキスト番号。getのとき無視。
        u16	セットするフラグ。
  応答：type9
        i16	出力したコンテキストの数。セットのときは0
        i32[]	{コンテキスト番号,cx->Flags} {p1}個
*/
bool DumpContext(CanHeader* ch, int fd UNUSED)
{
    CannaContext_t* cx;
    int16_t do_set, cxn;
    uint16_t flags;
    Array dump;

    ArNew(&dump, sizeof(int32_t) * 2, NULL); //int32×2個
    Req6(ch, &do_set, &cxn, &flags);
    DEBUGLOG(CH_CANNA, "set/get %hd, context %hd, flags 0x%hx\n", do_set, cxn, flags);
    if (do_set) {
        if ((cx = ValidContext(cxn, __FUNCTION__)) != NULL) {
            cx->Flags = flags;
        }
        else {
            DEBUGLOG(CH_CANNA, "invalid context number %hd\n", cxn);
        }
    }
    else {
        for (int n = 0; n < ArUsing(&Context); ++n) {
            cx = ArElem(&Context, n);
            if (cx->Win != NULL) {
                int32_t* dp = ArExpand(&dump, 1);
                *(dp++) = n;
                *dp = cx->Flags;
            }
        }
    }
    bool st = Reply9(ch->Major, ch->Minor, ArUsing(&dump), ArAdr(&dump), ArUsing(&dump) * 2);
    ArDelete(&dump);
    return st;
}

/*
  verboseレベルとchannelを設定する。
  要求：type5
        i16	verbose level
        u16	(未使用)
        i32	channel
  応答：なし
 */
bool SetDebugChannel(CanHeader* ch, int fd UNUSED)
{
    int16_t lv;
    uint16_t dum;
    int32_t channel;
    Req5(ch, &lv, &dum, &channel);
    Verbose = lv;
    DebugChannel = channel;
    return true;
}

/*
  前編集文字列の表示色を取得する。
  配列tblの大きさは最低ATIMECOMPCOL_ITEMMAX個なければならない。
  失敗したときはデフォルトの値をセットする。
  要求：type2
        i16	コンテクスト番号
  応答：type6
        i16	bool
        s8	色データ(ATImeColの配列) sizeof(ATImeCol)*ATIMECOMPCOL_ITEMMAX バイト
 */
bool GetColor(CanHeader* ch, int fd UNUSED)
{
    DEBUGLOG(CH_CANNA, "return default color with false.\n");
    /*
      常に失敗し、デフォルト値(前景色黒、背景色白)を返す。atokが初期化されているときはat.cの関数に置き換えられる。
     */
    ATImeCol col[ATIMECOMPCOL_ITEMMAX];
    for (int index = 0; index < ATIMECOMPCOL_ITEMMAX; ++index) {
        col[index].Back = RGB(255, 255, 255); //白
        col[index].Text = RGB(0, 0, 0); //黒
        col[index].UnderLine = true;
    }
    col[ATCOLINDEX_TARGETCONVERT].Back = RGB(0, 0, 0);
    col[ATCOLINDEX_TARGETCONVERT].Text = RGB(255, 255, 255);
    col[ATCOLINDEX_TARGETCONVERT].UnderLine = false;
    return Reply6(ch->Major, ch->Minor, false, (const char*)col, sizeof(col));
}

/*
  ImmGetCandidateWindow
  要求：type2
        i16	コンテクスト番号
  応答：type9
        i16	bool(ImmGetCandidateWindowの戻り値)
        i32[0]	index
        i32[1]	type
        i32[2]	POINT.x
        i32[3]	POINT.y
        i32[4]	RECT.left
        i32[5]	RECT.top
        i32[6]	RECT.right
        i32[7]	RECT.bottom
 */
bool GetCandWin(CanHeader* ch, int fd UNUSED)
{
    HIMC imc;
    int32_t cf[8];
    bool st = false;
    int16_t cxn = Req2(ch);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        CANDIDATEFORM candform;
        st = ImmGetCandidateWindow(imc, 0, &candform);
        cf[0] = candform.dwIndex;
        cf[1] = candform.dwStyle;
        cf[2] = candform.ptCurrentPos.x;
        cf[3] = candform.ptCurrentPos.y;
        cf[4] = candform.rcArea.left;
        cf[5] = candform.rcArea.top;
        cf[6] = candform.rcArea.right;
        cf[7] = candform.rcArea.bottom;
        ImmReleaseContext(cx->Win, imc);
        DEBUGLOG(CH_CANNA, "status %d\n", st);
    }
    return Reply9(ch->Major, ch->Minor, st, cf, ITEMS(cf));
}

/*
  現在選択中の候補の番号を返す。
  要求：type2
        i16	コンテクスト番号
  応答:type5
        i16	候補番号。エラーがあれば-1
*/
bool CandIndex(CanHeader* ch, int fd UNUSED)
{
    bool st = false;
    HIMC imc;
    int16_t ans = -1;
    int16_t cxn = Req2(ch);
    CannaContext_t* cx = GetContext(cxn, &imc, __FUNCTION__);
    if (cx != NULL) {
        int sz = ImmGetCandidateList(imc, 0, NULL, 0);
        if (sz != 0) {
            CANDIDATELIST* cl = malloc(sz);
            ImmGetCandidateList(imc, 0, cl, sz);
            ans = cl->dwSelection;
            free(cl);
        }
        st = Reply5(ch->Major, ch->Minor, ans);
    }
    return st;
}

//(C) 2008 thomas
