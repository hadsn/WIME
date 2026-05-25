
#include <string.h>
#include <stdlib.h>
#include "wimexim.h"
#include "lib/log.h"

typedef struct {
    XimHeader	h;
    uint16_t	imid;
    uint16_t	type;
    int16_t	index;
    uint16_t	unused;
}__attribute__((packed)) XimEncodingNegoReply;

void print_req_enc(XimEncodingNego* pkt, Array* buf);
void print_sel_enc(XimEncodingNego* pkt, int index);
void print_enc_info(XimEncodingNego* pkt, Array* buf);

/*
  euc-jpを選んでもcommitではctextで送らないといけないみたい。何のためにあるんだ？
  とりあえずctextを選ぶようにし、もしなければ１番目のものを選ぶことにする。
*/
int EncodingNego(WxContext* cx, XimEncodingNego* pkt)
{
    const char CTXT[] = "COMPOUND_TEXT";
    XimEncodingNegoReply r = { {0,0,0},pkt->imid,0,-1,0 };
    Array logbuf;

    ArNew(&logbuf, 1, NULL);
    DEBUGLOG(CH_XIM, "im-id=%hd\n", pkt->imid);

    //エンコード文字列のリスト
    int id = 0;
    Str* s = pkt->enc, * e = (Str*)((char*)s + pkt->names_len);
    while (s < e) {
        if (strncasecmp(s->str, CTXT, sizeof(CTXT) - 1) == 0)
            r.index = id;
        s = IncStr(s);
        ++id;
    }
    DEBUGDO(CH_XIM, print_req_enc(pkt, &logbuf));

    if (r.index == -1) {
        r.index = 0;	//ctextがなければ先頭のものを選ぶ。
        cx->Encoding = memcpy(malloc(pkt->enc->len + 1), pkt->enc->str, pkt->enc->len);
        cx->Encoding[pkt->enc->len] = 0;
    }
    DEBUGDO(CH_XIM, print_sel_enc(pkt, r.index));
    DEBUGDO(CH_XIM, print_enc_info(pkt, &logbuf));    //詳細情報らしい

    SendN(cx->Client, XIM_ENCODING_NEGOTIATION_REPLY, &r, sizeof(r));
    ArDelete(&logbuf);
    return 0;
}

void print_req_enc(XimEncodingNego* pkt, Array* buf)
{
    Str* s = pkt->enc, * e = (typeof(e))((char*)s + pkt->names_len);
    while (s < e) {
        char n[s->len + 1];
        memcpy(n, s->str, s->len);
        n[s->len] = 0;
        ArPrint(buf, "[%s]", n);
        s = IncStr(s);
    }
    DEBUGLOG(CH_XIM, "name=%s\n", (char*)ArAdr(buf));
}

void print_sel_enc(XimEncodingNego* pkt, int index)
{
    Str* s = pkt->enc;
    for (int n = 0; n < index; ++n)
        s = IncStr(s);
    char n[s->len + 1];
    memcpy(n, s->str, s->len);
    n[s->len] = 0;
    DEBUGLOG(CH_XIM, "selected encoding='%s'\n", n);
}

void print_enc_info(XimEncodingNego* pkt, Array* buf)
{
    XimEncNegoPart2* p2 = (typeof(p2))((char*)pkt + sizeof(*pkt) + pkt->names_len + Pad(pkt->names_len));
    EncodingInfo* eip = p2->enc, * eie = (typeof(eie))((char*)eip + p2->info_len);
    while (eip < eie) {
        ArPrint(buf, "[%s]", eip->info);
        eip = (typeof(eip))((char*)eip + eip->len + Pad(2 + eip->len));
    }
    DEBUGLOG(CH_XIM, "info=%s\n", (char*)ArAdr(buf));
}

//(C) 2009 thomas
