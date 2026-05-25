
#include "wimexim.h"
#include "lib/log.h"
#include <stdlib.h>

extern Array ContextList;

int Connect(WxContext* cx, XimConnect* pkt)
{
    DEBUGLOG(CH_XIM, "order=0x%hhx version=%hd/%hd auth=%hd\n", pkt->order, pkt->client_major, pkt->client_minor, pkt->auth_nums);

    SendW(cx->Client, XIM_CONNECT_REPLY, 1, 0);
    return 0;
}

int Disconnect(WxContext* cx)
{
    DEBUGLOG(CH_XIM, "disconnect: client 0x%lx proxy 0x%lx\n", cx->Client, cx->Proxy);
    Send0(cx->Client, XIM_DISCONNECT_REPLY);
    DisconnectClient(cx, true);
    return 0;
}

int Disconnect_nwm(WxContext* cx)
{
    Send0(cx->Client, XIM_DISCONNECT_REPLY);
    return 0;
}

/*
  disconnectされた時、クライアントが閉じられた時
  クライアントが閉じられた時,中継ウィンドウも閉じられる(クライアントを親ウィンドウにしているため)。中継ウィンドウが閉じられたことによるDestroyNotifyでこの関数を呼ぶ時はdispをNULLにすること。→やめてフラグにした
*/
void DisconnectClient(WxContext* cx, bool send_reply)
{
    XimImIc pkt;

    pkt.imid = ArIndex(&ContextList, cx) + 1;
    for (int n = 0; n < ArUsing(&cx->Ic); ++n) {
        IcData* icp = ArElem(&cx->Ic, n);
        if ((icp->Flags & ICF_INVALID) == 0) {
            pkt.icid = n + 1;
            DestroyIcIf(cx, &pkt, send_reply, true);
        }
    }

    if (!(cx->Flags & IMF_INVALID))
        free(cx->Encoding);

    /*
      イベントの順番が
      1. clientとproxyが閉じられる
      2. xim_disconnectが来る
      3. proxyのDestroyNotifyが来る
      であった場合、xim_disconnectでproxyを閉じればbadwindowになる。
      2より先に3がくれば問題ないが、もちろん確実ではない。
      なので、proxyはclientに閉じてもらうことにし、こちらでは何もしないことにする。
      そもそもxim-disconnectを送らないのが悪い。
    */
#if 0
    if (disp != NULL) {
        XDestroyWindow(disp, cx->Proxy);
        LOG("destroy proxy window %p\n", cx->Proxy);
    }
#endif
    cx->Flags |= IMF_INVALID;
}

//(C) 2009 thomas
