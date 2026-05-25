// -*- coding:euc-jp -*-
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "array.h"

    //リストslistにリストplistが含まれていれば開始メンバ番号を返す。無ければ-1を返す。
    int SubList(const Array* slist, const Array* plist);

    //メンバの数を返す
    int ListCount(const Array* slist);

    //index番目のメンバのアドレスを返す。
    void* ListInc(const Array* slist, int index);

    /*
      start(>=0)番目からend番目のメンバを削除する。
    */
    Array* ListRemoveRange(Array* slist, int start, int end);

    /*
      index(>=0)番目のメンバを削除する。
    */
    static inline Array* ListRemove(Array* slist, int index) {
        return ListRemoveRange(slist, index, index);
    }

    /*
      memがメンバであれば番号を返す。メンバでなかったら-1を返す。
    */
    int ListFind(const Array* slist, const Array* mem);

    /*
      位置indexにmemを挿入する
      index<0のときリストの最後に追加する
    */
    Array* ListInsert(Array* slist, int index, const Array* mem);

    /*
      リストを作る。memにはリスト終了マークを付けておくこと。
      slistは初期化せず、後ろに追加する。
     */
    Array* ListRaw(Array* slist, const void* mem);

#ifdef __cplusplus
}
#endif

//(C) 2008 thomas
