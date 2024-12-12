#include "vector.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define VECTOR_STACK_BUFFER_CAPACITY (sizeof(size_t) + 16ul)

struct vector_t {
    unsigned char stack_buffer[VECTOR_STACK_BUFFER_CAPACITY];
    size_t stack_size;
    void *heap_buffer;
    size_t heap_buffer_capacity;
    size_t heap_size;
};

[[nodiscard]] vector *vector_init(const size_t capacity) {
    vector *v = malloc(sizeof(vector));
    if (v == NULL) {
        fprintf(stderr, "malloc NULL return in vector_init");
        return v;
    }
    v->stack_size = 0ul;
    v->heap_size = 0ul;
    if (capacity > VECTOR_STACK_BUFFER_CAPACITY) {
        assert(capacity != 0ul);
        const size_t heap_buffer_capacity = capacity - VECTOR_STACK_BUFFER_CAPACITY;
        void *const heap_buffer = malloc(heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_init for capacity %lu\n", heap_buffer_capacity);
            v->heap_buffer = NULL;
            v->heap_buffer_capacity = 0ul;
            return v;
        }
        v->heap_buffer = heap_buffer;
        v->heap_buffer_capacity = capacity;
    }
    return v;
}

[[nodiscard]] size_t vector_size(const vector *const this) {
    if (this == NULL) {
        return 0ul;
    }
    return this->stack_size + this->heap_size;
}

[[nodiscard]] static size_t vector_offset_at(
    const vector *const this,
    const size_t index
) {
    const size_t size = vector_size(this);
    assert(index <= size);
    if (size == 0ul) {
        return 0ul;
    }
    const unsigned char *current = this->stack_buffer;
    size_t i = 0ul;
    for (; i < index && i < this->stack_size; ++i) {
        const size_t type_sz = *(size_t*)current;
        current += sizeof(size_t) + type_sz;
    }
    if (i >= index && this->stack_size > index) {
        return current - (unsigned char*)this->stack_buffer;
    }
    current = (unsigned char*)this->heap_buffer;
    for (size_t j = 0ul; i + j < index && j < this->heap_size; ++j) {
        const size_t type_sz = *(size_t*)current;
        current += sizeof(size_t) + type_sz;
    }
    return VECTOR_STACK_BUFFER_CAPACITY + (current - (unsigned char*)this->heap_buffer);
}

void vector_insert(
    vector *restrict const this,
    const size_t index,
    const size_t type_sz,
    const void *restrict const data
) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    if (index > size) {
        return;
    }
    const size_t occupied_size = vector_offset_at(this, size);
    const size_t offset = vector_offset_at(this, index);
    const size_t insertion_size = sizeof(size_t) + type_sz;
    const size_t required_stack_buffer_capacity = offset + insertion_size;
    if (required_stack_buffer_capacity <= VECTOR_STACK_BUFFER_CAPACITY) { // New element will be placed on stack
        // How much stack elements remains on stack after new element insertion ?
        size_t fitting_part_size = 0ul;
        unsigned char *const fitting_part_start = this->stack_buffer + offset;
        size_t i = index;
        for (; i < this->stack_size; ++i) {
            const unsigned char *const current = fitting_part_start + fitting_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            if (required_stack_buffer_capacity + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                break;
            }
            fitting_part_size += current_size;
        }
        // How much stack elements must be moved from stack to heap after new element insertion ?
        size_t not_fits_after = 0ul;
        size_t non_fitting_part_size = 0ul;
        const unsigned char *const non_fitting_part_start = fitting_part_start + fitting_part_size;
        for (; i < this->stack_size; ++i) {
            const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            non_fitting_part_size += current_size;
            ++not_fits_after;
        }
        // Move non fitting part from stack buffer to heap buffer
        size_t heap_buffer_occupied_size;
        if (occupied_size > VECTOR_STACK_BUFFER_CAPACITY) {
            heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
        } else {
            heap_buffer_occupied_size = 0ul;
        }
        const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + non_fitting_part_size;
        if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
            const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
            void *const heap_buffer = malloc(heap_buffer_capacity);
            if (heap_buffer == NULL) {
                fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                return;
            }
            memcpy(heap_buffer, non_fitting_part_start, non_fitting_part_size);
            memcpy((unsigned char*)heap_buffer + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
            free(this->heap_buffer);
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        } else { // No heap buffer realloc needed
            memcpy((unsigned char*)this->heap_buffer + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
            memcpy(this->heap_buffer, non_fitting_part_start, non_fitting_part_size);
        }
        memcpy(fitting_part_start + insertion_size, fitting_part_start, fitting_part_size);
        *(size_t*)fitting_part_start = type_sz;
        memcpy(fitting_part_start + sizeof(size_t), data, type_sz);
        this->stack_size = this->stack_size - not_fits_after + 1ul;
        this->heap_size += not_fits_after;
    } else { // New element will be placed on heap
        if (offset <= VECTOR_STACK_BUFFER_CAPACITY) { // Offset is on stack
            // How much stack elements must be moved from stack to heap after new element insertion ?
            size_t not_fits_after = 0ul;
            size_t non_fitting_part_size = 0ul;
            const unsigned char *const non_fitting_part_start = this->stack_buffer + offset;
            for (size_t i = index; i < this->stack_size; ++i) {
                const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
                const size_t current_type_sz = *(size_t*)current;
                const size_t current_size = sizeof(size_t) + current_type_sz;
                non_fitting_part_size += current_size;
                ++not_fits_after;
            }
            // Move non fitting part from stack buffer to heap buffer
            size_t heap_buffer_occupied_size;
            if (occupied_size > VECTOR_STACK_BUFFER_CAPACITY) {
                heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            } else {
                heap_buffer_occupied_size = 0ul;
            }
            const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + non_fitting_part_size + insertion_size;
            if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                void *const heap_buffer = malloc(heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                *(size_t*)heap_buffer = type_sz;
                memcpy((unsigned char*)heap_buffer + sizeof(size_t), data, type_sz);
                memcpy((unsigned char*)heap_buffer + insertion_size, non_fitting_part_start, non_fitting_part_size);
                memcpy((unsigned char*)heap_buffer + insertion_size + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                free(this->heap_buffer);
                this->heap_buffer = heap_buffer;
                this->heap_buffer_capacity = heap_buffer_capacity;
            } else { // No heap buffer realloc needed
                memcpy((unsigned char*)this->heap_buffer + insertion_size + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                *(size_t*)this->heap_buffer = type_sz;
                memcpy((unsigned char*)this->heap_buffer + sizeof(size_t), data, type_sz);
                memcpy((unsigned char*)this->heap_buffer + insertion_size, non_fitting_part_start, non_fitting_part_size);
            }
            this->stack_size -= not_fits_after;
            this->heap_size += 1ul + not_fits_after;
        } else { // Offset is on heap
            // Insert in heap buffer
            const size_t heap_buffer_offset = offset - VECTOR_STACK_BUFFER_CAPACITY;
            size_t heap_buffer_occupied_size;
            if (occupied_size > VECTOR_STACK_BUFFER_CAPACITY) {
                heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            } else {
                heap_buffer_occupied_size = 0ul;
            }
            const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + insertion_size;
            if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                void *const heap_buffer = malloc(heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                memcpy(heap_buffer, this->heap_buffer, heap_buffer_offset);
                void *heap_buffer_insertion_point = (unsigned char*)heap_buffer + heap_buffer_offset;
                *(size_t*)heap_buffer_insertion_point = type_sz;
                memcpy((unsigned char*)heap_buffer_insertion_point + sizeof(size_t), data, type_sz);
                memcpy((unsigned char*)heap_buffer_insertion_point + insertion_size, (unsigned char*)this->heap_buffer + heap_buffer_offset, heap_buffer_occupied_size - heap_buffer_offset);
                free(this->heap_buffer);
                this->heap_buffer = heap_buffer;
                this->heap_buffer_capacity = heap_buffer_capacity;
            } else { // No heap buffer realloc needed
                void *heap_buffer_insertion_point = (unsigned char*)this->heap_buffer + heap_buffer_offset;
                memcpy((unsigned char*)heap_buffer_insertion_point + insertion_size, heap_buffer_insertion_point, heap_buffer_occupied_size - heap_buffer_offset);
                *(size_t*)heap_buffer_insertion_point = type_sz;
                memcpy((unsigned char*)heap_buffer_insertion_point + sizeof(size_t), data, type_sz);
            }
            ++this->heap_size;
        }
    }
}

void vector_push_back(
    vector *restrict const this,
    const size_t type_sz,
    const void *restrict const data
) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    const size_t offset = vector_offset_at(this, size);
    const size_t insertion_size = sizeof(size_t) + type_sz;
    const size_t required_stack_buffer_capacity = offset + insertion_size;
    if (required_stack_buffer_capacity <= VECTOR_STACK_BUFFER_CAPACITY) { // New element will be placed on stack
        unsigned char *const stack_buffer_insertion_point = this->stack_buffer + offset;
        *(size_t*)stack_buffer_insertion_point = type_sz;
        memcpy(stack_buffer_insertion_point + sizeof(size_t), data, type_sz);
        ++this->stack_size;
    } else { // New element will be placed on heap
        // Insert in heap buffer
        size_t heap_buffer_offset;
        if (offset <= VECTOR_STACK_BUFFER_CAPACITY) { // Offset is on stack
            heap_buffer_offset = 0ul;
        } else { // Offset is on heap
            heap_buffer_offset = offset - VECTOR_STACK_BUFFER_CAPACITY;
        }
        const size_t occupied_size = vector_offset_at(this, size);
        const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
        const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + insertion_size;
        if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
            const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
            void *const heap_buffer = malloc(heap_buffer_capacity);
            if (heap_buffer == NULL) {
                fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                return;
            }
            memcpy(heap_buffer, this->heap_buffer, heap_buffer_offset);
            void *heap_buffer_insertion_point = (unsigned char*)heap_buffer + heap_buffer_offset;
            *(size_t*)heap_buffer_insertion_point = type_sz;
            memcpy((unsigned char*)heap_buffer_insertion_point + sizeof(size_t), data, type_sz);
            memcpy((unsigned char*)heap_buffer_insertion_point + insertion_size, (unsigned char*)this->heap_buffer + heap_buffer_offset, heap_buffer_occupied_size - heap_buffer_offset);
            free(this->heap_buffer);
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        } else { // No heap buffer realloc needed
            void *heap_buffer_insertion_point = (unsigned char*)this->heap_buffer + heap_buffer_offset;
            memcpy((unsigned char*)heap_buffer_insertion_point + insertion_size, heap_buffer_insertion_point, heap_buffer_occupied_size - heap_buffer_offset);
            *(size_t*)heap_buffer_insertion_point = type_sz;
            memcpy((unsigned char*)heap_buffer_insertion_point + sizeof(size_t), data, type_sz);
        }
        ++this->heap_size;
    }
}

