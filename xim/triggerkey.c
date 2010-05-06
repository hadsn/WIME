#include <X11/keysym.h>
#include "x.h"
#include "wimexim.h"
#include <string.h>
#include <stdlib.h>

typedef struct{
    uint32_t keysym;
    uint32_t modifier;
    uint32_t modifier_mask;
}__attribute__((packed)) XimTriggerKey;

typedef struct{
    XimHeader	h;
    uint16_t	imid;
    uint16_t	unused;
    uint32_t	sz_onkeys;
    XimTriggerKey on_key;
    uint32_t	sz_offkeys;
    XimTriggerKey off_key;
}__attribute__((packed)) XimRegisterTriggerKey;

extern Array ContextList;

int RegTriggerKeys(WxContext* cx)
{
    XimRegisterTriggerKey r={
	{0,0,0},	//h;
	ArIndex(&ContextList,cx)+1,	//imid;
	0,
	sizeof(r.on_key),		//sz_onkeys
	{XK_q,0,0},//{XK_grave,Mod1Mask,Mod1Mask},	//on
	sizeof(r.off_key),		//sz_offkeys
	{XK_x,0,0}//{XK_grave,Mod1Mask,ControlMask|Mod1Mask}	//off
    };
    send_n(cx->Client,XIM_REGISTER_TRIGGERKEYS,&r,sizeof(r));
    return 0;
}

//onキーがこないのはなぜ？
int TriggerNotify(WxContext* cx,XimTriggerNotify* pkt)
{
    LOG("im-id=%hd ic-id=%hd flag=%x key=%d ev-mask=%x\n",pkt->imid,pkt->icid,pkt->flag,pkt->keys_list,pkt->event_mask);
    if((cx->Flags ^= ICF_IME_ENABLE) & ICF_IME_ENABLE){
	//ツールバーを表示する
	LOG("	kanji on\n");
    }else{
	//ツールバーを消す
	LOG("	kanji off\n");
    }
    send_ww(cx->Client,XIM_TRIGGER_NOTIFY_REPLY,pkt->imid,pkt->icid);
    return 0;
}
