#include "wimexim.h"
#include "lib/log.h"

int ExtMove(WxContext* cx,XimExtMove* pkt)
{
    DEBUGLOG(CH_XIM,"ext move %hd,%hd\n",pkt->x,pkt->y);
    IcData* icp = ArElem(&cx->Ic,pkt->icid-1);
    icp->ExtPosX = pkt->x;
    icp->ExtPosY = pkt->y;
    return 0;
}