void vector_push_front(
    vector *restrict const this,
    const size_t type_sz,
    const void *restrict const data
) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    const size_t occupied_size = vector_offset_at(this, size);
    const size_t insertion_size = sizeof(size_t) + type_sz;
    if (insertion_size <= VECTOR_STACK_BUFFER_CAPACITY) { // New element will be placed on stack
        // How much stack elements remains on stack after new element insertion ?
        size_t fitting_part_size = 0ul;
        size_t i = 0ul;
        for (; i < this->stack_size; ++i) {
            const unsigned char *const current = this->stack_buffer + fitting_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            if (insertion_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                break;
            }
            fitting_part_size += current_size;
        }
        // How much stack elements must be moved from stack to heap after new element insertion ?
        size_t not_fits_after = 0ul;
        size_t non_fitting_part_size = 0ul;
        const unsigned char *const non_fitting_part_start = this->stack_buffer + fitting_part_size;
        for (; i < this->stack_size; ++i) {
            const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            non_fitting_part_size += current_size;
            ++not_fits_after;
        }
        // Move non fitting part from stack buffer to heap buffer
        const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
        const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + non_fitting_part_size;
        if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
            const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
            void *const heap_buffer = malloc(heap_buffer_capacity);
            if (heap_buffer == NULL) {
                fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                return;
            }
            memcpy(heap_buffer, non_fitting_part_start, non_fitting_part_size);
            memcpy((unsigned char*)heap_buffer + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
            free(this->heap_buffer);
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        } else { // No heap buffer realloc needed
            memcpy((unsigned char*)this->heap_buffer + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
            memcpy(this->heap_buffer, non_fitting_part_start, non_fitting_part_size);
        }
        memcpy(this->stack_buffer + insertion_size, this->stack_buffer, fitting_part_size);
        *(size_t*)this->stack_buffer = type_sz;
        memcpy(this->stack_buffer + sizeof(size_t), data, type_sz);
        this->stack_size = this->stack_size - not_fits_after + 1ul;
        this->heap_size += not_fits_after;
    } else { // New element will be placed on heap
        if (VECTOR_STACK_BUFFER_CAPACITY > 0ul) { // Offset is on stack
            // How much stack elements must be moved from stack to heap after new element insertion ?
            size_t non_fitting_part_size = 0ul;
            const unsigned char *const non_fitting_part_start = this->stack_buffer + 0ul;
            for (size_t i = 0ul; i < this->stack_size; ++i) {
                const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
                const size_t current_type_sz = *(size_t*)current;
                const size_t current_size = sizeof(size_t) + current_type_sz;
                non_fitting_part_size += current_size;
            }
            // Move non fitting part from stack buffer to heap buffer
            const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + non_fitting_part_size + insertion_size;
            if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                void *const heap_buffer = malloc(heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                *(size_t*)heap_buffer = type_sz;
                memcpy((unsigned char*)heap_buffer + sizeof(size_t), data, type_sz);
                memcpy((unsigned char*)heap_buffer + insertion_size, non_fitting_part_start, non_fitting_part_size);
                memcpy((unsigned char*)heap_buffer + insertion_size + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                free(this->heap_buffer);
                this->heap_buffer = heap_buffer;
                this->heap_buffer_capacity = heap_buffer_capacity;
            } else { // No heap buffer realloc needed
                memcpy((unsigned char*)this->heap_buffer + insertion_size + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                *(size_t*)this->heap_buffer = type_sz;
                memcpy((unsigned char*)this->heap_buffer + sizeof(size_t), data, type_sz);
                memcpy((unsigned char*)this->heap_buffer + insertion_size, non_fitting_part_start, non_fitting_part_size);
            }
            this->stack_size = 0ul;
            this->heap_size += 1ul + this->stack_size;
        } else { // Offset is on heap
            // Insert in heap buffer
            const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + insertion_size;
            if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                void *const heap_buffer = malloc(heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                *(size_t*)heap_buffer = type_sz;
                memcpy((unsigned char*)heap_buffer + sizeof(size_t), data, type_sz);
                memcpy((unsigned char*)heap_buffer + insertion_size, this->heap_buffer, heap_buffer_occupied_size);
                free(this->heap_buffer);
                this->heap_buffer = heap_buffer;
                this->heap_buffer_capacity = heap_buffer_capacity;
            } else { // No heap buffer realloc needed
                memcpy((unsigned char*)this->heap_buffer + insertion_size, this->heap_buffer, heap_buffer_occupied_size);
                *(size_t*)this->heap_buffer = type_sz;
                memcpy((unsigned char*)this->heap_buffer + sizeof(size_t), data, type_sz);
            }
            ++this->heap_size;
        }
    }
}

void vector_remove(
    vector *const this,
    const size_t index
) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    if (index >= size) {
        return;
    }
    size_t required_heap_buffer_capacity;
    const size_t occupied_size = vector_offset_at(this, size);
    const size_t offset = vector_offset_at(this, index);
    if (offset < VECTOR_STACK_BUFFER_CAPACITY) { // Element to remove is on stack
        unsigned char *const stack_buffer_removal_point = this->stack_buffer + offset;
        const size_t removal_type_sz = *(size_t*)stack_buffer_removal_point;
        const size_t removal_size = sizeof(size_t) + removal_type_sz;
        // How much elements are there on stack after the element being removed ?
        size_t remaining_part_size = 0ul;
        const unsigned char *const remaining_part_start = this->stack_buffer + offset + removal_size;
        for (size_t i = index + 1ul; i < this->stack_size; ++i) {
            const unsigned char *const current = remaining_part_start + remaining_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            remaining_part_size += current_size;
        }
        // Remove element on stack
        memcpy(stack_buffer_removal_point, remaining_part_start, remaining_part_size);
        // How much elements to move from heap to stack after removal ?
        const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - offset - remaining_part_size;
        size_t fits_after = 0ul;
        size_t fitting_part_size = 0ul;
        for (size_t i = 0ul; i < this->heap_size; ++i) {
            const unsigned char *const current = this->heap_buffer + fitting_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            if (stack_buffer_free_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                break;
            }
            fitting_part_size += current_size;
            ++fits_after;
        }
        // Move fitting part from heap buffer to stack buffer
        const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
        required_heap_buffer_capacity = heap_buffer_occupied_size - fitting_part_size;
        memcpy(stack_buffer_removal_point + remaining_part_size, this->heap_buffer, fitting_part_size);
        memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + fitting_part_size, required_heap_buffer_capacity);
        this->stack_size = this->stack_size + fits_after - 1ul;
        this->heap_size -= fits_after;
    } else { // Element to remove is on heap
        const size_t heap_buffer_offset = offset - VECTOR_STACK_BUFFER_CAPACITY;
        if (heap_buffer_offset == 0ul) { // Removing the first element in heap buffer
            const size_t removal_type_sz = *(size_t*)this->heap_buffer;
            const size_t removal_size = sizeof(size_t) + removal_type_sz;
            // How much elements to move from heap to stack after removal ?
            const size_t stack_buffer_occupied_size = vector_offset_at(this, this->stack_size);
            const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - stack_buffer_occupied_size;
            size_t fits_after = 0ul;
            size_t fitting_part_size = 0ul;
            for (size_t i = 1ul; i < this->heap_size; ++i) {
                const unsigned char *const current = this->heap_buffer + fitting_part_size + removal_size;
                const size_t current_type_sz = *(size_t*)current;
                const size_t current_size = sizeof(size_t) + current_type_sz;
                if (stack_buffer_free_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                    break;
                }
                fitting_part_size += current_size;
                ++fits_after;
            }
            // Move fitting part from heap buffer to stack buffer
            const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            required_heap_buffer_capacity = heap_buffer_occupied_size - fitting_part_size - removal_size;
            memcpy(this->stack_buffer + stack_buffer_occupied_size, (unsigned char*)this->heap_buffer + removal_size, fitting_part_size);
            // Remove element on heap
            memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + removal_size + fitting_part_size, required_heap_buffer_capacity);
            this->stack_size += fits_after;
            this->heap_size -= fits_after + 1ul;
        } else { // Removing non-first element in heap buffer
            unsigned char *const heap_buffer_removal_point = (unsigned char*)this->heap_buffer + heap_buffer_offset;
            const size_t removal_type_sz = *(size_t*)heap_buffer_removal_point;
            const size_t removal_size = sizeof(size_t) + removal_type_sz;
            const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            required_heap_buffer_capacity = heap_buffer_occupied_size - removal_size;
            // Remove element on heap
            memcpy(heap_buffer_removal_point, heap_buffer_removal_point + removal_size, required_heap_buffer_capacity);
            --this->heap_size;
        }
    }
    // Realloc heap buffer if needed
    if (required_heap_buffer_capacity + 1ul < this->heap_buffer_capacity >> 2) {
        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
        void *const heap_buffer = malloc(heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_remove for capacity %lu\n", heap_buffer_capacity);
        } else {
            memcpy(heap_buffer, this->heap_buffer, required_heap_buffer_capacity);
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
}

void vector_pop_back(vector *const this) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    if (size == 0ul) {
        return;
    }
    size_t required_heap_buffer_capacity;
    const size_t occupied_size = vector_offset_at(this, size);
    const size_t offset = vector_offset_at(this, size - 1ul);
    if (offset < VECTOR_STACK_BUFFER_CAPACITY) { // Element to remove is on stack
        unsigned char *const stack_buffer_removal_point = this->stack_buffer + offset;
        // How much elements to move from heap to stack after removal ?
        const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - offset;
        size_t fits_after = 0ul;
        size_t fitting_part_size = 0ul;
        for (size_t i = 0ul; i < this->heap_size; ++i) {
            const unsigned char *const current = this->heap_buffer + fitting_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            if (stack_buffer_free_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                break;
            }
            fitting_part_size += current_size;
            ++fits_after;
        }
        // Move fitting part from heap buffer to stack buffer
        const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
        required_heap_buffer_capacity = heap_buffer_occupied_size - fitting_part_size;
        memcpy(stack_buffer_removal_point, this->heap_buffer, fitting_part_size);
        memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + fitting_part_size, required_heap_buffer_capacity);
        this->stack_size = this->stack_size + fits_after - 1ul;
        this->heap_size -= fits_after;
    } else { // Element to remove is on heap
        const size_t heap_buffer_offset = offset - VECTOR_STACK_BUFFER_CAPACITY;
        unsigned char *const heap_buffer_removal_point = (unsigned char*)this->heap_buffer + heap_buffer_offset;
        const size_t removal_type_sz = *(size_t*)heap_buffer_removal_point;
        const size_t removal_size = sizeof(size_t) + removal_type_sz;
        const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
        required_heap_buffer_capacity = heap_buffer_occupied_size - removal_size;
        // Remove element on heap
        memcpy(heap_buffer_removal_point, heap_buffer_removal_point + removal_size, required_heap_buffer_capacity);
        --this->heap_size;
    }
    // Realloc heap buffer if needed
    if (required_heap_buffer_capacity + 1ul < this->heap_buffer_capacity >> 2) {
        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
        void *const heap_buffer = malloc(heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_remove for capacity %lu\n", heap_buffer_capacity);
        } else {
            memcpy(heap_buffer, this->heap_buffer, required_heap_buffer_capacity);
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
}

void vector_pop_front(vector *const this) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    if (size == 0ul) {
        return;
    }
    size_t required_heap_buffer_capacity;
    const size_t occupied_size = vector_offset_at(this, size);
    if (VECTOR_STACK_BUFFER_CAPACITY > 0ul) { // Element to remove is on stack
        const size_t removal_type_sz = *(size_t*)this->stack_buffer;
        const size_t removal_size = sizeof(size_t) + removal_type_sz;
        // How much elements are there on stack after the element being removed ?
        size_t remaining_part_size = 0ul;
        const unsigned char *const remaining_part_start = this->stack_buffer + removal_size;
        for (size_t i = 1ul; i < this->stack_size; ++i) {
            const unsigned char *const current = remaining_part_start + remaining_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            remaining_part_size += current_size;
        }
        // Remove element on stack
        memcpy(this->stack_buffer, remaining_part_start, remaining_part_size);
        // How much elements to move from heap to stack after removal ?
        const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - 0ul - remaining_part_size;
        size_t fits_after = 0ul;
        size_t fitting_part_size = 0ul;
        for (size_t i = 0ul; i < this->heap_size; ++i) {
            const unsigned char *const current = this->heap_buffer + fitting_part_size;
            const size_t current_type_sz = *(size_t*)current;
            const size_t current_size = sizeof(size_t) + current_type_sz;
            if (stack_buffer_free_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                break;
            }
            fitting_part_size += current_size;
            ++fits_after;
        }
        // Move fitting part from heap buffer to stack buffer
        const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
        required_heap_buffer_capacity = heap_buffer_occupied_size - fitting_part_size;
        memcpy(this->stack_buffer + remaining_part_size, this->heap_buffer, fitting_part_size);
        memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + fitting_part_size, required_heap_buffer_capacity);
        this->stack_size = this->stack_size + fits_after - 1ul;
        this->heap_size -= fits_after;
    } else { // Element to remove is on heap
        const size_t removal_type_sz = *(size_t*)this->heap_buffer;
        const size_t removal_size = sizeof(size_t) + removal_type_sz;
        // Remove element on heap
        required_heap_buffer_capacity = occupied_size - removal_size;
        memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + removal_size, required_heap_buffer_capacity);
        --this->heap_size;
    }
    // Realloc heap buffer if needed
    if (required_heap_buffer_capacity + 1ul < this->heap_buffer_capacity >> 2) {
        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
        void *const heap_buffer = malloc(heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in vector_remove for capacity %lu\n", heap_buffer_capacity);
        } else {
            memcpy(heap_buffer, this->heap_buffer, required_heap_buffer_capacity);
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
}

void vector_set(
    vector *restrict const this,
    const size_t index,
    const size_t type_sz,
    const void *restrict const data
) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    if (index >= size) {
        return;
    }
    const size_t occupied_size = vector_offset_at(this, size);
    const size_t offset = vector_offset_at(this, index);
    const size_t insertion_size = sizeof(size_t) + type_sz;
    if (offset < VECTOR_STACK_BUFFER_CAPACITY) { // Element to replace is on stack
        unsigned char *const stack_buffer_replacement_point = this->stack_buffer + offset;
        const size_t replacement_type_sz = *(size_t*)stack_buffer_replacement_point;
        const size_t replacement_size = sizeof(size_t) + replacement_type_sz;
        if (type_sz < replacement_type_sz) {
            // How much elements are there on stack after the element being replaced ?
            size_t remaining_part_size = 0ul;
            const unsigned char *const remaining_part_start = this->stack_buffer + offset + replacement_size;
            for (size_t i = index + 1ul; i < this->stack_size; ++i) {
                const unsigned char *const current = remaining_part_start + remaining_part_size;
                const size_t current_type_sz = *(size_t*)current;
                const size_t current_size = sizeof(size_t) + current_type_sz;
                remaining_part_size += current_size;
            }
            memcpy(stack_buffer_replacement_point + insertion_size, remaining_part_start, remaining_part_size);
            // Replacing element on stack
            *(size_t*)stack_buffer_replacement_point = type_sz;
            memcpy(stack_buffer_replacement_point + sizeof(size_t), data, type_sz);
            // How much elements to move from heap to stack after replacement ?
            const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - offset - remaining_part_size + replacement_size - type_sz;
            size_t fits_after = 0ul;
            size_t fitting_part_size = 0ul;
            for (size_t i = 0ul; i < this->heap_size; ++i) {
                const unsigned char *const current = this->heap_buffer + fitting_part_size;
                const size_t current_type_sz = *(size_t*)current;
                const size_t current_size = sizeof(size_t) + current_type_sz;
                if (stack_buffer_free_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                    break;
                }
                fitting_part_size += current_size;
                ++fits_after;
            }
            // Move fitting part from heap buffer to stack buffer
            const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            const size_t required_heap_buffer_capacity = heap_buffer_occupied_size - fitting_part_size;
            memcpy(stack_buffer_replacement_point + insertion_size + remaining_part_size, this->heap_buffer, fitting_part_size);
            memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + fitting_part_size, required_heap_buffer_capacity);
            this->stack_size = this->stack_size + fits_after;
            this->heap_size -= fits_after;
            // Realloc heap buffer if needed
            if (required_heap_buffer_capacity + 1ul < this->heap_buffer_capacity >> 2) {
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                void *const heap_buffer = malloc(heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_remove for capacity %lu\n", heap_buffer_capacity);
                } else {
                    memcpy(heap_buffer, this->heap_buffer, required_heap_buffer_capacity);
                    this->heap_buffer = heap_buffer;
                    this->heap_buffer_capacity = heap_buffer_capacity;
                }
            }
        } else if (type_sz > replacement_type_sz) {
            const size_t required_stack_buffer_capacity = offset + insertion_size - replacement_size;
            if (required_stack_buffer_capacity <= VECTOR_STACK_BUFFER_CAPACITY) { // New element will be placed on stack
                // How much stack elements remains on stack after new element insertion ?
                size_t fitting_part_size = 0ul;
                const unsigned char *const fitting_part_start = stack_buffer_replacement_point + replacement_size;
                size_t i = index;
                for (; i < this->stack_size; ++i) {
                    const unsigned char *const current = fitting_part_start + fitting_part_size;
                    const size_t current_type_sz = *(size_t*)current;
                    const size_t current_size = sizeof(size_t) + current_type_sz;
                    if (required_stack_buffer_capacity + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                        break;
                    }
                    fitting_part_size += current_size;
                }
                // How much stack elements must be moved from stack to heap after new element insertion ?
                size_t not_fits_after = 0ul;
                size_t non_fitting_part_size = 0ul;
                const unsigned char *const non_fitting_part_start = fitting_part_start + fitting_part_size;
                for (; i < this->stack_size; ++i) {
                    const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
                    const size_t current_type_sz = *(size_t*)current;
                    const size_t current_size = sizeof(size_t) + current_type_sz;
                    non_fitting_part_size += current_size;
                    ++not_fits_after;
                }
                // Move non fitting part from stack buffer to heap buffer
                size_t heap_buffer_occupied_size;
                if (occupied_size > VECTOR_STACK_BUFFER_CAPACITY) {
                    heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                } else {
                    heap_buffer_occupied_size = 0ul;
                }
                const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + non_fitting_part_size;
                if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                    const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                    void *const heap_buffer = malloc(heap_buffer_capacity);
                    if (heap_buffer == NULL) {
                        fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                        return;
                    }
                    memcpy(heap_buffer, non_fitting_part_start, non_fitting_part_size);
                    memcpy((unsigned char*)heap_buffer + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                    free(this->heap_buffer);
                    this->heap_buffer = heap_buffer;
                    this->heap_buffer_capacity = heap_buffer_capacity;
                } else { // No heap buffer realloc needed
                    memcpy((unsigned char*)this->heap_buffer + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                    memcpy(this->heap_buffer, non_fitting_part_start, non_fitting_part_size);
                }
                memcpy(stack_buffer_replacement_point + insertion_size, fitting_part_start, fitting_part_size);
                *(size_t*)stack_buffer_replacement_point = type_sz;
                memcpy(stack_buffer_replacement_point + sizeof(size_t), data, type_sz);
                this->stack_size -= not_fits_after;
                this->heap_size += not_fits_after;
            } else { // New element will be placed on heap
                // How much stack elements must be moved from stack to heap after new element insertion ?
                size_t not_fits_after = 0ul;
                size_t non_fitting_part_size = 0ul;
                const unsigned char *const non_fitting_part_start = this->stack_buffer + offset + replacement_size;
                for (size_t i = index; i < this->stack_size; ++i) {
                    const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
                    const size_t current_type_sz = *(size_t*)current;
                    const size_t current_size = sizeof(size_t) + current_type_sz;
                    non_fitting_part_size += current_size;
                    ++not_fits_after;
                }
                // Move non fitting part from stack buffer to heap buffer
                size_t heap_buffer_occupied_size;
                if (occupied_size > VECTOR_STACK_BUFFER_CAPACITY) {
                    heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                } else {
                    heap_buffer_occupied_size = 0ul;
                }
                const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + non_fitting_part_size + insertion_size;
                if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                    const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                    void *const heap_buffer = malloc(heap_buffer_capacity);
                    if (heap_buffer == NULL) {
                        fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                        return;
                    }
                    *(size_t*)heap_buffer = type_sz;
                    memcpy((unsigned char*)heap_buffer + sizeof(size_t), data, type_sz);
                    memcpy((unsigned char*)heap_buffer + insertion_size, non_fitting_part_start, non_fitting_part_size);
                    memcpy((unsigned char*)heap_buffer + insertion_size + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                    free(this->heap_buffer);
                    this->heap_buffer = heap_buffer;
                    this->heap_buffer_capacity = heap_buffer_capacity;
                } else { // No heap buffer realloc needed
                    memcpy((unsigned char*)this->heap_buffer + insertion_size + non_fitting_part_size, this->heap_buffer, heap_buffer_occupied_size);
                    *(size_t*)this->heap_buffer = type_sz;
                    memcpy((unsigned char*)this->heap_buffer + sizeof(size_t), data, type_sz);
                    memcpy((unsigned char*)this->heap_buffer + insertion_size, non_fitting_part_start, non_fitting_part_size);
                }
                this->stack_size -= 1ul + not_fits_after;
                this->heap_size += 1ul + not_fits_after;
            }
        } else { // type_sz == replacement_type_sz
            memcpy(stack_buffer_replacement_point + sizeof(size_t), data, type_sz);
        }
    } else { // Element to replace is on heap
        const size_t heap_buffer_offset = offset - VECTOR_STACK_BUFFER_CAPACITY;
        unsigned char *const heap_buffer_replacement_point = this->heap_buffer + heap_buffer_offset;
        const size_t replacement_type_sz = *(size_t*)heap_buffer_replacement_point;
        const size_t replacement_size = sizeof(size_t) + replacement_type_sz;
        if (type_sz < replacement_type_sz) {
            size_t required_heap_buffer_capacity;
            if (heap_buffer_offset == 0ul) { // Replacing the first element in heap buffer
                // How much elements to move from heap to stack after removal ?
                const size_t stack_buffer_occupied_size = vector_offset_at(this, this->stack_size);
                const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - stack_buffer_occupied_size;
                if (stack_buffer_free_size + insertion_size <= VECTOR_STACK_BUFFER_CAPACITY) {
                    size_t fits_after = 0ul;
                    size_t fitting_part_size = 0ul;
                    for (size_t i = 1ul; i < this->heap_size; ++i) {
                        const unsigned char *const current = this->heap_buffer + replacement_size + fitting_part_size;
                        const size_t current_type_sz = *(size_t*)current;
                        const size_t current_size = sizeof(size_t) + current_type_sz;
                        if (stack_buffer_free_size + insertion_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                            break;
                        }
                        fitting_part_size += current_size;
                        ++fits_after;
                    }
                    // Move fitting part from heap buffer to stack buffer
                    const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                    required_heap_buffer_capacity = heap_buffer_occupied_size - fitting_part_size - replacement_size;
                    unsigned char *const stack_buffer_insertion_point = this->stack_buffer + stack_buffer_occupied_size;
                    *(size_t*)stack_buffer_insertion_point = type_sz;
                    memcpy(stack_buffer_insertion_point + type_sz, data, type_sz);
                    memcpy(stack_buffer_insertion_point + insertion_size, (unsigned char*)this->heap_buffer + replacement_size, fitting_part_size);
                    // Remove element on heap
                    memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + replacement_size + fitting_part_size, required_heap_buffer_capacity);
                    this->stack_size += 1ul + fits_after;
                    this->heap_size -= 1ul + fits_after;
                } else {
                    // Move fitting part from heap buffer to stack buffer
                    const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                    required_heap_buffer_capacity = heap_buffer_occupied_size + insertion_size - replacement_size;
                    // Remove element on heap
                    memcpy((unsigned char*)this->heap_buffer + insertion_size, (unsigned char*)this->heap_buffer + replacement_size, heap_buffer_occupied_size - replacement_size);
                    *(size_t*)this->heap_buffer = type_sz;
                    memcpy((unsigned char*)this->heap_buffer + sizeof(size_t), data, type_sz);
                }
            } else { // Removing non-first element in heap buffer
                unsigned char *const heap_buffer_removal_point = (unsigned char*)this->heap_buffer + heap_buffer_offset;
                const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                required_heap_buffer_capacity = heap_buffer_occupied_size + insertion_size - replacement_size;
                // Remove element on heap
                memcpy(heap_buffer_removal_point + insertion_size, heap_buffer_removal_point + replacement_size, heap_buffer_occupied_size - heap_buffer_offset - replacement_size);
                *(size_t*)heap_buffer_removal_point = type_sz;
                memcpy(heap_buffer_removal_point + sizeof(size_t), data, type_sz);
            }
        } else if (type_sz > replacement_type_sz) {
            size_t heap_buffer_occupied_size;
            if (occupied_size > VECTOR_STACK_BUFFER_CAPACITY) {
                heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
            } else {
                heap_buffer_occupied_size = 0ul;
            }
            const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + insertion_size - replacement_size;
            if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                void *const heap_buffer = malloc(heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                memcpy(heap_buffer, this->heap_buffer, heap_buffer_offset);
                void *heap_buffer_insertion_point = (unsigned char*)heap_buffer + heap_buffer_offset;
                *(size_t*)heap_buffer_insertion_point = type_sz;
                memcpy((unsigned char*)heap_buffer_insertion_point + sizeof(size_t), data, type_sz);
                memcpy((unsigned char*)heap_buffer_insertion_point + insertion_size, (unsigned char*)this->heap_buffer + heap_buffer_offset + replacement_size, heap_buffer_occupied_size - heap_buffer_offset - replacement_size);
                free(this->heap_buffer);
                this->heap_buffer = heap_buffer;
                this->heap_buffer_capacity = heap_buffer_capacity;
            } else { // No heap buffer realloc needed
                void *heap_buffer_insertion_point = (unsigned char*)this->heap_buffer + heap_buffer_offset;
                memcpy((unsigned char*)heap_buffer_insertion_point + insertion_size, heap_buffer_insertion_point + replacement_size, heap_buffer_occupied_size - heap_buffer_offset - replacement_size);
                *(size_t*)heap_buffer_insertion_point = type_sz;
                memcpy((unsigned char*)heap_buffer_insertion_point + sizeof(size_t), data, type_sz);
            }
        } else { // type_sz == replacement_type_sz
            memcpy(heap_buffer_replacement_point + sizeof(size_t), data, type_sz);
        }
    }
}

