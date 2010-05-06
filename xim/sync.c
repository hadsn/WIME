#include "wimexim.h"

int SyncReply(WxContext* cx,XimImIc* pkt)
{
    LOG("im-id=%hd ic-id=%hd\n",pkt->imid,pkt->icid);
    send_n(cx->Client,XIM_SYNC_REPLY,pkt,sizeof(*pkt));
    return 0;
}
