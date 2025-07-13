#include "priority_queue.h"
#include <stdio.h>
#include <string.h>

struct priority_queue {
    void *buffer;
    size_t size;
    size_t allocated;
    size_t occupied;
    comparator_t comparator;
};

priority_queue_t *priority_queue_init(const comparator_t comparator) {
    if (comparator == NULL) {
        fprintf(stderr, "priority_queue_init: no comparator\n");
        return NULL;
    }

    priority_queue_t *this = malloc(sizeof(priority_queue_t));
    if (this == NULL) {
        fprintf(stderr, "priority_queue_init: malloc NULL return for %lu\n", sizeof(priority_queue_t));
        return NULL;
    }

    this->buffer = NULL;
    this->size = 0;
    this->allocated = 0;
    this->occupied = 0;
    this->comparator = comparator;

    return this;
}

static void *priority_queue_buffer_node_at(const priority_queue_t *this, size_t index) {
    if (this == NULL || index > this->size) {
        return NULL;
    }

    void *node = this->buffer;
    for (; index > 0; --index) {
        const size_t size = *(size_t *)node;
        node = (unsigned char *)node + sizeof(size) + size;
    }
    return node;
}

static void *priority_queue_buffer_data_at(const priority_queue_t *this, const size_t index) {
    if (this == NULL || index >= this->size) {
        return NULL;
    }

    void *node = priority_queue_buffer_node_at(this, index);
    return (unsigned char *)node + sizeof(size_t);
}

static void priority_queue_buffer_insert(priority_queue_t *this, const size_t index, const size_t type_size, const void *data) {
    if (this == NULL || index > this->size || type_size == 0 || data == NULL) {
        return;
    }

    void *insert_address = priority_queue_buffer_node_at(this, index);
    const size_t insert_size = sizeof(type_size) + type_size;

    size_t after_bytes = 0;
    void *node = insert_address;
    for (size_t i = index; i < this->size; ++i) {
        const size_t size = *(size_t *)node;
        const size_t node_size = sizeof(size) + size;
        node = (unsigned char *)node + node_size;
        after_bytes += node_size;
    }

    memcpy((unsigned char *)insert_address + insert_size, insert_address, after_bytes);
    *(size_t *)insert_address = type_size;
    memcpy((unsigned char *)insert_address + sizeof(type_size), data, type_size);

    ++this->size;
    this->occupied += insert_size;
}

void priority_queue_push(priority_queue_t *this, const size_t type_size, const void *data) {
    if (this == NULL || type_size == 0 || data == NULL) {
        return;
    }

    const size_t required_capacity = this->occupied + sizeof(type_size) + type_size;
    if (this->allocated < required_capacity) {
        const size_t new_allocated = (required_capacity + 1) << 1;
        void *new_buffer = realloc(this->buffer, new_allocated);
        if (new_buffer == NULL) {
            fprintf(stderr, "priority_queue_push: realloc NULL return for %lu\n", new_allocated);
            return;
        }
        this->allocated = new_allocated;
        this->buffer = new_buffer;
    }

    size_t l = 0;
    size_t r = this->size;
    while (l < r) {
        const size_t m = l + ((r - l) >> 1);
        const signed char comparison_result = this->comparator(data, priority_queue_buffer_data_at(this, m));
        if (comparison_result == 0) {
            break;
        }
        if (comparison_result < 0) {
            r = m;
        } else /* comparison_result > 0 */ {
            l = m + 1;
        }
    }

    size_t m;
    for (m = l + ((r - l) >> 1); m > 0; --m) {
        const signed char comparison_result = this->comparator(data, priority_queue_buffer_data_at(this, m));
        if (comparison_result < 0) {
            break;
        }
    }

    priority_queue_buffer_insert(this, m, type_size, data);
}

node_data_t priority_queue_top(const priority_queue_t *this) {
    node_data_t data_node = {
        .data = NULL,
        .type_sz = 0,
    };

    if (this == NULL) {
        return data_node;
    }

    void *node = priority_queue_buffer_node_at(this, this->size - 1);
    data_node.type_sz = *(size_t *)node;
    data_node.data = (unsigned char *)node + sizeof(data_node.type_sz);

    return data_node;
}

void priority_queue_pop(priority_queue_t *this) {
    if (this == NULL) {
        return;
    }

    void *node = priority_queue_buffer_node_at(this, this->size - 1);

    --this->size;
    this->occupied -= sizeof(size_t) + *(size_t *)node;

    if (this->occupied < this->allocated >> 2) {
        const size_t new_allocated = this->allocated >> 1;
        void *new_buffer = realloc(this->buffer, new_allocated);
        if (new_buffer == NULL) {
            fprintf(stderr, "priority_queue_pop: realloc NULL return for %lu\n", new_allocated);
            return;
        }
        this->allocated = new_allocated;
        this->buffer = new_buffer;
    }
}

void priority_queue_delete(priority_queue_t *this) {
    if (this == NULL) {
        return;
    }

    free(this->buffer);
    free(this);
}
