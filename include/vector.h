#ifndef VECTOR_H
#define VECTOR_H

#include "containers.h"

struct vector_t;

typedef struct vector_t vector;

[[nodiscard]] vector *vector_init(size_t);
[[nodiscard]] size_t vector_size(const vector *);
void vector_insert(vector *, size_t, size_t, const void *);
void vector_push_back(vector *, size_t, const void *);
void vector_push_front(vector *, size_t, const void *);
void vector_remove(vector *, size_t);
void vector_pop_back(vector *);
void vector_pop_front(vector *);
void vector_set(vector *, size_t, size_t, const void *);
[[nodiscard]] node_data vector_get(const vector *, size_t);
void vector_reverse(vector *);
void vector_delete(vector *);
void vector_print(const vector *, print_t);

#endif // VECTOR_H