[[nodiscard]] node_data vector_get(
    const vector *const this,
    const size_t index
) {
    node_data nd = {
        .type_sz = 0ul,
        .data = NULL
    };
    if (this == NULL) {
        return nd;
    }
    const size_t size = vector_size(this);
    if (index >= size) {
        return nd;
    }
    const size_t offset = vector_offset_at(this, index);
    unsigned char *element_at;
    if (offset < VECTOR_STACK_BUFFER_CAPACITY) {
        element_at = (unsigned char*)this->stack_buffer + offset;
    } else {
        const size_t heap_buffer_offset = offset - VECTOR_STACK_BUFFER_CAPACITY;
        element_at = (unsigned char*)this->heap_buffer + heap_buffer_offset;
    }
    nd.type_sz = *(size_t*)element_at;
    nd.data = element_at + sizeof(size_t);
    return nd;
}

static void vector_swap(
    vector *const this,
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
    const size_t j_offset = vector_offset_at(this, j);
    if (i_offset < VECTOR_STACK_BUFFER_CAPACITY) { // i-th element is on stack
        unsigned char *const i_element = this->stack_buffer + i_offset;
        const size_t i_type_sz = *(size_t*)i_element;
        const size_t i_size = sizeof(size_t) + i_type_sz;
        if (j_offset < VECTOR_STACK_BUFFER_CAPACITY) { // j-th element is on stack
            unsigned char *const j_element = this->stack_buffer + j_offset;
            const size_t j_type_sz = *(size_t*)j_element;
            const size_t j_size = sizeof(size_t) + j_type_sz;
            if (i_size < j_size) {
                void *const tmp = malloc(j_size);
                if (tmp == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", j_size);
                    return;
                }
                memcpy(tmp, j_element, j_size);
                memcpy(j_element + j_size - i_size, i_element, i_size);
                memcpy(i_element + j_size, i_element + i_size, j_element - i_element - i_size);
                memcpy(i_element, tmp, j_size);
                free(tmp);
            } else if (i_size > j_size) {
                void *const tmp = malloc(i_size);
                if (tmp == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", i_size);
                    return;
                }
                memcpy(tmp, i_element, i_size);
                memcpy(i_element, j_element, j_size);
                memcpy(i_element + j_size, i_element + i_size, j_element - i_element - i_size);
                memcpy(j_element + j_size - i_size, tmp, i_size);
                free(tmp);
            } else { // i_size == j_size
                void *const tmp = malloc(i_size);
                if (tmp == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", i_size);
                    return;
                }
                memcpy(tmp, i_element, i_size);
                memcpy(i_element, j_element, j_size);
                memcpy(j_element, tmp, i_size);
                free(tmp);
            }
        } else { // j-th element is on heap
            const size_t j_heap_buffer_offset = j_offset - VECTOR_STACK_BUFFER_CAPACITY;
            unsigned char *const j_element = this->heap_buffer + j_heap_buffer_offset;
            const size_t j_type_sz = *(size_t*)j_element;
            const size_t j_size = sizeof(size_t) + j_type_sz;
            if (i_size < j_size) {
                const size_t required_stack_buffer_capacity = i_offset + j_size;
                if (required_stack_buffer_capacity <= VECTOR_STACK_BUFFER_CAPACITY) { // j-th element fits on stack
                    // How much stack elements remains on stack after j-th element replaces i-th element ?
                    size_t fitting_part_size = 0ul;
                    unsigned char *const fitting_part_start = i_element + i_size;
                    size_t k = i;
                    for (; k < this->stack_size; ++k) {
                        const unsigned char *const current = fitting_part_start + fitting_part_size;
                        const size_t current_type_sz = *(size_t*)current;
                        const size_t current_size = sizeof(size_t) + current_type_sz;
                        if (required_stack_buffer_capacity + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                            break;
                        }
                        fitting_part_size += current_size;
                    }
                    // How much stack elements must be moved from stack to heap after j-th element replaces i-th element ?
                    size_t not_fits_after = 0ul;
                    size_t non_fitting_part_size = 0ul;
                    const unsigned char *const non_fitting_part_start = fitting_part_start + fitting_part_size;
                    for (; k < this->stack_size; ++k) {
                        const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
                        const size_t current_type_sz = *(size_t*)current;
                        const size_t current_size = sizeof(size_t) + current_type_sz;
                        non_fitting_part_size += current_size;
                        ++not_fits_after;
                    }
                    // Move non fitting part from stack buffer to heap buffer
                    void *const tmp = malloc(j_size);
                    if (tmp == NULL) {
                        fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", j_size);
                        return;
                    }
                    memcpy(tmp, j_element, j_size);
                    const size_t occupied_size = vector_offset_at(this, this->stack_size + this->heap_size);
                    const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                    const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + i_size - j_size + non_fitting_part_size;
                    if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                        void *const heap_buffer = malloc(heap_buffer_capacity);
                        if (heap_buffer == NULL) {
                            fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                            return;
                        }
                        memcpy(heap_buffer, non_fitting_part_start, non_fitting_part_size);
                        memcpy((unsigned char*)heap_buffer + non_fitting_part_size, this->heap_buffer, j_offset);
                        memcpy((unsigned char*)heap_buffer + non_fitting_part_size + j_offset, i_element, i_size);
                        memcpy((unsigned char*)heap_buffer + non_fitting_part_size + j_offset + i_size, (unsigned char*)this->heap_buffer + j_offset + j_size, heap_buffer_occupied_size - j_offset - j_size);
                        free(this->heap_buffer);
                        this->heap_buffer = heap_buffer;
                        this->heap_buffer_capacity = heap_buffer_capacity;
                    } else { // No heap buffer realloc needed
                        memcpy((unsigned char*)this->heap_buffer + non_fitting_part_size + j_offset + i_size, (unsigned char*)this->heap_buffer + j_offset + j_size, heap_buffer_occupied_size - j_offset - j_size);
                        memcpy((unsigned char*)this->heap_buffer + non_fitting_part_size, this->heap_buffer, j_offset);
                        memcpy((unsigned char*)this->heap_buffer + non_fitting_part_size + j_offset, i_element, i_size);
                        memcpy(this->heap_buffer, non_fitting_part_start, non_fitting_part_size);
                    }
                    memcpy(fitting_part_start + i_size, fitting_part_start, fitting_part_size);
                    memcpy(fitting_part_start, tmp, j_size);
                    free(tmp);
                    this->stack_size -= not_fits_after;
                    this->heap_size += not_fits_after;
                } else { // j-th element doesn't fit on stack
                    // How much stack elements must be moved from stack to heap after j-th element replaces i-th element ?
                    size_t not_fits_after = 0ul;
                    size_t non_fitting_part_size = 0ul;
                    const unsigned char *const non_fitting_part_start = i_element;
                    for (size_t k = i; k < this->stack_size; ++k) {
                        const unsigned char *const current = non_fitting_part_start + non_fitting_part_size;
                        const size_t current_type_sz = *(size_t*)current;
                        const size_t current_size = sizeof(size_t) + current_type_sz;
                        non_fitting_part_size += current_size;
                        ++not_fits_after;
                    }
                    // Move non fitting part from stack buffer to heap buffer
                    void *const tmp = malloc(j_size);
                    if (tmp == NULL) {
                        fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", j_size);
                        return;
                    }
                    memcpy(tmp, j_element, j_size);
                    const size_t occupied_size = vector_offset_at(this, this->stack_size + this->heap_size);
                    const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                    const size_t required_heap_buffer_capacity = heap_buffer_occupied_size + i_size - j_size + non_fitting_part_size;
                    if (required_heap_buffer_capacity > this->heap_buffer_capacity) { // Heap buffer realloc needed
                        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                        void *const heap_buffer = malloc(heap_buffer_capacity);
                        if (heap_buffer == NULL) {
                            fprintf(stderr, "malloc NULL return in vector_insert for capacity %lu\n", heap_buffer_capacity);
                            return;
                        }
                        memcpy(heap_buffer, tmp, j_size);
                        memcpy((unsigned char*)heap_buffer + j_size, non_fitting_part_start, non_fitting_part_size);
                        memcpy((unsigned char*)heap_buffer + j_size + non_fitting_part_size, this->heap_buffer, j_offset);
                        memcpy((unsigned char*)heap_buffer + j_size + non_fitting_part_size + j_offset, i_element, i_size);
                        memcpy((unsigned char*)heap_buffer + j_size + non_fitting_part_size + j_offset + i_size, (unsigned char*)this->heap_buffer + j_offset + j_size, heap_buffer_occupied_size - j_offset - j_size);
                        free(this->heap_buffer);
                        this->heap_buffer = heap_buffer;
                        this->heap_buffer_capacity = heap_buffer_capacity;
                    } else { // No heap buffer realloc needed
                        memcpy((unsigned char*)this->heap_buffer + j_size + non_fitting_part_size + j_offset + i_size, (unsigned char*)this->heap_buffer + j_offset + j_size, heap_buffer_occupied_size - j_offset - j_size);
                        memcpy((unsigned char*)this->heap_buffer + j_size + non_fitting_part_size, this->heap_buffer, j_offset);
                        memcpy((unsigned char*)this->heap_buffer + j_size + non_fitting_part_size + j_offset, i_element, i_size);
                        memcpy((unsigned char*)this->heap_buffer + j_size, non_fitting_part_start, non_fitting_part_size);
                        memcpy(this->heap_buffer, tmp, j_size);
                    }
                    free(tmp);
                    this->stack_size -= 1ul + not_fits_after;
                    this->heap_size += 1ul + not_fits_after;
                }
            } else if (i_size > j_size) {
                // How much elements to move from heap to stack after j-th element replaces i-th element ?
                const size_t stack_buffer_occupied_size = vector_offset_at(this, this->stack_size);
                const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - stack_buffer_occupied_size + i_size - j_size;
                size_t fits_after = 0ul;
                size_t fitting_part_size = 0ul;
                for (size_t k = 0ul; k < j; ++k) {
                    const unsigned char *const current = this->heap_buffer + fitting_part_size;
                    const size_t current_type_sz = *(size_t*)current;
                    const size_t current_size = sizeof(size_t) + current_type_sz;
                    if (stack_buffer_free_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                        break;
                    }
                    fitting_part_size += current_size;
                    ++fits_after;
                }
                // Move fitting part from heap buffer to stack buffer
                void *const tmp = malloc(i_size);
                if (tmp == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", i_size);
                    return;
                }
                memcpy(tmp, i_element, i_size);
                const size_t occupied_size = vector_offset_at(this, this->stack_size + this->heap_size);
                const size_t heap_buffer_occupied_size = occupied_size - VECTOR_STACK_BUFFER_CAPACITY;
                memcpy(i_element + j_size, i_element + i_size, stack_buffer_occupied_size - i_offset - i_size);
                memcpy(i_element, j_element, j_size);
                memcpy(i_element + j_size + stack_buffer_occupied_size - i_offset - i_size, this->heap_buffer, fitting_part_size);
                // Moving elements on heap
                memcpy(this->heap_buffer, (unsigned char*)this->heap_buffer + fitting_part_size, j_heap_buffer_offset - fitting_part_size);
                memcpy((unsigned char*)this->heap_buffer + j_heap_buffer_offset - fitting_part_size, tmp, i_size);
                memcpy((unsigned char*)this->heap_buffer + j_heap_buffer_offset - fitting_part_size + i_size, j_element + j_size, heap_buffer_occupied_size - j_heap_buffer_offset - j_size);
                free(tmp);
                this->stack_size += fitting_part_size;
                this->heap_size -= fitting_part_size;
                // Realloc heap buffer if needed
                const size_t required_heap_buffer_capacity = heap_buffer_occupied_size - fitting_part_size + i_size - j_size;
                if (required_heap_buffer_capacity + 1ul < this->heap_buffer_capacity >> 2) {
                    const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                    void *const heap_buffer = malloc(heap_buffer_capacity);
                    if (heap_buffer == NULL) {
                        fprintf(stderr, "malloc NULL return in vector_remove for capacity %lu\n", heap_buffer_capacity);
                    } else {
                        memcpy(heap_buffer, this->heap_buffer, required_heap_buffer_capacity);
                        this->heap_buffer = heap_buffer;
                        this->heap_buffer_capacity = heap_buffer_capacity;
                    }
                }
            } else { // i_size == j_size
                void *const tmp = malloc(i_size);
                if (tmp == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", i_size);
                    return;
                }
                memcpy(tmp, i_element, i_size);
                memcpy(i_element, j_element, j_size);
                memcpy(j_element, tmp, i_size);
                free(tmp);
            }
        }
    } else { // i-th and j-th elements are on heap
        const size_t i_heap_buffer_offset = i_offset - VECTOR_STACK_BUFFER_CAPACITY;
        unsigned char *const i_element = this->heap_buffer + i_heap_buffer_offset;
        const size_t i_type_sz = *(size_t*)i_element;
        const size_t i_size = sizeof(size_t) + i_type_sz;
        const size_t j_heap_buffer_offset = j_offset - VECTOR_STACK_BUFFER_CAPACITY;
        unsigned char *const j_element = this->heap_buffer + j_heap_buffer_offset;
        const size_t j_type_sz = *(size_t*)j_element;
        const size_t j_size = sizeof(size_t) + j_type_sz;
        if (i_size < j_size) {
            void *const tmp = malloc(j_size);
            if (tmp == NULL) {
                fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", j_size);
                return;
            }
            memcpy(tmp, j_element, j_size);
            memcpy(j_element + j_size - i_size, i_element, i_size);
            memcpy(i_element + j_size, i_element + i_size, j_element - i_element - i_size);
            memcpy(i_element, tmp, j_size);
            free(tmp);
        } else if (i_size > j_size) {
            if (i_heap_buffer_offset == 0ul) { // i-th element is the first on heap
                const size_t stack_buffer_occupied_size = vector_offset_at(this, this->stack_size);
                const size_t stack_buffer_free_size = VECTOR_STACK_BUFFER_CAPACITY - stack_buffer_occupied_size;
                if (j_size <= stack_buffer_free_size) { // Moving some elements from heap buffer to stack buffer
                    size_t fits_after = 1ul;
                    size_t fitting_part_size = 0ul;
                    const unsigned char *const fitting_part_start = i_element + i_size;
                    size_t k = i + 1ul;
                    for (; k < j; ++k) {
                        const unsigned char *const current = fitting_part_start + fitting_part_size;
                        const size_t current_type_sz = *(size_t*)current;
                        const size_t current_size = sizeof(size_t) + current_type_sz;
                        if (stack_buffer_free_size + fitting_part_size + current_size > VECTOR_STACK_BUFFER_CAPACITY) {
                            break;
                        }
                        fitting_part_size += current_size;
                        ++fits_after;
                    }
                    size_t remains_after = 0ul;
                    size_t remaining_part_size = 0ul;
                    const unsigned char *const remaining_part_start = fitting_part_start + fitting_part_size;
                    for (; k < j; ++k) {
                        const unsigned char *const current = remaining_part_start + remaining_part_size;
                        const size_t current_type_sz = *(size_t*)current;
                        const size_t current_size = sizeof(size_t) + current_type_sz;
                        remaining_part_size += current_size;
                        ++remains_after;
                    }
                    // Moving j-th element and fitting part from heap buffer to stack buffer
                    memcpy(this->stack_buffer + stack_buffer_occupied_size, j_element, j_size);
                    memcpy(this->stack_buffer + stack_buffer_occupied_size + j_size, fitting_part_start, fitting_part_size);
                    // Moving i-th element and remaining part within heap buffer
                    void *const tmp = malloc(i_size);
                    if (tmp == NULL) {
                        fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", i_size);
                        return;
                    }
                    memcpy(tmp, i_element, i_size);
                    memcpy(this->heap_buffer, remaining_part_start, remaining_part_size);
                    memcpy((unsigned char*)this->heap_buffer + remaining_part_size, tmp, i_size);
                    free(tmp);
                }
            } else { // i-th element is the non-first on heap
                void *const tmp = malloc(i_size);
                if (tmp == NULL) {
                    fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", i_size);
                    return;
                }
                memcpy(tmp, i_element, i_size);
                memcpy(i_element, j_element, j_size);
                memcpy(i_element + j_size, i_element + i_size, j_element - i_element - i_size);
                memcpy(j_element + j_size - i_size, tmp, i_size);
                free(tmp);
            }
        } else { // i_size == j_size
            void *const tmp = malloc(i_size);
            if (tmp == NULL) {
                fprintf(stderr, "malloc NULL return in vector_swap for size %lu\n", i_size);
                return;
            }
            memcpy(tmp, i_element, i_size);
            memcpy(i_element, j_element, j_size);
            memcpy(j_element, tmp, i_size);
            free(tmp);
        }
    }
}

