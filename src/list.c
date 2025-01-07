#include "list.h"
#include <stdio.h>
#include <string.h>

struct list_node_t {
    struct list_node_t *prev;
    struct list_node_t *next;
    node_data data;
};

struct list_t {
    list_node *head;
    list_node *tail;
};

[[nodiscard]] list *list_init(void) {
    list *const l = malloc(sizeof(list));
    if (l == NULL) {
        fprintf(stderr, "malloc NULL return in list_init\n");
        return l;
    }
    l->head = NULL;
    l->tail = NULL;
    return l;
}

[[nodiscard]] list_node *list_head(const list *const this) {
    return this->head;
}

[[nodiscard]] list_node *list_tail(const list *const this) {
    return this->tail;
}

[[nodiscard]] node_data *list_node_data(list_node *const node) {
    return &node->data;
}

[[nodiscard]] size_t list_size(const list *const this) {
    size_t sz = 0;
    if (this != NULL) {
        const list_node *current_node = this->head;
        while (current_node != NULL) {
            ++sz;
            current_node = current_node->next;
        }
    }
    return sz;
}

static list_node *list_node_init(
    const size_t type_sz,
    void *restrict const data,
    list_node *restrict const prev,
    list_node *restrict const next,
    const unsigned char intrusive
) {
    list_node *new_node = malloc(sizeof(list_node));
    if (new_node == NULL) {
        fprintf(stderr, "malloc NULL return in list_node_init\n");
        return NULL;
    }
    void *new_data;
    if (intrusive) {
        new_data = data;
    } else {
        new_data = malloc(type_sz);
        if (new_data == NULL) {
            fprintf(stderr, "malloc NULL return in list_node_init for type_sz %lu\n", type_sz);
            return NULL;
        }
        memcpy(new_data, data, type_sz);
    }
    new_node->data.data = new_data;
    new_node->data.type_sz = type_sz;
    new_node->prev = prev;
    new_node->next = next;
    if (prev != NULL) {
        prev->next = new_node;
    }
    if (next != NULL) {
        next->prev = new_node;
    }
    return new_node;
}

void list_insert(
    list *restrict const this,
    size_t index,
    const size_t type_sz,
    void *restrict const data,
    const unsigned char intrusive
) {
    if (this == NULL) {
        return;
    }
    const size_t size = list_size(this);
    if (index > size) {
        return;
    }
    if (this->head == NULL) {
        list_node *new_node = list_node_init(type_sz, data, NULL, NULL, intrusive);
        this->head = new_node;
        this->tail = new_node;
        return;
    }
    if (index == 0) {
        list_node *new_node = list_node_init(type_sz, data, NULL, this->head, intrusive);
        this->head = new_node;
        return;
    }
    if (index == size) {
        list_node *new_node = list_node_init(type_sz, data, this->tail, NULL, intrusive);
        this->tail = new_node;
        return;
    }
    list_node *prev_node;
    list_node *next_node;
    if (index <= size >> 1) {
        prev_node = this->head;
        while (--index > 0) {
            prev_node = prev_node->next;
        }
        next_node = prev_node->next;
    } else {
        next_node = this->tail;
        index = size - index;
        while (--index > 0) {
            next_node = next_node->prev;
        }
        prev_node = next_node->prev;
    }
    list_node_init(type_sz, data, prev_node, next_node, intrusive);
}

void list_push_back(
    list *restrict const this,
    const size_t type_sz,
    void *restrict const data,
    const unsigned char intrusive
) {
    if (this == NULL) {
        return;
    }
    if (this->head == NULL) {
        list_node *new_node = list_node_init(type_sz, data, NULL, NULL, intrusive);
        this->head = new_node;
        this->tail = new_node;
        return;
    }
    list_node *new_node = list_node_init(type_sz, data, this->tail, NULL, intrusive);
    this->tail = new_node;
}

void list_push_front(
    list *restrict const this,
    const size_t type_sz,
    void *restrict const data,
    const unsigned char intrusive
) {
    if (this == NULL) {
        return;
    }
    if (this->head == NULL) {
        list_node *new_node = list_node_init(type_sz, data, NULL, NULL, intrusive);
        this->head = new_node;
        this->tail = new_node;
        return;
    }
    list_node *new_node = list_node_init(type_sz, data, NULL, this->head, intrusive);
    this->head = new_node;
}

void list_remove(
    list *const this,
    size_t index,
    const unsigned char intrusive
) {
    if (this == NULL || this->head == NULL) {
        return;
    }
    const size_t size = list_size(this);
    if (index > size - 1) {
        return;
    }
    if (this->head == this->tail) {
        if (!intrusive) {
            free(this->head->data.data);
        }
        free(this->head);
        this->head = NULL;
        this->tail = NULL;
        return;
    }
    if (index == 0) {
        list_node *next_node = this->head->next;
        next_node->prev = NULL;
        if (!intrusive) {
            free(this->head->data.data);
        }
        free(this->head);
        this->head = next_node;
        return;
    }
    if (index == size - 1) {
        list_node *prev_node = this->tail->prev;
        prev_node->next = NULL;
        if (!intrusive) {
            free(this->tail->data.data);
        }
        free(this->tail);
        this->tail = prev_node;
        return;
    }
    list_node *prev_node;
    list_node *current_node;
    list_node *next_node;
    if (index < size >> 1) {
        prev_node = this->head;
        while (--index > 0) {
            prev_node = prev_node->next;
        }
        current_node = prev_node->next;
        next_node = current_node->next;

    } else {
        next_node = this->tail;
        index = size - 1 - index;
        while (--index > 0) {
            next_node = next_node->prev;
        }
        current_node = next_node->prev;
        prev_node = current_node->prev;
    }
    if (!intrusive) {
        free(current_node->data.data);
    }
    free(current_node);
    prev_node->next = next_node;
    next_node->prev = prev_node;
}

