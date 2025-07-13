#ifndef CONTAINERS_H
#define CONTAINERS_H

#include <stdlib.h>

struct node_data {
    size_t type_sz;
    void *data;
};

typedef struct node_data node_data_t;
typedef void (*print_t)(const void *);
typedef signed char (*comparator_t)(const void *, const void *);

#endif // CONTAINERS_H
