#ifndef BIT_SET_H
#define BIT_SET_H

#include "containers.h"

typedef enum : unsigned char {
    ZERO = 0,
    ONE = 1
} bit_t;

struct bit_set;

typedef struct bit_set bit_set_t;

[[ nodiscard ]] bit_set_t *bit_set_init(size_t);
[[ nodiscard ]] size_t bit_set_size(const bit_set_t *);
void bit_set_insert(bit_set_t *, long long, bit_t);
void bit_set_push_back(bit_set_t *, bit_t);
void bit_set_push_front(bit_set_t *, bit_t);
void bit_set_remove(bit_set_t *, long long);
void bit_set_pop_back(bit_set_t *);
void bit_set_pop_front(bit_set_t *);
void bit_set_set(bit_set_t *, long long, bit_t);
[[ nodiscard ]] bit_t bit_set_get(const bit_set_t *, long long);
void bit_set_reverse(bit_set_t *);
void bit_set_delete(bit_set_t *);
void bit_set_print(const bit_set_t *);

#endif // BIT_SET_H
