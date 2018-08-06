#include <stdlib.h>
#include "link.h"

void LkPushEnd(BiLink** bgn,void* obj)
{
    BiLink *pv;

    for(pv=NULL; *bgn!=NULL; pv=*bgn,bgn=&((*bgn)->next))
	;
    *bgn = malloc(sizeof(BiLink));
    (*bgn)->prev = pv;
    (*bgn)->next = NULL;
    (*bgn)->obj = obj;
}

void* LkRemove(BiLink** bgn,BiLink* c)
{
    *(c->prev!=NULL ? &c->prev->next : bgn) = c->next;
    if(c->next!=NULL)
	c->next->prev = c->prev;
    void *o = c->obj;
    free(c);
    return o;
}

//(C) 2008 thomas
