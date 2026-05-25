// -*- coding:euc-jp -*-
#define _GNU_SOURCE //memmem()
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include "list.h"

static inline bool is_nul(const Array* slist, int pos)
{
    return memcmp(ArElem(slist, pos), &(int) { 0 }, ArBlockSize(slist)) == 0;
}

static inline int nul_pos(const Array* slist, int start_pos)
{
    return ArFind(slist, start_pos, &(int){0});
}

//リストslistにリストplistが含まれていれば開始メンバ番号を返す。無ければ-1を返す。
int SubList(const Array* slist, const Array* plist)
{
    if (slist == NULL || plist == NULL)
        return -1;

    int slist_len = ArUsing(slist) - 1;
    int plist_len = ArUsing(plist) - 1;
    if (slist_len <= 0 || plist_len <= 0) //どちらかが空なら失敗とする。
        return -1;

    int index = 0; //メンバの番号
    int slist_pos = 0; //メンバの先頭位置
    bool found = false;
    while (slist_len >= plist_len) {
        if (memcmp(ArElem(slist, slist_pos), plist->adr, plist_len * ArBlockSize(plist)) == 0) {
            found = true;
            break;
        }
        int slist_new_pos = nul_pos(slist, slist_pos) + 1; //ヌル文字の次
        slist_len -= (slist_new_pos - slist_pos) / ArBlockSize(slist);
        slist_pos = slist_new_pos;
        ++index;
    }
    return found ? index : -1;
}

//メンバの数を返す
int ListCount(const Array* slist)
{
    if (slist == NULL || ArUsing(slist) == 0)
        return 0;

    int mem = 1;
    int pos = 0;
    while (pos = nul_pos(slist, pos) + 1, !is_nul(slist, pos)) {
        ++mem;
    }
    return mem;
}

//index番目のメンバのアドレスを返す。
void* ListInc(const Array* slist, int index)
{
    if (slist == NULL || ArUsing(slist) == 0 || index < 0)
        return NULL;

    int pos = 0;
    while (index > 0 && !is_nul(slist, pos)) {
        pos = nul_pos(slist, pos) + 1;
        --index;
    }
    return index >= 0 && !is_nul(slist, pos) ? ArElem(slist, pos) : NULL;
}

/*
  start(>=0)番目からend番目のメンバを削除する。
*/
Array* ListRemoveRange(Array* slist, int start, int end)
{
    int posb = ArIndex(slist, ListInc(slist, start));
    int pose = nul_pos(slist, ArIndex(slist, ListInc(slist, end))) + 1;
    ArRemove(slist, posb, pose - posb);
    if (ArUsing(slist) == 1) {
        ArClear(slist); //リスト終了マークだけになった→クリアする。
    }
    return slist;
}

/*
  memがメンバであれば番号を返す。メンバでなかったら-1を返す。
*/
int ListFind(const Array* slist, const Array* mem)
{
    Array cpmem;
    ArNew(&cpmem, ArBlockSize(mem), mem->constructor);
    ArCopy(&cpmem, mem);
    ArAdd1(&cpmem, &(int){0}); //リストにする。
    int index = SubList(slist, &cpmem);
    ArDelete(&cpmem);
    return index;
}

/*
  位置indexにmemを挿入する
  index<0のときリストの最後に追加する
*/
Array* ListInsert(Array* slist, int index, const Array* mem)
{
    if (index >= 0) {
        ArInsert(slist,
            ArIndex(slist, ListInc(slist, index)),
            ArUsing(mem),
            ArAdrC(mem));
    }
    else {
        ArAdd1(ArAddAr(ArDec(slist)/*リストの終了マークを削除*/,
            mem), /*memを付け足す*/
            &(int) {
            0
        }); /*リストの終了マークを付ける*/
    }
    return slist;
}

/*
  リストを作る。memにはリスト終了マークを付けておくこと。
  slistは初期化せず、後ろに追加する。
 */
Array* ListRaw(Array* slist, const void* mem)
{
    void* listend = memmem(mem, INT_MAX, &(int){0}, 2 * ArBlockSize(slist));
    int listlen = (char*)listend + 2 * ArBlockSize(slist) - (char*)mem;
    return ArAddN(slist, mem, listlen / ArBlockSize(slist));
}

//(C) 2008 thomas
