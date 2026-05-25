#include "wimexim.h"
#include "lib/log.h"

int ExtForwardKeyEvent(WxContext* cx, XimExtForwardKeyEvent* pkt)
{
    /*imExten.c‚ðŒ©‚é‚Æwindowƒƒ“ƒo‚Í‘¶Ý‚µ‚È‚¢‚æ‚¤‚¾‚ªH
     */
    DEBUGLOG(CH_XIM, "im %hd ic %hd flag 0x%hx num %hu\n", pkt->imid, pkt->icid, pkt->flag, pkt->sn);
    DEBUGLOG(CH_XIM, "type 0x%hhx code 0x%hhx state 0x%hx time %d\n", pkt->type, pkt->keycode, pkt->state, pkt->time);

    return ForwardKey(cx, (XimImIc*)pkt, pkt->keycode, pkt->state);
}

int ExtForwardKeyEvent_nwm(WxContext* cx, XimExtForwardKeyEvent* pkt)
{
    return ForwardEvent_nwm(cx, (XimForwardEvent*)pkt);
}
