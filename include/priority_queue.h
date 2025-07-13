#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

#include "containers.h"

struct priority_queue;

typedef struct priority_queue priority_queue_t;

[[ nodiscard ]] priority_queue_t *priority_queue_init(comparator_t comparator);
void priority_queue_push(priority_queue_t *this, size_t type_size, const void *data);
[[ nodiscard ]] node_data_t priority_queue_top(const priority_queue_t *this);
void priority_queue_pop(priority_queue_t *this);
void priority_queue_delete(priority_queue_t *this);

#endif // PRIORITY_QUEUE_H