void vector_reverse(vector *const this) {
    if (this == NULL) {
        return;
    }
    const size_t size = vector_size(this);
    if (size < 2ul) {
        return;
    }
    size_t i = 0ul;
    size_t j = size - 1ul;
    while (i < j) {
        vector_swap(this, i, j);
        ++i;
        --j;
    }
}

void vector_delete(vector *const this) {
    free(this->heap_buffer);
    free(this);
}

void vector_print(
    const vector *const this,
    const print_t print_data
) {
    printf("[");
    const size_t size = vector_size(this);
    if (size > 0ul) {
        unsigned char not_first = 1;
        const unsigned char *current = this->stack_buffer;
        for (size_t i = 0ul; i < this->stack_size; ++i) {
            const size_t type_sz = *(size_t*)current;
            current = current + sizeof(size_t);
            if (not_first) {
                not_first = 0;
            } else {
                printf(", ");
            }
            print_data(current);
            current += type_sz;
        }
        current = (unsigned char*)this->heap_buffer;
        for (size_t i = 0ul; i < this->heap_size; ++i) {
            const size_t type_sz = *(size_t*)current;
            current = current + sizeof(size_t);
            if (not_first) {
                not_first = 0;
            } else {
                printf(", ");
            }
            print_data(current);
            current += type_sz;
        }
    }
    printf("]\n");
}
