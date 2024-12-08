#include "vector.h"
#include <stdio.h>
#include <string.h>

struct vector_t {
    void *buffer;
    size_t capacity;
    size_t size;
};

[[nodiscard]] vector *vector_init(const size_t capacity) {
    vector *v = malloc(sizeof(vector));
    if (v == NULL) {
        fprintf(stderr, "malloc NULL return in vector_init");
        return v;
    }
    void *buffer = NULL;
    if (capacity) {
        buffer = malloc(capacity);
        if (buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_init for capacity %lu\n", capacity);
            v->buffer = NULL;
            v->capacity = 0ul;
            v->size = 0ul;
            return v;
        }
    }
    v->buffer = buffer;
    v->capacity = capacity;
    v->size = 0ul;
    return v;
}

[[nodiscard]] size_t vector_size(const vector *const this) {
    if (this == NULL) {
        return 0ul;
    }
    return this->size;
}

[[nodiscard]] static size_t vector_offset_at(
    const vector *const this,
    const size_t index
) {
    const void *current = this->buffer;
    for (size_t i = 0; i < index; ++i) {
        const size_t type_sz = *(size_t*)current;
        current = (char*)current + sizeof(size_t) + type_sz;
    }
    return (char*)current - (char*)this->buffer;
}

void vector_insert(
    vector *restrict const this,
    const size_t index,
    const size_t type_sz,
    const void *restrict const data
) {
    if (this == NULL || index > this->size) {
        return;
    }
    const size_t occupied_size = vector_offset_at(this, this->size);
    const size_t required_capacity = occupied_size + sizeof(size_t) + type_sz;
    if (required_capacity > this->capacity) {
        const size_t capacity = (required_capacity + 1) << 1;
        void *const buffer = malloc(capacity);
        if (buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", capacity);
            return;
        }
        const size_t offset = vector_offset_at(this, index);
        memcpy(buffer, this->buffer, offset);
        void *insertion_point = (char*)buffer + offset;
        memcpy((char*)insertion_point + sizeof(size_t) + type_sz, (char*)this->buffer + offset, occupied_size - offset);
        free(this->buffer);
        *(size_t*)insertion_point = type_sz;
        insertion_point = (char*)insertion_point + sizeof(size_t);
        memcpy(insertion_point, data, type_sz);
        this->buffer = buffer;
        this->capacity = capacity;
    }
    const size_t offset = vector_offset_at(this, index);
    void *insertion_point = (char*)this->buffer + offset;
    memcpy((char*)insertion_point + sizeof(size_t) + type_sz, insertion_point, occupied_size - offset);
    *(size_t*)insertion_point = type_sz;
    insertion_point = (char*)insertion_point + sizeof(size_t);
    memcpy(insertion_point, data, type_sz);
    ++this->size;
}

void vector_push_back(
    vector *restrict const this,
    const size_t type_sz,
    const void *restrict const data
) {
    if (this == NULL) {
        return;
    }
    const size_t occupied_size = vector_offset_at(this, this->size);
    const size_t required_capacity = occupied_size + sizeof(size_t) + type_sz;
    if (required_capacity > this->capacity) {
        const size_t capacity = (required_capacity + 1) << 1;
        void *const buffer = malloc(capacity);
        if (buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_push_back for capacity %lu\n", capacity);
            return;
        }
        memcpy(buffer, this->buffer, occupied_size);
        free(this->buffer);
        void *insertion_point = (char*)buffer + occupied_size;
        *(size_t*)insertion_point = type_sz;
        insertion_point = (char*)insertion_point + sizeof(size_t);
        memcpy(insertion_point, data, type_sz);
        this->buffer = buffer;
        this->capacity = capacity;
    }
    void *insertion_point = (char*)this->buffer + occupied_size;
    *(size_t*)insertion_point = type_sz;
    insertion_point = (char*)insertion_point + sizeof(size_t);
    memcpy(insertion_point, data, type_sz);
    ++this->size;
}

