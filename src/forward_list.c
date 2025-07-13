#include "forward_list.h"
#include <stdio.h>
#include <string.h>

struct forward_list_node {
    struct forward_list_node *next;
    node_data_t data;
};

struct forward_list {
    forward_list_node_t *head;
};

[[nodiscard]] forward_list_t *forward_list_init(void) {
    forward_list_t *const fl = malloc(sizeof(forward_list_t));
    if (fl == NULL) {
        fprintf(stderr, "malloc NULL return in forward_list_init\n");
        return fl;
    }
    fl->head = NULL;
    return fl;
}

[[nodiscard]] forward_list_node_t *forward_list_head(const forward_list_t *const this) {
    return this->head;
}

[[nodiscard]] size_t forward_list_size(const forward_list_t *const this) {
    size_t sz = 0;
    const forward_list_node_t *current_node = this->head;
    while (current_node != NULL) {
        ++sz;
        current_node = current_node->next;
    }
    return sz;
}

static forward_list_node_t *forward_list_node_init(
    const size_t type_sz,
    void *restrict const data,
    forward_list_node_t *restrict const next,
    const unsigned char intrusive
) {
    forward_list_node_t *new_node = malloc(sizeof(forward_list_node_t));
    if (new_node == NULL) {
        fprintf(stderr, "malloc NULL return in forward_list_node_init\n");
        return NULL;
    }
    void *new_data;
    if (intrusive) {
        new_data = data;
    } else {
        new_data = malloc(type_sz);
        if (new_data == NULL) {
            fprintf(stderr, "malloc NULL return in forward_list_node_init for type_sz %lu\n", type_sz);
            return NULL;
        }
        memcpy(new_data, data, type_sz);
    }
    new_node->data.data = new_data;
    new_node->data.type_sz = type_sz;
    new_node->next = next;
    return new_node;
}

void forward_list_insert(
    forward_list_t *restrict const this,
    size_t index,
    const size_t type_sz,
    void *restrict const data,
    const unsigned char intrusive
) {
    const size_t size = forward_list_size(this);
    if (index > size) {
        return;
    }
    if (this->head == NULL) {
        forward_list_node_t *new_node = forward_list_node_init(type_sz, data, NULL, intrusive);
        this->head = new_node;
        return;
    }
    if (index == 0) {
        forward_list_node_t *new_node = forward_list_node_init(type_sz, data, this->head, intrusive);
        this->head = new_node;
        return;
    }
    forward_list_node_t *prev_node = this->head;
    while (--index > 0) {
        prev_node = prev_node->next;
    }
    forward_list_node_t *next_node = prev_node->next;
    forward_list_node_t *new_node = forward_list_node_init(type_sz, data, next_node, intrusive);
    prev_node->next = new_node;
}

void forward_list_push_back(
    forward_list_t *restrict const this,
    const size_t type_sz,
    void *restrict const data,
    const unsigned char intrusive
) {
    if (this->head == NULL) {
        forward_list_node_t *new_node = forward_list_node_init(type_sz, data, NULL, intrusive);
        this->head = new_node;
        return;
    }
    forward_list_node_t *prev_node = this->head;
    while (prev_node->next != NULL) {
        prev_node = prev_node->next;
    }
    forward_list_node_t *new_node = forward_list_node_init(type_sz, data, NULL, intrusive);
    prev_node->next = new_node;
}

void forward_list_push_front(
    forward_list_t *restrict const this,
    const size_t type_sz,
    void *restrict const data,
    const unsigned char intrusive
) {
    if (this->head == NULL) {
        forward_list_node_t *new_node = forward_list_node_init(type_sz, data, NULL, intrusive);
        this->head = new_node;
        return;
    }
    forward_list_node_t *new_node = forward_list_node_init(type_sz, data, this->head, intrusive);
    this->head = new_node;
}

void forward_list_remove(
    forward_list_t *const this,
    size_t index,
    const unsigned char intrusive
) {
    const size_t size = forward_list_size(this);
    if (this->head == NULL || index > size - 1) {
        return;
    }
    if (index == 0) {
        forward_list_node_t *next_node = this->head->next;
        if (!intrusive) {
            free(this->head->data.data);
        }
        free(this->head);
        this->head = next_node;
        return;
    }
    forward_list_node_t *prev_node = this->head;
    while (--index > 0) {
        prev_node = prev_node->next;
    }
    forward_list_node_t *current_node = prev_node->next;
    forward_list_node_t *next_node = current_node->next;
    if (!intrusive) {
        free(current_node->data.data);
    }
    free(current_node);
    prev_node->next = next_node;
}

void forward_list_pop_back(
    forward_list_t *const this,
    const unsigned char intrusive
) {
    if (this->head == NULL) {
        return;
    }
    if (this->head->next == NULL) {
        if (!intrusive) {
            free(this->head->data.data);
        }
        free(this->head);
        this->head = NULL;
        return;
    }
    forward_list_node_t *prev_node = this->head;
    while (prev_node->next->next != NULL) {
        prev_node = prev_node->next;
    }
    forward_list_node_t *current_node = prev_node->next;
    if (!intrusive) {
        free(current_node->data.data);
    }
    free(current_node);
    prev_node->next = NULL;
}

void forward_list_pop_front(
    forward_list_t *const this,
    const unsigned char intrusive
) {
    if (this->head == NULL) {
        return;
    }
    forward_list_node_t *next_node = this->head->next;
    if (!intrusive) {
        free(this->head->data.data);
    }
    free(this->head);
    this->head = next_node;
}

[[nodiscard]] static forward_list_node_t *forward_list_node_at(
    const forward_list_t *const this,
    long long index
) {
    const size_t size = forward_list_size(this);
    if (index < 0) {
        index %= (long long)size;
    }
    if (this->head == NULL || index > size - 1) {
        return NULL;
    }
    if (index == 0) {
        return this->head;
    }
    forward_list_node_t *current_node = this->head->next;
    while (--index > 0) {
        current_node = current_node->next;
    }
    return current_node;
}

[[nodiscard]] node_data_t *forward_list_at(
    const forward_list_t *const this,
    const long long index
) {
    forward_list_node_t *const node_at = forward_list_node_at(this, index);
    if (node_at == NULL) {
        return NULL;
    }
    return &node_at->data;
}

void forward_list_reverse(forward_list_t *const this) {
    if (this->head == NULL || this->head->next == NULL) {
        return;
    }
    forward_list_node_t *new_head = this->head;
    forward_list_node_t *rem_head = new_head->next;
    new_head->next = NULL;
    forward_list_node_t *new_head_prev = this->head;
    while (rem_head != NULL) {
        new_head = rem_head;
        rem_head = new_head->next;
        new_head->next = new_head_prev;
        new_head_prev = new_head;
    }
    this->head = new_head;
}

void forward_list_delete(
    forward_list_t *const this,
    const unsigned char intrusive
) {
    forward_list_node_t *current_node = this->head;
    while (current_node != NULL) {
        forward_list_node_t *prev_node = current_node;
        current_node = current_node->next;
        if (!intrusive) {
            free(prev_node->data.data);
        }
        free(prev_node);
    }
    free(this);
}

void forward_list_print(
    const forward_list_t *restrict const this,
    const print_t print_data
) {
    printf("[");
    if (this->head != NULL) {
        const forward_list_node_t *current_node = this->head;
        while (current_node->next != NULL) {
            print_data(current_node->data.data);
            printf(", ");
            current_node = current_node->next;
        }
        print_data(current_node->data.data);
    }
    printf("]\n");
}
