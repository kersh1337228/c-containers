#ifndef VECTOR_H
#define VECTOR_H

#include "containers.h"

struct vector;

typedef struct vector vector_t;

[[ nodiscard ]] vector_t *vector_init(size_t);
[[ nodiscard ]] size_t vector_size(const vector_t *);
void vector_insert(vector_t *, size_t, size_t, const void *);
void vector_push_back(vector_t *, size_t, const void *);
void vector_push_front(vector_t *, size_t, const void *);
void vector_remove(vector_t *, size_t);
void vector_pop_back(vector_t *);
void vector_pop_front(vector_t *);
void vector_set(vector_t *, size_t, size_t, const void *);
[[ nodiscard ]] node_data_t vector_get(const vector_t *, size_t);
void vector_reverse(vector_t *);
void vector_delete(vector_t *);
void vector_print(const vector_t *, print_t);

#endif // VECTOR_H
