#ifndef LIST_H
#define LIST_H

#include "containers.h"

struct list_node_t;
struct list_t;

typedef struct list_node_t list_node;
typedef struct list_t list;

[[nodiscard]] list *list_init(void);
[[nodiscard]] list_node *list_head(const list *);
[[nodiscard]] list_node *list_tail(const list *);
[[nodiscard]] node_data *list_node_data(list_node *);
[[nodiscard]] size_t list_size(const list *);
void list_insert(list *, size_t, size_t, void *, unsigned char);
void list_push_back(list *, size_t, void *, unsigned char);
void list_push_front(list *, size_t, void *, unsigned char);
void list_remove(list *, size_t, unsigned char);
void list_pop_back(list *, unsigned char);
void list_pop_front(list *, unsigned char);
[[nodiscard]] node_data *list_at(const list *, long long);
void list_node_move_to_head(list *, list_node *);
void list_swap(list *, list_node *, list_node *);
void list_reverse(list *);
void list_delete(list *, unsigned char);
void list_print(const list *, print_t);

#endif // LIST_H
