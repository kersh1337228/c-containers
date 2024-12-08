#ifndef HASH_MAP_H
#define HASH_MAP_H

#include "containers.h"

struct pair_t;
struct bucket_t;
struct hash_map_t;

typedef struct pair_t pair;
typedef struct bucket_t bucket;
typedef size_t (*hash_t)(size_t, size_t, const void *);
typedef struct hash_map_t hash_map;

[[nodiscard]] hash_map *hash_map_init(size_t, hash_t, comparator_t, size_t);
[[nodiscard]] size_t hash_map_size(const hash_map *);
void hash_map_insert(hash_map *, const void *, size_t, const void *, unsigned char);
void hash_map_remove(hash_map *, const void *, unsigned char);
[[nodiscard]] node_data *hash_map_at(const hash_map *, const void *);
void hash_map_delete(hash_map *, unsigned char);
void hash_map_print(const hash_map *, print_t, print_t);

// hash functions
size_t hash_any(size_t, size_t, const void *);

#endif // HASH_MAP_H
