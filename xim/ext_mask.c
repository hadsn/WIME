#include "wimexim.h"
#include "lib/log.h"

int ExtSetEventMask(WxContext* cx,XimExtSetEventMask* pkt)
{
    DEBUGLOG(CH_XIM,"im %hd ic %hd\n",pkt->imid,pkt->icid);
    DEBUGLOG(CH_XIM,"filter 0x%x, intercept 0x%x, select 0x%x, forward 0x%x, sync 0x%x\n",
	     pkt->filter_event_mask,pkt->intercept_event_mask,pkt->select_event_mask,pkt->forward_event_mask,pkt->sync_event_mask);

    SendW(cx->Client,XIM_SYNC_REPLY,pkt->imid,pkt->icid);
    return 0;
}
