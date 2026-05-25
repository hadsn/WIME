#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    struct BiLink {
        struct BiLink* prev;
        struct BiLink* next;
        void* obj;
    };
    typedef struct BiLink BiLink;

    void LkPushEnd(BiLink** bgn, void* obj);
    void* LkRemove(BiLink** bgn, BiLink* c);

#ifdef __cplusplus
}
#endif

//(C) 2008 thomas
