#ifndef SLPOOL_H
#define SLPOOL_H

#include <stddef.h>

typedef struct sl_pool_s *sl_pool;
typedef size_t sl_id;

extern const sl_id nullid;

sl_pool sl_create(size_t);
sl_pool sl_destroy(sl_pool, void (*)(void*));
sl_pool sl_clear(sl_pool);
int sl_exp(sl_pool);
sl_id sl_first(sl_pool);
sl_id sl_last(sl_pool);
sl_id sl_prev(sl_pool, sl_id);
sl_id sl_next(sl_pool, sl_id);
void *sl_get(sl_pool, sl_id);
size_t sl_length(sl_pool);
int sl_is_empty(sl_pool);
sl_id sl_add(sl_pool, const void*, sl_id);
sl_id sl_push_back(sl_pool, const void*);
sl_id sl_push_front(sl_pool, const void*);
sl_id sl_remove(sl_pool, sl_id);
sl_pool sl_for_each(sl_pool, void (*)(void*, const void*), const void*);

#endif
