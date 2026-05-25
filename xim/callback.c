
#include "x.h"
#include "wimexim.h"
#include "lib/log.h"
#include "lib/ut.h"

/* 前編集状態コールバックの説明文から:
   PreeditStartCallbackは前編集文字列の最大長を返す。正の数は前編集文字列に許される最大のバイト数を示し、-1 はバイト数が無制限であることを示す。
   なので、XIM_PREEDIT_START_REPLYが返した数値を考慮しなければならない。
*/
int PreeditStartReply(WxContext* cx UNUSED,XimPreeditStartReply* pkt)
{
    DEBUGLOG(CH_XIM,"im %hd,ic %hd  value=%d(0x%x)\n",pkt->imid,pkt->icid,pkt->value,pkt->value);
    return 0;
}

//(C) 2009 thomas
