#ifndef BINARY_TREE_H
#define BINARY_TREE_H

#include "containers.h"

struct binary_tree_node;
struct binary_tree;

typedef struct binary_tree_node binary_tree_node_t;
typedef struct binary_tree binary_tree_t;
typedef void (*visit_t)(const node_data_t *);

[[ nodiscard ]] binary_tree_t* binary_tree_init(comparator_t);
[[ nodiscard ]] size_t binary_tree_size(const binary_tree_t *);
void binary_tree_insert(binary_tree_t *, size_t, void *, unsigned char);
void binary_tree_remove(binary_tree_t *, const void *, unsigned char);
[[ nodiscard ]] node_data_t *binary_tree_at(const binary_tree_t *, const void *);
void binary_tree_visit_range(const binary_tree_t *, const void *, const void *, visit_t);
void binary_tree_delete(binary_tree_t *, unsigned char);

#endif // BINARY_TREE_H
