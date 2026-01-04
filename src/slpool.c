#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slpool.h"

#define POOL_INIT_CAP 16
#define HOLES_INIT_CAP 16

struct sl_pool_s {
    	char *buffer;
        sl_id first, last, hwm, *holes;
        size_t len, cap, ssize, esize, hlen, hcap;
        int exp;
};

const sl_id nullid = (size_t)-1;
static const size_t offset = sizeof(size_t) * 2;

sl_pool sl_create(size_t esize) {
        if(!esize)
                return NULL;
        sl_pool p = malloc(sizeof(struct sl_pool_s));
        if(!p)
                return NULL;
        p->ssize = esize + offset;
        p->ssize = (p->ssize + sizeof(sl_id) - 1) / sizeof(sl_id) * sizeof(sl_id);
        p->esize = esize;
        p->len = 0;
        p->cap = POOL_INIT_CAP;
        p->first = nullid;
        p->last = nullid;
        p->hlen = 0;
        p->hcap = HOLES_INIT_CAP;
        p->hwm = nullid;
        p->exp = 0;
        if(!(p->buffer = malloc(p->cap * p->ssize))) {
                free(p);
                return NULL;
        }
        if(!(p->holes = malloc(p->hcap * sizeof(sl_id)))) {
                free(p->buffer);
                free(p);
                return NULL;
        }
        return p;
}

sl_pool sl_destroy(sl_pool p, void (*destructor)(void*)) {
        if(!p)
                return NULL;
        if(destructor && p->hwm != nullid) {
                for(sl_id i = 0; i <= p->hwm; i += p->ssize)
                        destructor(p->buffer + i + offset);
        }
        free(p->buffer);
        free(p->holes);
        free(p);
        return NULL;
}

sl_pool sl_clear(sl_pool p) {
        if(!p)
                return NULL;
        p->first = (p->last = nullid);
        p->len = (p->hlen = 0);
        return p;
}

int sl_exp(sl_pool p) {
        return p ? p->exp : -1;
}

sl_id sl_first(sl_pool p) {
        return p ? p->first : nullid;
}

sl_id sl_last(sl_pool p) {
        return p ? p->last : nullid;
}

sl_id sl_prev(sl_pool p, sl_id i) {
        return p && i != nullid ? *(sl_id*)(p->buffer + i) : nullid;
}

sl_id sl_next(sl_pool p, sl_id i) {
        return p && i != nullid ? *(sl_id*)(p->buffer + i + sizeof(sl_id)) : nullid;
}

void *sl_get(sl_pool p, sl_id i) {
        return p && i != nullid ? p->buffer + i + offset : NULL;
}

size_t sl_length(sl_pool p) {
        return p ? p->len : 0;
}

int sl_is_empty(sl_pool p) {
        return p ? p->len == 0 : -1;
}

sl_id sl_add(sl_pool p, const void *e, sl_id id) {
        if(!p)
                return nullid;
        if(p->len == p->cap) {
                size_t cap = p->cap * 2;
                char *buffer = realloc(p->buffer, p->ssize * cap);
                if(!buffer)
                        return nullid;
                p->buffer = buffer;
                p->cap = cap;
        }
        sl_id place = p->hlen ? p->holes[--p->hlen] : p->len * p->ssize;
        sl_id prev = id == nullid ? p->last : sl_prev(p, id);
        *(sl_id*)(p->buffer + place) = prev;
        *(sl_id*)(p->buffer + place + sizeof(sl_id)) = id;
        if(e)
                memcpy(p->buffer + place + offset, e, p->esize);
        if(prev == nullid)
                p->first = place;
        else
                *(sl_id*)(p->buffer + prev + sizeof(sl_id)) = place;
        if(id == nullid)
                p->last = place;
        else
                *(sl_id*)(p->buffer + id) = place;
        p->len++;
        p->exp = p->hwm == nullid || place > p->hwm;
        p->hwm = p->hwm == nullid || place > p->hwm ? place : p->hwm;
        return place;
}

sl_id sl_push_back(sl_pool p, const void *e) {
        return p ? sl_add(p, e, nullid) : nullid;
}

sl_id sl_push_front(sl_pool p, const void *e) {
        return p ? sl_add(p, e, p->first) : nullid;
}

sl_id sl_remove(sl_pool p, sl_id id) {
        if(!p || id == nullid)
                return nullid;
        if(p->len == 1)
                p->hlen = 0;
        else if(p->hlen == p->hcap) {
                size_t hcap = p->hcap * 2;
                sl_id *holes = realloc(p->holes, sizeof(sl_id) * hcap);
                if(!holes)
                        return nullid;
                p->holes = holes;
                p->hcap = hcap;
        }
        sl_id prev = sl_prev(p, id), next = sl_next(p, id);
        if(prev == nullid)
                p->first = next;
        else
                *(sl_id*)(p->buffer + prev + sizeof(sl_id)) = next;
        if(next == nullid)
                p->last = prev;
        else
                *(sl_id*)(p->buffer + next) = prev;
        *(sl_id*)(p->buffer + id) = nullid;
        *(sl_id*)(p->buffer + id + sizeof(sl_id)) = nullid;
        p->holes[p->hlen++] = id;
        p->len--;
        return next;
}

sl_pool sl_for_each(sl_pool p, void (*f)(void*, const void*), const void *ctx) {
        if(!p || !f)
                return p;
        for(sl_id id = p->first; id != nullid; id = *(sl_id*)(p->buffer + id + sizeof(sl_id)))
                f(p->buffer + id + offset, ctx);
        return p;
}