void list_pop_back(
    list *const this,
    const unsigned char intrusive
) {
    if (this == NULL || this->head == NULL) {
        return;
    }
    if (this->head == this->tail) {
        if (!intrusive) {
            free(this->head->data.data);
        }
        free(this->head);
        this->head = NULL;
        this->tail = NULL;
        return;
    }
    list_node *prev_node = this->tail->prev;
    prev_node->next = NULL;
    if (!intrusive) {
        free(this->tail->data.data);
    }
    free(this->tail);
    this->tail = prev_node;
}

void list_pop_front(
    list *const this,
    const unsigned char intrusive
) {
    if (this == NULL || this->head == NULL) {
        return;
    }
    if (this->head == this->tail) {
        if (!intrusive) {
            free(this->head->data.data);
        }
        free(this->head);
        this->head = NULL;
        this->tail = NULL;
        return;
    }
    list_node *next_node = this->head->next;
    next_node->prev = NULL;
    if (!intrusive) {
        free(this->head->data.data);
    }
    free(this->head);
    this->head = next_node;
}

[[nodiscard]] static list_node *list_node_at(
    const list *const this,
    long long index
) {
    const size_t size = list_size(this);
    if (this->head == NULL || index > (long long)size - 1) {
        return NULL;
    }
    if (index < 0ll) {
        if (index < -(long long)size) {
            return NULL;
        }
        index += (long long)size;
    }
    if (index == 0ll) {
        return this->head;
    }
    if (index == size - 1ll) {
        return this->tail;
    }
    if (index < size >> 1) {
        const list_node *prev_node = this->head;
        while (--index > 0) {
            prev_node = prev_node->next;
        }
        return prev_node->next;
    }
    const list_node *next_node = this->tail;
    index = (long long)size - 1ll - index;
    while (--index > 0) {
        next_node = next_node->prev;
    }
    return next_node->prev;
}

[[nodiscard]] node_data *list_at(
    const list *const this,
    const long long index
) {
    if (this == NULL || this->head == NULL) {
        return NULL;
    }
    list_node *const node_at = list_node_at(this, index);
    if (node_at == NULL) {
        return NULL;
    }
    return &node_at->data;
}

void list_node_move_to_head(list *const this, list_node *const node) {
    if (this == NULL || node == NULL || this->head == node) {
        return;
    }
    list_node *prev = node->prev;
    list_node *next = node->next;
    if (prev != NULL) {
        prev->next = next;
    }
    if (next != NULL) {
        next->prev = prev;
    }
    this->head->prev = node;
    node->next = this->head;
    node->prev = NULL;
    this->head = node;
}

void list_node_move_to_tail(list *const this, list_node *const node) {
    if (this == NULL || node == NULL || this->tail == node) {
        return;
    }
    list_node *prev = node->prev;
    list_node *next = node->next;
    if (prev != NULL) {
        prev->next = next;
    }
    if (next != NULL) {
        next->prev = prev;
    }
    this->tail->next = node;
    node->prev = this->tail;
    node->next = NULL;
    this->tail = node;
}

void list_swap(list *const this, list_node *const i, list_node *const j) {
    if (this == NULL || i == NULL || j == NULL || i == j) {
        return;
    }
    list_node *i_prev = i->prev;
    if (i_prev != NULL) {
        i_prev->next = j;
    }
    i->prev = j->prev;
    if (j->prev != NULL) {
        j->prev->next = i;
    }
    j->prev = i_prev;
    list_node *i_next = i->next;
    if (i_next != NULL) {
        i_next->prev = j;
    }
    i->next = j->next;
    if (j->next != NULL) {
        j->next->prev = i;
    }
    j->next = i_next;
    if (i == this->head) {
        this->head = j;
    } else if (j == this->head) {
        this->head = i;
    }
    if (i == this->tail) {
        this->tail = j;
    } else if (j == this->tail) {
        this->tail = i;
    }
}

void list_reverse(list *const this) {
    if (this == NULL || this->head == NULL || this->head->next == NULL) {
        return;
    }
    this->tail = this->head;
    list_node *new_head = this->head;
    list_node *rem_head = new_head->next;
    new_head->next = NULL;
    rem_head->prev = NULL;
    list_node *new_head_prev = this->head;
    for (;;) {
        new_head = rem_head;
        rem_head = new_head->next;
        new_head->next = new_head_prev;
        new_head_prev->prev = new_head;
        new_head_prev = new_head;
        if (rem_head == NULL) {
            break;
        }
        rem_head->prev = NULL;
    }
    this->head = new_head;
}

void list_delete(
    list *const this,
    const unsigned char intrusive
) {
    if (this == NULL) {
        return;
    }
    list_node *current_node = this->head;
    while (current_node != NULL) {
        list_node *prev_node = current_node;
        current_node = current_node->next;
        if (!intrusive) {
            free(prev_node->data.data);
        }
        free(prev_node);
    }
    free(this);
}

void list_print(
    const list *restrict const this,
    const print_t print_data
) {
    printf("[");
    if (this != NULL && this->head != NULL) {
        const list_node *current_node = this->head;
        while (current_node->next != NULL) {
            print_data(current_node->data.data);
            printf(", ");
            current_node = current_node->next;
        }
        print_data(current_node->data.data);
    }
    printf("]\n");
}