void vector_push_front(
    vector *restrict const this,
    const size_t type_sz,
    const void *restrict const data
) {
    if (this == NULL) {
        return;
    }
    const size_t occupied_size = vector_offset_at(this, this->size);
    const size_t required_capacity = occupied_size + sizeof(size_t) + type_sz;
    if (required_capacity > this->capacity) {
        const size_t capacity = (required_capacity + 1) << 1;
        void *const buffer = malloc(capacity);
        if (buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_push_front for capacity %lu\n", capacity);
            return;
        }
        memcpy((char*)buffer + sizeof(size_t) + type_sz, this->buffer, occupied_size);
        free(this->buffer);
        *(size_t*)buffer = type_sz;
        memcpy((char*)buffer + sizeof(size_t), data, type_sz);
        this->buffer = buffer;
        this->capacity = capacity;
    }
    memcpy((char*)this->buffer + sizeof(size_t) + type_sz, this->buffer, occupied_size);
    *(size_t*)this->buffer = type_sz;
    memcpy((char*)this->buffer + sizeof(size_t), data, type_sz);
    ++this->size;
}

void vector_remove(
    vector *const this,
    const size_t index
) {
    if (this == NULL || !this->size || index > this->size - 1) {
        return;
    }
    const size_t offset = vector_offset_at(this, index);
    void *const removal_point = (char*)this->buffer + offset;
    const size_t type_sz = *(size_t*)removal_point;
    const size_t removal_size = sizeof(size_t) + type_sz;
    const size_t occupied_size = vector_offset_at(this, this->size);
    const size_t required_capacity = occupied_size - removal_size;
    if (required_capacity + 1 < this->capacity >> 2) {
        const size_t capacity = (required_capacity + 1) << 1;
        void *const buffer = malloc(capacity);
        if (buffer != NULL) {
            memcpy(buffer, this->buffer, offset);
            memcpy((char*)buffer + offset, (char*)removal_point + removal_size, required_capacity - offset);
            free(this->buffer);
            this->buffer = buffer;
            this->capacity = capacity;
        } else {
            fprintf(stderr, "malloc NULL return in vector_remove for capacity %lu\n", capacity);
        }
    }
    memcpy(removal_point, (char*)removal_point + removal_size, required_capacity - offset);
    --this->size;
}

void vector_pop_back(vector *const this) {
    if (this == NULL || !this->size) {
        return;
    }
    const size_t offset = vector_offset_at(this, this->size - 1);
    if (offset + 1 < this->capacity >> 2) {
        const size_t capacity = (offset + 1) << 1;
        void *const buffer = malloc(capacity);
        if (buffer != NULL) {
            memcpy(buffer, this->buffer, offset);
            free(this->buffer);
            this->buffer = buffer;
            this->capacity = capacity;
        } else {
            fprintf(stderr, "malloc NULL return in vector_pop_back for capacity %lu\n", capacity);
        }
    }
    --this->size;
}

void vector_pop_front(vector *const this) {
    if (this == NULL || !this->size) {
        return;
    }
    const size_t type_sz = *(size_t*)this->buffer;
    const size_t removal_size = sizeof(size_t) + type_sz;
    const size_t occupied_size = vector_offset_at(this, this->size);
    const size_t required_capacity = occupied_size - removal_size;
    if (required_capacity + 1 < this->capacity >> 2) {
        const size_t capacity = (required_capacity + 1) << 1;
        void *const buffer = malloc(capacity);
        if (buffer != NULL) {
            memcpy(buffer, (char*)this->buffer + removal_size, required_capacity);
            free(this->buffer);
            this->buffer = buffer;
            this->capacity = capacity;
        } else {
            fprintf(stderr, "malloc NULL return in vector_pop_front for capacity %lu\n", capacity);
        }
    }
    memcpy(this->buffer, (char*)this->buffer + removal_size, required_capacity);
    --this->size;
}

