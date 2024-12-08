#ifndef FORWARD_LIST_H
#define FORWARD_LIST_H

#include "containers.h"

struct forward_list_node_t;
struct forward_list_t;

typedef struct forward_list_node_t forward_list_node;
typedef struct forward_list_t forward_list;

[[nodiscard]] forward_list *forward_list_init(void);
[[nodiscard]] forward_list_node *forward_list_head(const forward_list *);
[[nodiscard]] size_t forward_list_size(const forward_list *);
void forward_list_insert(forward_list *, size_t, size_t, void *, unsigned char);
void forward_list_push_back(forward_list *, size_t, void *, unsigned char);
void forward_list_push_front(forward_list *, size_t, void *, unsigned char);
void forward_list_remove(forward_list *, size_t, unsigned char);
void forward_list_pop_back(forward_list *, unsigned char);
void forward_list_pop_front(forward_list *, unsigned char);
[[nodiscard]] node_data *forward_list_at(const forward_list *, long long);
void forward_list_reverse(forward_list *);
void forward_list_delete(forward_list *, unsigned char);
void forward_list_print(const forward_list *, print_t);

#endif // FORWARD_LIST_H
