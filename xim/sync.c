#include "wimexim.h"
#include "lib/log.h"

int SyncReply(WxContext* cx,XimImIc* pkt)
{
    DEBUGLOG(CH_XIM,"im-id=%hd ic-id=%hd\n",pkt->imid,pkt->icid);
    SendN(cx->Client,XIM_SYNC_REPLY,pkt,sizeof(*pkt));
    return 0;
}

//(C) 2009 thomas