[[nodiscard]] node_data *vector_at(
    const vector *const this,
    const size_t index
) {
    if (this == NULL || index >= this->size) {
        return NULL;
    }
    const size_t offset = vector_offset_at(this, index);
    return (node_data*)((char*)this->buffer + offset);
}

static void vector_swap(
    const vector *const this,
    size_t i,
    size_t j
) {
    if (i == j) {
        return;
    }
    if (j < i) {
        const size_t tmp = i;
        i = j;
        j = tmp;
    }
    const size_t i_offset = vector_offset_at(this, i);
    void *const i_pointer = (char*)this->buffer + i_offset;
    const size_t i_type_sz = *(size_t*)i_pointer;
    const size_t j_offset = vector_offset_at(this, j);
    void *const j_pointer = (char*)this->buffer + j_offset;
    const size_t j_type_sz = *(size_t*)j_pointer;
    if (i_type_sz == j_type_sz) {
        void *const tmp = malloc(i_type_sz);
        if (tmp == NULL) {
            fprintf(stderr, "malloc NULL return in vector_swap for type_sz %lu\n", i_type_sz);
            return;
        }
        void *const i_data = (char*)i_pointer + sizeof(size_t);
        memcpy(tmp, i_data, i_type_sz);
        void *const j_data = (char*)j_pointer + sizeof(size_t);
        memcpy(i_data, j_data, j_type_sz);
        memcpy(j_data, tmp, i_type_sz);
        free(tmp);
    } else if (i_type_sz < j_type_sz) {
        const size_t j_size = sizeof(size_t) + j_type_sz;
        void *const tmp = malloc(j_size);
        if (tmp == NULL) {
            fprintf(stderr, "malloc NULL return in vector_swap for type_sz %lu\n", j_size);
            return;
        }
        memcpy(tmp, j_pointer, j_size);
        const size_t diff = j_type_sz - i_type_sz;
        const size_t i_size = sizeof(size_t) + i_type_sz;
        memcpy((char*)i_pointer + i_size + diff, (char*)i_pointer + i_size, (char*)j_pointer - (char*)i_pointer - i_size - j_size);
        memcpy((char*)j_pointer + diff, i_pointer, i_size);
        memcpy(i_pointer, tmp, j_size);
        free(tmp);
    } else {
        const size_t i_size = sizeof(size_t) + i_type_sz;
        void *const tmp = malloc(i_size);
        if (tmp == NULL) {
            fprintf(stderr, "malloc NULL return in vector_swap for type_sz %lu\n", i_size);
            return;
        }
        memcpy(tmp, i_pointer, i_size);
        const size_t diff = i_type_sz - j_type_sz;
        const size_t j_size = sizeof(size_t) + j_type_sz;
        memcpy((char*)i_pointer + diff, (char*)i_pointer + i_size, (char*)j_pointer - (char*)i_pointer - i_size - j_size);
        memcpy(i_pointer, j_pointer, j_size);
        memcpy((char*)j_pointer - diff, tmp, i_size);
        free(tmp);
    }
}

void vector_reverse(const vector *const this) {
    size_t i = 0ul;
    size_t j = this->size - 1ul;
    while (i < j) {
        vector_swap(this, i, j);
        ++i;
        --j;
    }
}

void vector_delete(vector *const this) {
    free(this->buffer);
    free(this);
}

void vector_print(
    const vector *const this,
    const print_t print_data
) {
    printf("[");
    if (this->size) {
        const void *current = this->buffer;
        for (size_t i = 0; i < this->size - 1; ++i) {
            const size_t type_sz = *(size_t*)current;
            current = (char*)current + sizeof(size_t);
            print_data(current);
            printf(", ");
            current = (char*)current + type_sz;
        }
        current = (char*)current + sizeof(size_t);
        print_data(current);
    }
    printf("]\n");
}
