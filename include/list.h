#ifndef LIST_H
#define LIST_H

#include "containers.h"

struct list_node;
struct list;

typedef struct list_node list_node_t;
typedef struct list list_t;

[[ nodiscard ]] list_t *list_init(void);
[[ nodiscard ]] list_node_t *list_head(const list_t *);
[[ nodiscard ]] list_node_t *list_tail(const list_t *);
[[ nodiscard ]] node_data_t *list_node_data(list_node_t *);
[[ nodiscard ]] size_t list_size(const list_t *);
void list_insert(list_t *, size_t, size_t, void *, unsigned char);
void list_push_back(list_t *, size_t, void *, unsigned char);
void list_push_front(list_t *, size_t, void *, unsigned char);
void list_remove(list_t *, size_t, unsigned char);
void list_pop_back(list_t *, unsigned char);
void list_pop_front(list_t *, unsigned char);
[[ nodiscard ]] node_data_t *list_at(const list_t *, long long);
void list_node_move_to_head(list_t *, list_node_t *);
void list_swap(list_t *, list_node_t *, list_node_t *);
void list_reverse(list_t *);
void list_delete(list_t *, unsigned char);
void list_print(const list_t *, print_t);

#endif // LIST_H
