#ifndef BIT_SET_H
#define BIT_SET_H

#include "containers.h"

enum bit_t : unsigned char {
    ZERO = 0,
    ONE = 1
};

struct bit_set_t;

typedef enum bit_t bit;
typedef struct bit_set_t bit_set;

[[nodiscard]] bit_set *bit_set_init(size_t);
[[nodiscard]] size_t bit_set_size(const bit_set *);
void bit_set_insert(bit_set *, long long, bit);
void bit_set_push_back(bit_set *, bit);
void bit_set_push_front(bit_set *, bit);
void bit_set_remove(bit_set *, long long);
void bit_set_pop_back(bit_set *);
void bit_set_pop_front(bit_set *);
void bit_set_set(bit_set *, long long, bit);
[[nodiscard]] bit bit_set_get(const bit_set *, long long);
void bit_set_reverse(bit_set *);
void bit_set_delete(bit_set *);
void bit_set_print(const bit_set *);

#endif // BIT_SET_H
