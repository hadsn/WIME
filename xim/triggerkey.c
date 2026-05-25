
#include <X11/keysym.h>
#include "x.h"
#include "wimexim.h"
#include "lib/log.h"
#include <string.h>
#include <stdlib.h>
#include "so/xres.h"

typedef struct {
    uint32_t keysym;
    uint32_t modifier;
    uint32_t modifier_mask;
}__attribute__((packed)) XimTriggerKey;

typedef struct {
    XimHeader	h;
    uint16_t	imid;
    uint16_t	unused;
    //uint32_t	sz_onkeys;
    //XimTriggerKey on_key;	
    //uint32_t	sz_offkeys;	
    //XimTriggerKey off_key;	
}__attribute__((packed)) XimRegisterTriggerKey;

extern Array ContextList;
extern ToggleKey* ToggleKeys;

int RegTriggerKeys(WxContext* cx)
{
    int tks = 0;
    for (ToggleKey* k = ToggleKeys; k->Key != 0; ++k)
        ++tks;
    unsigned size = sizeof(XimRegisterTriggerKey) + (sizeof(uint32_t) + sizeof(XimTriggerKey) * tks) * 2;
    XimRegisterTriggerKey* r = malloc(size);

    uint32_t* sz_onkeys = (uint32_t*)(r + 1);
    XimTriggerKey* on_key = (XimTriggerKey*)(sz_onkeys + 1);
    uint32_t* sz_offkeys = (uint32_t*)(on_key + tks);
    XimTriggerKey* off_key = (XimTriggerKey*)(sz_offkeys + 1);

    XimTriggerKey tk[tks];
    for (int n = 0; n < tks; ++n) {
        tk[n].keysym = ToggleKeys[n].Key;
        tk[n].modifier = tk[n].modifier_mask = ToggleKeys[n].Mod;
    }

    r->imid = ArIndex(&ContextList, cx) + 1;
    *sz_onkeys = *sz_offkeys = tks;
    memcpy(on_key, tk, sizeof(tk));
    memcpy(off_key, tk, sizeof(tk));
    SendN(cx->Client, XIM_REGISTER_TRIGGERKEYS, r, size);
    free(r);
    DEBUGLOG(CH_XIM, "client %lx, %d keys\n", cx->Client, tks);
    return 0;
}

//onキーがこないのはなぜ？
int TriggerNotify(WxContext* cx, XimTriggerNotify* pkt)
{
    DEBUGLOG(CH_XIM, "im-id=%hd ic-id=%hd flag=0x%x key=%d ev-mask=0x%x\n", pkt->imid, pkt->icid, pkt->flag, pkt->keys_list, pkt->event_mask);
    if ((cx->Flags ^= ICF_IME_ENABLE) & ICF_IME_ENABLE) {
        //ツールバーを表示する
        DEBUGLOG(CH_XIM, "	kanji on\n");
    }
    else {
        //ツールバーを消す
        DEBUGLOG(CH_XIM, "	kanji off\n");
    }
    SendW(cx->Client, XIM_TRIGGER_NOTIFY_REPLY, pkt->imid, pkt->icid);
    return 0;
}

//(C) 2009 thomas
