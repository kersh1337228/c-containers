#ifndef FORWARD_LIST_H
#define FORWARD_LIST_H

#include "containers.h"

struct forward_list_node;
struct forward_list;

typedef struct forward_list_node forward_list_node_t;
typedef struct forward_list forward_list_t;

[[ nodiscard ]] forward_list_t *forward_list_init(void);
[[ nodiscard ]] forward_list_node_t *forward_list_head(const forward_list_t *);
[[ nodiscard ]] size_t forward_list_size(const forward_list_t *);
void forward_list_insert(forward_list_t *, size_t, size_t, void *, unsigned char);
void forward_list_push_back(forward_list_t *, size_t, void *, unsigned char);
void forward_list_push_front(forward_list_t *, size_t, void *, unsigned char);
void forward_list_remove(forward_list_t *, size_t, unsigned char);
void forward_list_pop_back(forward_list_t *, unsigned char);
void forward_list_pop_front(forward_list_t *, unsigned char);
[[ nodiscard ]] node_data_t *forward_list_at(const forward_list_t *, long long);
void forward_list_reverse(forward_list_t *);
void forward_list_delete(forward_list_t *, unsigned char);
void forward_list_print(const forward_list_t *, print_t);

#endif // FORWARD_LIST_H
