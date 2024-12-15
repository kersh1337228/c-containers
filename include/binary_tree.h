#ifndef BINARY_TREE_H
#define BINARY_TREE_H

#include "containers.h"

struct binary_tree_node_t;
struct binary_tree_t;

typedef struct binary_tree_node_t binary_tree_node;
typedef struct binary_tree_t binary_tree;
typedef void (*visit_t)(const node_data *);

[[nodiscard]] binary_tree* binary_tree_init(comparator_t, size_t);
[[nodiscard]] size_t binary_tree_size(const binary_tree *);
void binary_tree_insert(binary_tree *, void *, unsigned char);
void binary_tree_remove(binary_tree *, const void *, unsigned char);
[[nodiscard]] node_data *binary_tree_at(const binary_tree *, const void *);
void binary_tree_visit_range(const binary_tree *, const void *, const void *, visit_t);
void binary_tree_delete(binary_tree *, unsigned char);

#endif // BINARY_TREE_H
