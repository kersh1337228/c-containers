#ifndef HASH_MAP_H
#define HASH_MAP_H

#include "containers.h"

struct pair;
struct bucket;
struct hash_map;

typedef struct pair pair_t;
typedef struct bucket bucket_t;
typedef size_t (*hash_t)(size_t, size_t, const void *);
typedef struct hash_map hash_map_t;

[[ nodiscard ]] hash_map_t *hash_map_init(size_t, hash_t, comparator_t);
[[ nodiscard ]] size_t hash_map_size(const hash_map_t *);
void hash_map_insert(hash_map_t *, size_t, const void *, size_t, const void *, unsigned char);
void hash_map_remove(hash_map_t *, size_t, const void *, unsigned char);
[[ nodiscard ]] node_data_t *hash_map_at(const hash_map_t *, size_t, const void *);
void hash_map_delete(hash_map_t *, unsigned char);
void hash_map_print(const hash_map_t *, print_t, print_t);

// hash functions
[[ nodiscard ]] size_t hash_any(size_t, size_t, const void *);
[[ nodiscard ]] size_t hash_ul(size_t, size_t, const void *);
[[ nodiscard ]] size_t hash_str(size_t, size_t, const void *);

#endif // HASH_MAP_H
