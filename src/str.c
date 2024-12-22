#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "str.h"

#define STRING_STACK_BUFFER_CAPACITY 8ul
#define RK_M 256ul
#define RK_SHIFT (sizeof(size_t) * CHAR_BIT - 8ul) // mod 2^{8} = 256

struct string_control_block_t {
    char *heap_buffer;
    size_t heap_buffer_capacity;
    size_t ref_counter;
};

[[nodiscard]] static string_control_block *string_control_block_init(const size_t heap_buffer_capacity) {
    string_control_block *control_block = malloc(sizeof(string_control_block));
    if (control_block == NULL) {
        fprintf(stderr, "malloc NULL return in string_control_block_init for size %lu\n", sizeof(string_control_block));
        return NULL;
    }
    control_block->ref_counter = 1ul;
    control_block->heap_buffer = NULL;
    control_block->heap_buffer_capacity = 0ul;
    char *heap_buffer = malloc(heap_buffer_capacity);
    if (heap_buffer == NULL) {
        fprintf(stderr, "malloc NULL return in string_init for capacity %lu\n", heap_buffer_capacity);
        return NULL;
    }
    control_block->heap_buffer = heap_buffer;
    control_block->heap_buffer_capacity = heap_buffer_capacity;
    return control_block;
}

[[nodiscard]] static string_control_block *string_control_block_copy(string_control_block *control_block) {
    string_control_block *copy = malloc(sizeof(string_control_block));
    if (copy == NULL) {
        fprintf(stderr, "malloc NULL return in string_control_block_copy for size %lu\n", sizeof(string_control_block));
        return NULL;
    }
    --control_block->ref_counter;
    copy->ref_counter = 1ul;
    copy->heap_buffer = NULL;
    copy->heap_buffer_capacity = 0ul;
    if (control_block->heap_buffer_capacity > 0ul) {
        char *heap_buffer = malloc(control_block->heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in string_control_block_copy for capacity %lu\n", control_block->heap_buffer_capacity);
            return copy;
        }
        memcpy(heap_buffer, control_block->heap_buffer, control_block->heap_buffer_capacity);
        copy->heap_buffer = heap_buffer;
        copy->heap_buffer_capacity = control_block->heap_buffer_capacity;
    }
    return copy;
}

[[nodiscard]] string string_init(const char *init_str) {
    const size_t size = strlen(init_str);
    string this = {
        .size = size,
        .control_block = NULL
    };
    if (size <= STRING_STACK_BUFFER_CAPACITY) { // no heap required
        memcpy(this.stack_buffer, init_str, size);
    } else { // heap allocation required
        // control block initialization
        const size_t remaining_size = size - STRING_STACK_BUFFER_CAPACITY;
        string_control_block *control_block = string_control_block_init(remaining_size);
        if (control_block == NULL) {
            fprintf(stderr, "malloc NULL return in string_init for capacity %lu\n", remaining_size);
            this.size = 0ul;
            return this;
        }
        this.control_block = control_block;
        // filling up stack buffer
        memcpy(this.stack_buffer, init_str, STRING_STACK_BUFFER_CAPACITY);
        // copying remaining part to heap buffer
        memcpy(control_block->heap_buffer, init_str + STRING_STACK_BUFFER_CAPACITY, remaining_size);
    }
    return this;
}

[[nodiscard]] size_t string_length(const string *this) {
    if (this == NULL) {
        return 0ul;
    }
    return this->size;
}

void string_insert(
    string *const this,
    const size_t index,
    const string *const other
) {
    // incorrect arguments
    if (this == NULL || other == NULL || index > this->size || other->size == 0ul) {
        return;
    }
    // lazy copy
    if (this->control_block != NULL && this->control_block->ref_counter > 1ul) {
        this->control_block = string_control_block_copy(this->control_block);
    }
    // calculating buffers occupied size
    size_t this_stack_buffer_occupied_size;
    size_t this_heap_buffer_occupied_size;
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        this_stack_buffer_occupied_size = STRING_STACK_BUFFER_CAPACITY;
        this_heap_buffer_occupied_size = this->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        this_stack_buffer_occupied_size = this->size;
        this_heap_buffer_occupied_size = 0ul;
    }
    size_t other_stack_buffer_occupied_size;
    size_t other_heap_buffer_occupied_size;
    if (other->size > STRING_STACK_BUFFER_CAPACITY) {
        other_stack_buffer_occupied_size = STRING_STACK_BUFFER_CAPACITY;
        other_heap_buffer_occupied_size = other->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        other_stack_buffer_occupied_size = other->size;
        other_heap_buffer_occupied_size = 0ul;
    }
    // stack buffer insertion
    if (index < STRING_STACK_BUFFER_CAPACITY) {
        // chars to remain on stack after insertion
        const size_t required_stack_buffer_capacity = index + other->size;
        // part of chars after insertion point remains on stack after insertion
        if (required_stack_buffer_capacity <= STRING_STACK_BUFFER_CAPACITY) {
            const size_t stack_buffer_fitting_size = STRING_STACK_BUFFER_CAPACITY - required_stack_buffer_capacity;
            const size_t stack_buffer_non_fitting_size = this_stack_buffer_occupied_size - index - stack_buffer_fitting_size;
            // check if heap buffer realloc is required
            const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + stack_buffer_non_fitting_size;
            if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
                this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
            }
            if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                // reassigning heap buffer
                this->control_block->heap_buffer = heap_buffer;
                this->control_block->heap_buffer_capacity = heap_buffer_capacity;
            }
            // shifting part on heap
            memcpy(this->control_block->heap_buffer + stack_buffer_non_fitting_size, this->control_block->heap_buffer, this_heap_buffer_occupied_size);
            // moving non-fitting part to heap
            memcpy(this->control_block->heap_buffer, this->stack_buffer + index + stack_buffer_fitting_size, stack_buffer_non_fitting_size);
            // shifting fitting part
            memcpy(this->stack_buffer + index + other->size, this->stack_buffer + index, stack_buffer_fitting_size);
            // inserting other string
            memcpy(this->stack_buffer + index, other->stack_buffer, other->size);
        }
        // none of chars after insertion point remain on stack after insertion
        else {
            const size_t insertion_non_fitting_size = required_stack_buffer_capacity - STRING_STACK_BUFFER_CAPACITY;
            const size_t insertion_fitting_size = other->size - insertion_non_fitting_size;
            const size_t stack_buffer_non_fitting_size = this_stack_buffer_occupied_size - index;
            // check if heap buffer realloc is required
            const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + stack_buffer_non_fitting_size + insertion_non_fitting_size;
            if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
                this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
            }
            if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                // reassigning heap buffer
                this->control_block->heap_buffer = heap_buffer;
                this->control_block->heap_buffer_capacity = heap_buffer_capacity;
            }
            // shifting part on heap
            memcpy(this->control_block->heap_buffer + stack_buffer_non_fitting_size + insertion_non_fitting_size, this->control_block->heap_buffer, this_heap_buffer_occupied_size);
            // moving non-fitting part to heap
            memcpy(this->control_block->heap_buffer + insertion_non_fitting_size, this->stack_buffer + index, stack_buffer_non_fitting_size);
            // inserting non fitting part into heap
            const size_t insertion_non_fitting_stack_size = other_stack_buffer_occupied_size - insertion_fitting_size;
            const size_t insertion_non_fitting_heap_size = other_heap_buffer_occupied_size;
            memcpy(this->control_block->heap_buffer, other->stack_buffer + insertion_fitting_size, insertion_non_fitting_stack_size);
            if (other->control_block != NULL) {
                memcpy(this->control_block->heap_buffer + insertion_non_fitting_stack_size, other->control_block->heap_buffer, insertion_non_fitting_heap_size);
            }
            // inserting other string
            memcpy(this->stack_buffer + index, other->stack_buffer, insertion_fitting_size);
        }
    }
    // heap buffer insertion
    else {
        const size_t heap_index = index - STRING_STACK_BUFFER_CAPACITY;
        // check if heap buffer realloc is required
        const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + other->size;
        if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
            this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
        }
        if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
            const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
            char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
            if (heap_buffer == NULL) {
                fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                return;
            }
            // reassigning heap buffer
            this->control_block->heap_buffer = heap_buffer;
            this->control_block->heap_buffer_capacity = heap_buffer_capacity;
        }
        // shifting part on heap
        memcpy(this->control_block->heap_buffer + heap_index + other->size, this->control_block->heap_buffer + heap_index, this_heap_buffer_occupied_size - heap_index);
        // inserting other string
        memcpy(this->control_block->heap_buffer + heap_index, other->stack_buffer, other_stack_buffer_occupied_size);
        if (other->control_block != NULL) {
            memcpy(this->control_block->heap_buffer + heap_index + other_stack_buffer_occupied_size, other->control_block->heap_buffer, other_heap_buffer_occupied_size);
        }
    }
    this->size += other->size;
}

void string_rconcat(
    string *const this,
    const string *const other
) {
    // incorrect arguments
    if (this == NULL || other == NULL || other->size == 0ul) {
        return;
    }
    // lazy copy
    if (this->control_block != NULL && this->control_block->ref_counter > 1ul) {
        this->control_block = string_control_block_copy(this->control_block);
    }
    // calculating buffers occupied size
    size_t this_heap_buffer_occupied_size;
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        this_heap_buffer_occupied_size = this->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        this_heap_buffer_occupied_size = 0ul;
    }
    size_t other_stack_buffer_occupied_size;
    size_t other_heap_buffer_occupied_size;
    if (other->size > STRING_STACK_BUFFER_CAPACITY) {
        other_stack_buffer_occupied_size = STRING_STACK_BUFFER_CAPACITY;
        other_heap_buffer_occupied_size = other->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        other_stack_buffer_occupied_size = other->size;
        other_heap_buffer_occupied_size = 0ul;
    }
    // stack buffer insertion
    if (this->size < STRING_STACK_BUFFER_CAPACITY) {
        const size_t required_stack_buffer_capacity = this->size + other->size;
        const size_t insertion_non_fitting_size = required_stack_buffer_capacity - STRING_STACK_BUFFER_CAPACITY;
        const size_t insertion_fitting_size = other->size - insertion_non_fitting_size;
        // check if heap buffer realloc is required
        const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + insertion_non_fitting_size;
        if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
            this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
        }
        if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
            const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
            char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
            if (heap_buffer == NULL) {
                fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                return;
            }
            // reassigning heap buffer
            this->control_block->heap_buffer = heap_buffer;
            this->control_block->heap_buffer_capacity = heap_buffer_capacity;
        }
        // inserting non fitting part into heap
        const size_t insertion_non_fitting_stack_size = other_stack_buffer_occupied_size - insertion_fitting_size;
        const size_t insertion_non_fitting_heap_size = other_heap_buffer_occupied_size;
        memcpy(this->control_block->heap_buffer, other->stack_buffer + insertion_fitting_size, insertion_non_fitting_stack_size);
        if (other->control_block != NULL) {
            memcpy(this->control_block->heap_buffer + insertion_non_fitting_stack_size, other->control_block->heap_buffer, insertion_non_fitting_heap_size);
        }
        // inserting other string
        memcpy(this->stack_buffer + this->size, other->stack_buffer, insertion_fitting_size);
    }
    // heap buffer insertion
    else {
        const size_t heap_index = this->size - STRING_STACK_BUFFER_CAPACITY;
        // check if heap buffer realloc is required
        const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + other->size;
        if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
            this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
        }
        if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
            const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
            char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
            if (heap_buffer == NULL) {
                fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                return;
            }
            // reassigning heap buffer
            this->control_block->heap_buffer = heap_buffer;
            this->control_block->heap_buffer_capacity = heap_buffer_capacity;
        }
        // inserting other string
        memcpy(this->control_block->heap_buffer + heap_index, other->stack_buffer, other_stack_buffer_occupied_size);
        if (other->control_block != NULL) {
            memcpy(this->control_block->heap_buffer + heap_index + other_stack_buffer_occupied_size, other->control_block->heap_buffer, other_heap_buffer_occupied_size);
        }
    }
    this->size += other->size;
}

void string_lconcat(
    string *const this,
    const string *const other
) {
    // incorrect arguments
    if (this == NULL || other == NULL || other->size == 0ul) {
        return;
    }
    // lazy copy
    if (this->control_block != NULL && this->control_block->ref_counter > 1ul) {
        this->control_block = string_control_block_copy(this->control_block);
    }
    // calculating buffers occupied size
    size_t this_stack_buffer_occupied_size;
    size_t this_heap_buffer_occupied_size;
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        this_stack_buffer_occupied_size = STRING_STACK_BUFFER_CAPACITY;
        this_heap_buffer_occupied_size = this->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        this_stack_buffer_occupied_size = this->size;
        this_heap_buffer_occupied_size = 0ul;
    }
    size_t other_stack_buffer_occupied_size;
    size_t other_heap_buffer_occupied_size;
    if (other->size > STRING_STACK_BUFFER_CAPACITY) {
        other_stack_buffer_occupied_size = STRING_STACK_BUFFER_CAPACITY;
        other_heap_buffer_occupied_size = other->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        other_stack_buffer_occupied_size = other->size;
        other_heap_buffer_occupied_size = 0ul;
    }
    // stack buffer insertion
    #if STRING_STACK_BUFFER_CAPACITY > 0ul
        // chars to remain on stack after insertion
        // part of chars after insertion point remains on stack after insertion
        if (other->size <= STRING_STACK_BUFFER_CAPACITY) {
            const size_t stack_buffer_fitting_size = STRING_STACK_BUFFER_CAPACITY - other->size;
            const size_t stack_buffer_non_fitting_size = this_stack_buffer_occupied_size - stack_buffer_fitting_size;
            // check if heap buffer realloc is required
            const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + stack_buffer_non_fitting_size;
            if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
                this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
            }
            if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                // reassigning heap buffer
                this->control_block->heap_buffer = heap_buffer;
                this->control_block->heap_buffer_capacity = heap_buffer_capacity;
            }
            // shifting part on heap
            memcpy(this->control_block->heap_buffer + stack_buffer_non_fitting_size, this->control_block->heap_buffer, this_heap_buffer_occupied_size);
            // moving non-fitting part to heap
            memcpy(this->control_block->heap_buffer, this->stack_buffer + stack_buffer_fitting_size, stack_buffer_non_fitting_size);
            // shifting fitting part
            memcpy(this->stack_buffer + other->size, this->stack_buffer, stack_buffer_fitting_size);
            // inserting other string
            memcpy(this->stack_buffer, other->stack_buffer, other->size);
        }
        // none of chars after insertion point remain on stack after insertion
        else {
            const size_t insertion_non_fitting_size = other->size - STRING_STACK_BUFFER_CAPACITY;
            const size_t insertion_fitting_size = other->size - insertion_non_fitting_size;
            const size_t stack_buffer_non_fitting_size = this_stack_buffer_occupied_size;
            // check if heap buffer realloc is required
            const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + stack_buffer_non_fitting_size + insertion_non_fitting_size;
            if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
                this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
            }
            if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
                const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
                char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
                if (heap_buffer == NULL) {
                    fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                    return;
                }
                // reassigning heap buffer
                this->control_block->heap_buffer = heap_buffer;
                this->control_block->heap_buffer_capacity = heap_buffer_capacity;
            }
            // shifting part on heap
            memcpy(this->control_block->heap_buffer + stack_buffer_non_fitting_size + insertion_non_fitting_size, this->control_block->heap_buffer, this_heap_buffer_occupied_size);
            // moving non-fitting part to heap
            memcpy(this->control_block->heap_buffer + insertion_non_fitting_size, this->stack_buffer, stack_buffer_non_fitting_size);
            // inserting non fitting part into heap
            const size_t insertion_non_fitting_stack_size = other_stack_buffer_occupied_size - insertion_fitting_size;
            const size_t insertion_non_fitting_heap_size = other_heap_buffer_occupied_size;
            memcpy(this->control_block->heap_buffer, other->stack_buffer + insertion_fitting_size, insertion_non_fitting_stack_size);
            if (other->control_block != NULL) {
                memcpy(this->control_block->heap_buffer + insertion_non_fitting_stack_size, other->control_block->heap_buffer, insertion_non_fitting_heap_size);
            }
            // inserting other string
            memcpy(this->stack_buffer, other->stack_buffer, insertion_fitting_size);
        }
    #else
        // check if heap buffer realloc is required
        const size_t required_heap_buffer_capacity = this_heap_buffer_occupied_size + other->size;
        if (required_heap_buffer_capacity > 0ul && this->control_block == NULL) {
            this->control_block = string_control_block_init((required_heap_buffer_capacity + 1ul) << 1);
        }
        if (required_heap_buffer_capacity > this->control_block->heap_buffer_capacity) {
            const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
            char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
            if (heap_buffer == NULL) {
                fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
                return;
            }
            // reassigning heap buffer
            this->control_block->heap_buffer = heap_buffer;
            this->control_block->heap_buffer_capacity = heap_buffer_capacity;
        }
        // shifting part on heap
        memcpy(this->control_block->heap_buffer + other->size, this->control_block->heap_buffer, this_heap_buffer_occupied_size);
        // inserting other string
        memcpy(this->control_block->heap_buffer, other->stack_buffer, other_stack_buffer_occupied_size);
        if (other->control_block != NULL) {
            memcpy(this->control_block->heap_buffer + other_stack_buffer_occupied_size, other->control_block->heap_buffer, other_heap_buffer_occupied_size);
        }
    #endif
    this->size += other->size;
}

void string_remove(
    string *const this,
    const size_t index,
    size_t count
) {
    // incorrect arguments
    if (this == NULL || index >= this->size || count == 0ul) {
        return;
    }
    size_t occupied_stack_buffer_size;
    size_t occupied_heap_buffer_size;
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        occupied_stack_buffer_size = STRING_STACK_BUFFER_CAPACITY;
        occupied_heap_buffer_size = this->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        occupied_stack_buffer_size = this->size;
        occupied_heap_buffer_size = 0ul;
    }
    size_t end = index + count;
    if (end > this->size) {
        end = this->size;
        count = end - index;
    }
    // stack buffer removal
    if (index < STRING_STACK_BUFFER_CAPACITY) {
        // stack buffer only removal
        if (end <= STRING_STACK_BUFFER_CAPACITY) {
            const size_t remaining_stack_buffer_size = occupied_stack_buffer_size - end;
            // shifting remaining stack buffer part
            memcpy(this->stack_buffer + index, this->stack_buffer + end, remaining_stack_buffer_size);
            const size_t stack_buffer_free_size = STRING_STACK_BUFFER_CAPACITY - index - remaining_stack_buffer_size;
            if (this->control_block != NULL) {
                // moving part from heap buffer to stack buffer
                memcpy(this->stack_buffer + index + remaining_stack_buffer_size, this->control_block->heap_buffer, stack_buffer_free_size);
                // shifting remaining heap buffer part
                memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + stack_buffer_free_size, occupied_heap_buffer_size - stack_buffer_free_size);
            }
        }
        // stack and heap buffer removal
        else {
            const size_t heap_end = end - STRING_STACK_BUFFER_CAPACITY;
            const size_t stack_buffer_free_size = STRING_STACK_BUFFER_CAPACITY - index;
            if (this->control_block != NULL) {
                // moving part from heap buffer to stack buffer
                memcpy(this->stack_buffer + index, this->control_block->heap_buffer + heap_end, stack_buffer_free_size);
                // lazy copy
                if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block->ref_counter > 1ul) {
                    this->control_block = string_control_block_copy(this->control_block);
                }
                // shifting remaining heap buffer part
                memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + heap_end + stack_buffer_free_size, occupied_heap_buffer_size - stack_buffer_free_size - heap_end);
            }
        }
    }
    // heap buffer removal
    else {
        const size_t heap_index = index - STRING_STACK_BUFFER_CAPACITY;
        const size_t heap_end = end - STRING_STACK_BUFFER_CAPACITY;
        if (this->control_block != NULL) {
            // lazy copy
            if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block->ref_counter > 1ul) {
                this->control_block = string_control_block_copy(this->control_block);
            }
            // shifting heap buffer part
            memcpy(this->control_block->heap_buffer + heap_index, this->control_block->heap_buffer + heap_end, this->size - end);
        }
    }
    // check if heap buffer realloc is required
    const size_t required_heap_buffer_capacity = this->size > count + STRING_STACK_BUFFER_CAPACITY ? this->size - count - STRING_STACK_BUFFER_CAPACITY : 0ul;
    // releasing control block
    if (required_heap_buffer_capacity == 0ul && this->control_block != NULL) {
        if (--this->control_block->ref_counter == 0ul) {
            free(this->control_block->heap_buffer);
            free(this->control_block);
        }
        this->control_block = NULL;
    }
    // heap buffer realloc
    else if (required_heap_buffer_capacity + 1ul < this->control_block->heap_buffer_capacity >> 2) {
        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
        char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
        } else {
            this->control_block->heap_buffer = heap_buffer;
            this->control_block->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
    this->size -= count;
}

void string_rtrim(
    string *const this,
    size_t count
) {
    // incorrect arguments
    if (this == NULL || count == 0ul) {
        return;
    }
    // lazy copy
    if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
        this->control_block = string_control_block_copy(this->control_block);
    }
    if (this->size < count) {
        count = this->size;
    }
    // check if heap buffer realloc is required
    const size_t required_heap_buffer_capacity = this->size > count + STRING_STACK_BUFFER_CAPACITY ? this->size - count - STRING_STACK_BUFFER_CAPACITY : 0ul;
    // releasing control block
    if (required_heap_buffer_capacity == 0ul && this->control_block != NULL) {
        if (--this->control_block->ref_counter == 0ul) {
            free(this->control_block->heap_buffer);
            free(this->control_block);
        }
        this->control_block = NULL;
    }
    // heap buffer realloc
    else if (required_heap_buffer_capacity + 1ul < this->control_block->heap_buffer_capacity >> 2) {
        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
        char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
        } else {
            this->control_block->heap_buffer = heap_buffer;
            this->control_block->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
    this->size -= count;
}

void string_ltrim(
    string *const this,
    size_t count
) {
    // incorrect arguments
    if (this == NULL || count == 0ul) {
        return;
    }
    // lazy copy
    if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
        this->control_block = string_control_block_copy(this->control_block);
    }
    size_t occupied_stack_buffer_size;
    size_t occupied_heap_buffer_size;
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        occupied_stack_buffer_size = STRING_STACK_BUFFER_CAPACITY;
        occupied_heap_buffer_size = this->size - STRING_STACK_BUFFER_CAPACITY;
    } else {
        occupied_stack_buffer_size = this->size;
        occupied_heap_buffer_size = 0ul;
    }
    if (count > this->size) {
        count = this->size;
    }
    // stack buffer removal
    #if STRING_STACK_BUFFER_CAPACITY > 0ul
        // stack buffer only removal
        if (count <= STRING_STACK_BUFFER_CAPACITY) {
            const size_t remaining_stack_buffer_size = occupied_stack_buffer_size - count;
            // shifting remaining stack buffer part
            memcpy(this->stack_buffer, this->stack_buffer + count, remaining_stack_buffer_size);
            const size_t stack_buffer_free_size = STRING_STACK_BUFFER_CAPACITY - remaining_stack_buffer_size;
            if (this->control_block != NULL) {
                // moving part from heap buffer to stack buffer
                memcpy(this->stack_buffer + remaining_stack_buffer_size, this->control_block->heap_buffer, stack_buffer_free_size);
                // shifting remaining heap buffer part
                memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + stack_buffer_free_size, occupied_heap_buffer_size - stack_buffer_free_size);
            }
        }
        // stack and heap buffer removal
        else if (this->control_block != NULL) {
            const size_t heap_end = count - STRING_STACK_BUFFER_CAPACITY;
            // moving part from heap buffer to stack buffer
            memcpy(this->stack_buffer, this->control_block->heap_buffer + heap_end, STRING_STACK_BUFFER_CAPACITY);
            // shifting remaining heap buffer part
            memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + heap_end + STRING_STACK_BUFFER_CAPACITY, occupied_heap_buffer_size - STRING_STACK_BUFFER_CAPACITY - heap_end);
        }
    #else
        if (this->control_block != NULL) {
            // shifting heap buffer part
            memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + count, this->size - count);
        }
    #endif
    // check if heap buffer realloc is required
    const size_t required_heap_buffer_capacity = this->size > count + STRING_STACK_BUFFER_CAPACITY ? this->size - count - STRING_STACK_BUFFER_CAPACITY : 0ul;
    // releasing control block
    if (required_heap_buffer_capacity == 0ul && this->control_block != NULL) {
        if (--this->control_block->ref_counter == 0ul) {
            free(this->control_block->heap_buffer);
            free(this->control_block);
        }
        this->control_block = NULL;
    }
    // heap buffer realloc
    else if (required_heap_buffer_capacity + 1ul < this->control_block->heap_buffer_capacity >> 2) {
        const size_t heap_buffer_capacity = (required_heap_buffer_capacity + 1ul) << 1;
        char *const heap_buffer = realloc(this->control_block->heap_buffer, heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", heap_buffer_capacity);
        } else {
            this->control_block->heap_buffer = heap_buffer;
            this->control_block->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
    this->size -= count;
}

[[nodiscard]] static signed char compare_char(
    const char a,
    const char b,
    const unsigned char case_insensitive
) {
    if (case_insensitive) {
        return (signed char)(tolower(a) - tolower(b));
    }
    return (signed char)(a - b);
}

[[nodiscard]] static int compare_string(
    const char *const a,
    const char *const b,
    const size_t len,
    const unsigned char case_insensitive
) {
    assert(a != NULL && b != NULL);
    // case sensitive comparison
    if (!case_insensitive) {
        return memcmp(a, b, len);
    }
    // case insensitive comparison
    for (size_t i = 0ul; i < len; ++i) {
        const int diff = tolower(a[i]) - tolower(b[i]);
        if (diff != 0ul) {
            return diff;
        }
    }
    return 0;
}

unsigned char string_rtrim_like(
    string *const restrict this,
    const char *const restrict like,
    const unsigned char case_insensitive
) {
    unsigned char not_copied = 1;
    if (this == NULL || like == NULL) {
        return not_copied;
    }
    const size_t like_len = strlen(like);
    if (like_len == 0ul || like_len > this->size) {
        return not_copied;
    }
    long long start = (long long)this->size - (long long)like_len;
    // looking for from in initial string
    while (start >= 0ul) {
        if (start < STRING_STACK_BUFFER_CAPACITY) { // start is on stack
            if (this->size > STRING_STACK_BUFFER_CAPACITY) { // end is on heap
                const size_t stack_part_size = STRING_STACK_BUFFER_CAPACITY - start;
                const size_t heap_part_size = like_len - stack_part_size;
                const int stack_diff = memcmp(this->stack_buffer + start, like, stack_part_size);
                const int heap_diff = memcmp(this->control_block->heap_buffer, like + stack_part_size, heap_part_size);
                if (stack_diff == 0ul && heap_diff == 0ul) {
                    // lazy copy
                    if (not_copied) {
                        if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                            this->control_block = string_control_block_copy(this->control_block);
                        }
                        not_copied = 0;
                    }
                    // this string occupies both stack and heap
                    // releasing control block
                    if (--this->control_block->ref_counter == 0ul) {
                        free(this->control_block->heap_buffer);
                        free(this->control_block);
                    }
                    this->control_block = NULL;
                    start -= (long long)like_len;
                    this->size -= like_len;
                } else {
                    return not_copied;
                }
            } else { // end is on stack
                const int diff = memcmp(this->stack_buffer + start, like, like_len);
                if (diff == 0ul) {
                    // lazy copy
                    if (not_copied) {
                        if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                            this->control_block = string_control_block_copy(this->control_block);
                        }
                        not_copied = 0;
                    }
                    // new end is on stack
                    // new string fits on stack
                    start -= (long long)like_len;
                    this->size -= like_len;
                } else {
                    return not_copied;
                }
            }
        } else { // start and end are on heap
            // new end is on heap
            // this string occupies both stack and heap
            // new string needs heap
            const size_t heap_size = this->size - STRING_STACK_BUFFER_CAPACITY;
            const size_t heap_start = start - STRING_STACK_BUFFER_CAPACITY;
            const int diff = memcmp(this->control_block->heap_buffer + heap_start, like, like_len);
            if (diff == 0ul) {
                // lazy copy
                if (not_copied) {
                    if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                        this->control_block = string_control_block_copy(this->control_block);
                    }
                    not_copied = 0;
                }
                const size_t required = heap_size - like_len;
                // check if heap buffer realloc is required
                if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                    const size_t capacity = (required + 1ul) << 1;
                    char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                    if (heap_buffer == NULL) {
                        fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                    } else {
                        // reassigning heap buffer
                        this->control_block->heap_buffer = heap_buffer;
                        this->control_block->heap_buffer_capacity = capacity;
                    }
                }
                start -= (long long)like_len;
                this->size -= like_len;
            } else {
                return not_copied;
            }
        }
    }
    return not_copied;
}

unsigned char string_ltrim_like(
    string *const restrict this,
    const char *const restrict like,
    const unsigned char case_insensitive
) {
    unsigned char not_copied = 1;
    if (this == NULL || like == NULL) {
        return not_copied;
    }
    const size_t like_len = strlen(like);
    if (like_len == 0ul || like_len > this->size) {
        return not_copied;
    }
    // looking for from in initial string
    while (this->size >= like_len) {
        #if STRING_STACK_BUFFER_CAPACITY > 0ul // start is on stack
            if (like_len > STRING_STACK_BUFFER_CAPACITY) { // end is on heap
                // this string occupies both stack and heap
                const size_t heap_part_size = like_len - STRING_STACK_BUFFER_CAPACITY;
                const int stack_diff = compare_string(this->stack_buffer, like, STRING_STACK_BUFFER_CAPACITY, case_insensitive);
                const int heap_diff = compare_string(this->control_block->heap_buffer, like + STRING_STACK_BUFFER_CAPACITY, heap_part_size, case_insensitive);
                if (stack_diff == 0ul && heap_diff == 0ul) {
                    // lazy copy
                    if (not_copied) {
                        if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                            this->control_block = string_control_block_copy(this->control_block);
                        }
                        not_copied = 0;
                    }
                    //
                    const size_t heap_size = this->size - STRING_STACK_BUFFER_CAPACITY;
                    const size_t heap_end = like_len - STRING_STACK_BUFFER_CAPACITY;
                    const size_t new_size = this->size - like_len;
                    if (new_size <= STRING_STACK_BUFFER_CAPACITY) { // new string fits on stack
                        // moving part from heap to stack
                        memcpy(this->stack_buffer, this->control_block->heap_buffer + heap_end, heap_size - heap_end);
                        // releasing control block
                        if (--this->control_block->ref_counter == 0ul) {
                            free(this->control_block->heap_buffer);
                            free(this->control_block);
                        }
                        this->control_block = NULL;
                    } else { // new string needs heap
                        const size_t required = heap_size - STRING_STACK_BUFFER_CAPACITY;
                        // check if heap buffer realloc is required
                        if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                            const size_t capacity = (required + 1ul) << 1;
                            char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                            if (heap_buffer == NULL) {
                                fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                            } else {
                                // reassigning heap buffer
                                this->control_block->heap_buffer = heap_buffer;
                                this->control_block->heap_buffer_capacity = capacity;
                            }
                        }
                        // moving part from heap to stack
                        memcpy(this->stack_buffer, this->control_block->heap_buffer, STRING_STACK_BUFFER_CAPACITY);
                        // shifting part on heap
                        memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + STRING_STACK_BUFFER_CAPACITY + heap_end, heap_size - heap_end - STRING_STACK_BUFFER_CAPACITY);
                    }
                    this->size -= like_len;
                } else {
                    return not_copied;
                }
            } else { // end is on stack
                const int diff = compare_string(this->stack_buffer, like, like_len, case_insensitive);
                if (diff == 0ul) {
                    // lazy copy
                    if (not_copied) {
                        if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                            this->control_block = string_control_block_copy(this->control_block);
                        }
                        not_copied = 0;
                    }
                    // new end is on stack
                    if (this->size <= STRING_STACK_BUFFER_CAPACITY) { // this string fits on stack
                        // new string fits on stack
                        // shifting part after
                        memcpy(this->stack_buffer, this->stack_buffer + like_len, this->size - like_len);
                    } else { // this string occupies both stack and heap
                        const size_t new_size = this->size - like_len;
                        if (new_size <= STRING_STACK_BUFFER_CAPACITY) { // new string fits on stack
                            const size_t stack_after_size = STRING_STACK_BUFFER_CAPACITY - like_len;
                            // shifting part after
                            memcpy(this->stack_buffer, this->stack_buffer + like_len, stack_after_size);
                            // moving part from heap to stack
                            memcpy(this->stack_buffer + stack_after_size, this->control_block->heap_buffer, this->size - STRING_STACK_BUFFER_CAPACITY);
                            // releasing control block
                            if (--this->control_block->ref_counter == 0ul) {
                                free(this->control_block->heap_buffer);
                                free(this->control_block);
                            }
                            this->control_block = NULL;
                        } else { // new string needs heap
                            const size_t stack_after_size = STRING_STACK_BUFFER_CAPACITY - like_len;
                            const size_t required = this->size - STRING_STACK_BUFFER_CAPACITY - like_len;
                            // shifting part after
                            memcpy(this->stack_buffer, this->stack_buffer + like_len, stack_after_size);
                            // moving part from heap to stack
                            memcpy(this->stack_buffer + stack_after_size, this->control_block->heap_buffer, like_len);
                            // check if heap buffer realloc is required
                            if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                                const size_t capacity = (required + 1ul) << 1;
                                char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                                if (heap_buffer == NULL) {
                                    fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                                } else {
                                    // reassigning heap buffer
                                    this->control_block->heap_buffer = heap_buffer;
                                    this->control_block->heap_buffer_capacity = capacity;
                                }
                            }
                            // shifting part remained on heap
                            memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + like_len, required);
                        }
                    }
                    this->size -= like_len;
                } else {
                    return not_copied;
                }
            }
        #else // start and end are on heap
            // new end is on heap
            // this string occupies both stack and heap
            // new string needs heap
            const int diff = compare_string(this->control_block->heap_buffer, like, like_len, case_insensitive);
            if (diff == 0ul) {
                // lazy copy
                if (not_copied) {
                    if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                        this->control_block = string_control_block_copy(this->control_block);
                    }
                    not_copied = 0;
                }
                //
                const size_t required = this->size - like_len;
                // check if heap buffer realloc is required
                if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                    const size_t capacity = (required + 1ul) << 1;
                    char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                    if (heap_buffer == NULL) {
                        fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                    } else {
                        // reassigning heap buffer
                        this->control_block->heap_buffer = heap_buffer;
                        this->control_block->heap_buffer_capacity = capacity;
                    }
                }
                // shifting part on heap
                memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + like_len, this->size - like_len);
                this->size -= like_len;
            } else {
                return not_copied;
            }
        #endif
    }
    return not_copied;
}

unsigned char string_trim_like(
    string *const restrict this,
    const char *const restrict like,
    const unsigned char case_insensitive
) {
    unsigned char not_copied = 1;
    if (this == NULL || like == NULL) {
        return not_copied;
    }
    const size_t like_len = strlen(like);
    if (like_len == 0ul) {
        return not_copied;
    }
    not_copied = string_ltrim_like(this, like, case_insensitive);
    not_copied = string_rtrim_like(this, like, case_insensitive) && not_copied;
    return not_copied;
}

void string_set(
    string *const this,
    const size_t index,
    const char c
) {
    // incorrect arguments
    if (this == NULL || index >= this->size) {
        return;
    }
    // stack buffer set
    if (index <= STRING_STACK_BUFFER_CAPACITY) {
        this->stack_buffer[index] = c;
    }
    // heap buffer set
    else if (this->control_block != NULL) {
        // lazy copy
        if (this->control_block->ref_counter > 1ul) {
            this->control_block = string_control_block_copy(this->control_block);
        }
        this->control_block->heap_buffer[index - STRING_STACK_BUFFER_CAPACITY] = c;
    }
}

[[nodiscard]] char string_get(
    const string *const this,
    const size_t index
) {
    // incorrect arguments
    if (this == NULL || index >= this->size) {
        return '\0';
    }
    // stack buffer get
    if (index <= STRING_STACK_BUFFER_CAPACITY) {
        return this->stack_buffer[index];
    }
    // heap buffer get
    if (this->control_block != NULL) {
        return this->control_block->heap_buffer[index - STRING_STACK_BUFFER_CAPACITY];
    }
    return '\0';
}

[[nodiscard]] string string_copy(const string *const this) {
    string copy = {
        .control_block = this->control_block,
        .size = this->size,
    };
    #if STRING_STACK_BUFFER_CAPACITY > 0ul
        if (this->size > 0ul) {
            const size_t occupied_stack_buffer_size = this->size < STRING_STACK_BUFFER_CAPACITY ? this->size : STRING_STACK_BUFFER_CAPACITY;
            memcpy(copy.stack_buffer, this->stack_buffer, occupied_stack_buffer_size);
        }
    #endif
    ++this->control_block->ref_counter;
    return copy;
}

[[nodiscard]] string string_substring(
    const string *const this,
    const size_t index,
    size_t count
) {
    string substring = {
        .control_block = NULL,
        .size = 0ul
    };
    if (this == NULL || index >= this->size || count == 0ul) {
        return substring;
    }
    size_t end = index + count;
    if (end > this->size) {
        end = this->size;
        count = end - index;
    }
    if (count <= STRING_STACK_BUFFER_CAPACITY) { // no heap required
        if (end <= STRING_STACK_BUFFER_CAPACITY) { // copying from stack
            memcpy(substring.stack_buffer, this->stack_buffer + index, count);
        } else {
            if (index < STRING_STACK_BUFFER_CAPACITY) { // copying from stack and heap
                // stack part
                memcpy(substring.stack_buffer, this->stack_buffer + index, STRING_STACK_BUFFER_CAPACITY - index);
                // heap part
                memcpy(substring.stack_buffer, this->control_block->heap_buffer, end - STRING_STACK_BUFFER_CAPACITY);
            } else { // copying from heap
                memcpy(substring.stack_buffer, this->control_block->heap_buffer + index, count);
            }
        }
    } else { // heap allocation required
        // control block initialization
        const size_t remaining_size = count - STRING_STACK_BUFFER_CAPACITY;
        string_control_block *control_block = string_control_block_init(remaining_size);
        if (control_block == NULL) {
            substring.size = 0ul;
            return substring;
        }
        substring.control_block = control_block;
        if (index < STRING_STACK_BUFFER_CAPACITY) { // copying from stack and heap
            // stack part
            memcpy(substring.stack_buffer, this->stack_buffer + index, STRING_STACK_BUFFER_CAPACITY - index);
            // heap part fitting stack
            memcpy(substring.stack_buffer, this->control_block->heap_buffer, index);
            // heap part not fitting stack
            memcpy(control_block->heap_buffer, this->control_block->heap_buffer + index, this->size - STRING_STACK_BUFFER_CAPACITY - index);
        } else { // copying from heap
            // heap part fitting stack
            memcpy(substring.stack_buffer, this->control_block->heap_buffer, STRING_STACK_BUFFER_CAPACITY);
            // heap part not fitting stack
            memcpy(control_block->heap_buffer, this->control_block->heap_buffer + STRING_STACK_BUFFER_CAPACITY, this->size - 2 * STRING_STACK_BUFFER_CAPACITY);
        }
    }
    return substring;
}

[[nodiscard]] long long string_lfind(
    const string *const restrict this,
    const char *const restrict str,
    const unsigned char case_insensitive
) {
    if (this == NULL || str == NULL) {
        return -1ll;
    }
    const size_t str_len = strlen(str);
    if (str_len == 0ul || str_len > this->size) {
        return -1ll;
    }
    // calculating str hash
    size_t str_hash = 0ul;
    for (size_t i = 0ul; i < str_len; ++i) {
        // h += c_{i} * 2^{l - i - 1} mod 256
        const int ci = case_insensitive ? tolower(str[i]) : str[i];
        str_hash = (str_hash + (ci << (str_len - i - 1ul))) & (-1ul >> RK_SHIFT);
    }
    // calculating initial string hash
    size_t hash = 0ul;
    for (size_t i = 0ul; i < str_len && i < STRING_STACK_BUFFER_CAPACITY; ++i) {
        const int ci = case_insensitive ? tolower(this->stack_buffer[i]) : this->stack_buffer[i];
        hash = (hash + (ci << (str_len - i - 1ul))) & (-1ul >> RK_SHIFT);
    }
    for (size_t i = STRING_STACK_BUFFER_CAPACITY; i < str_len; ++i) {
        const size_t heap_i = i - STRING_STACK_BUFFER_CAPACITY;
        const int ci = case_insensitive ? tolower(this->control_block->heap_buffer[heap_i]) : this->control_block->heap_buffer[heap_i];
        hash = (hash + (ci << (str_len - i - 1ul))) & (-1ul >> RK_SHIFT);
    }
    // looking for str in initial string
    size_t end = str_len;
    while (end <= this->size) {
        const size_t start = end - str_len;
        // replacing if from found
        if (hash == str_hash) {
            // checking hash collision
            if (start < STRING_STACK_BUFFER_CAPACITY) { // start is on stack
                if (end > STRING_STACK_BUFFER_CAPACITY) { // end is on heap
                    const size_t stack_part_size = STRING_STACK_BUFFER_CAPACITY - start;
                    const size_t heap_part_size = str_len - stack_part_size;
                    const int stack_diff = compare_string(this->stack_buffer + start, str, stack_part_size, case_insensitive);
                    const int heap_diff = compare_string(this->control_block->heap_buffer, str + stack_part_size, heap_part_size, case_insensitive);
                    if (stack_diff == 0ul && heap_diff == 0ul) {
                        return (long long)start;
                    }
                } else { // end is on stack
                    const int diff = compare_string(this->stack_buffer + start, str, str_len, case_insensitive);
                    if (diff == 0ul) {
                        return (long long)start;
                    }
                }
            } else { // start and end are on heap
                const int diff = compare_string(this->control_block->heap_buffer + start - STRING_STACK_BUFFER_CAPACITY, str, str_len, case_insensitive);
                if (diff == 0ul) {
                    return (long long)start;
                }
            }
        }
        // recalculating hash
        char start_char;
        if (start < STRING_STACK_BUFFER_CAPACITY) {
            start_char = this->stack_buffer[start];
        } else {
            start_char = this->control_block->heap_buffer[start - STRING_STACK_BUFFER_CAPACITY];
        }
        char end_char = '\0';
        if (end < this->size) {
            if (end < STRING_STACK_BUFFER_CAPACITY) {
                end_char = this->stack_buffer[end];
            } else {
                end_char = this->control_block->heap_buffer[end - STRING_STACK_BUFFER_CAPACITY];
            }
        }
        if (case_insensitive) {
            start_char = (char)tolower(start_char);
            end_char = (char)tolower(end_char);
        }
        // h_{i + 1} = (h_{i} - c_{i} * 2^{l - 1}) * 2 + c_{i + l} mod 256
        hash = (((hash + (RK_M - (start_char << (str_len - 1ul)) & (-1ul >> RK_SHIFT)) & (-1ul >> RK_SHIFT)) << 1) + end_char) & (-1ul >> RK_SHIFT);
        ++end;
    }
    return -1ll;
}

[[nodiscard]] long long string_rfind(
    const string *this,
    const char* str,
    const unsigned char case_insensitive
) {
    if (this == NULL || str == NULL) {
        return -1ll;
    }
    const size_t str_len = strlen(str);
    if (str_len == 0ul || str_len > this->size) {
        return -1ll;
    }
    // calculating str hash
    size_t str_hash = 0ul;
    for (size_t i = 0ul; i < str_len; ++i) {
        // h += c_{i} * 2^{l - i - 1} mod 256
        const int ci = case_insensitive ? tolower(str[i]) : str[i];
        str_hash = (str_hash + (ci << (str_len - i - 1ul))) & (-1ul >> RK_SHIFT);
    }
    // substring borders
    long long start = (long long)this->size - (long long)str_len;
    // calculating initial string hash
    size_t hash = 0ul;
    for (size_t i = start, j = i - start; j < str_len && i < STRING_STACK_BUFFER_CAPACITY; ++i, ++j) {
        const int ci = case_insensitive ? tolower(this->stack_buffer[i]) : this->stack_buffer[i];
        hash = (hash + (ci << (str_len - j - 1ul))) & (-1ul >> RK_SHIFT);
    }
    for (size_t i = STRING_STACK_BUFFER_CAPACITY, j = i - start; j < str_len; ++i, ++j) {
        const size_t heap_i = i - STRING_STACK_BUFFER_CAPACITY;
        const int ci = case_insensitive ? tolower(this->control_block->heap_buffer[heap_i]) : this->control_block->heap_buffer[heap_i];
        hash = (hash + (ci << (str_len - j - 1ul))) & (-1ul >> RK_SHIFT);
    }
    // looking for str in initial string
    while (start >= 0ll) {
        const size_t end = start + str_len;
        // replacing if from found
        if (hash == str_hash) {
            // checking hash collision
            if (start < STRING_STACK_BUFFER_CAPACITY) { // start is on stack
                if (end > STRING_STACK_BUFFER_CAPACITY) { // end is on heap
                    const size_t stack_part_size = STRING_STACK_BUFFER_CAPACITY - start;
                    const size_t heap_part_size = str_len - stack_part_size;
                    const int stack_diff = compare_string(this->stack_buffer + start, str, stack_part_size, case_insensitive);
                    const int heap_diff = compare_string(this->control_block->heap_buffer, str + stack_part_size, heap_part_size, case_insensitive);
                    if (stack_diff == 0ul && heap_diff == 0ul) {
                        return (long long)start;
                    }
                } else { // end is on stack
                    const int diff = compare_string(this->stack_buffer + start, str, str_len, case_insensitive);
                    if (diff == 0ul) {
                        return (long long)start;
                    }
                }
            } else { // start and end are on heap
                const int diff = compare_string(this->control_block->heap_buffer + start - STRING_STACK_BUFFER_CAPACITY, str, str_len, case_insensitive);
                if (diff == 0ul) {
                    return (long long)start;
                }
            }
        }
        // recalculating hash
        char start_char = '\0';
        if (start > 0ul) {
            if (start < STRING_STACK_BUFFER_CAPACITY) {
                start_char = this->stack_buffer[start - 1];
            } else {
                start_char = this->control_block->heap_buffer[start - 1 - STRING_STACK_BUFFER_CAPACITY];
            }
        }
        char end_char;
        if (end < STRING_STACK_BUFFER_CAPACITY) {
            end_char = this->stack_buffer[end - 1];
        } else {
            end_char = this->control_block->heap_buffer[end - 1 - STRING_STACK_BUFFER_CAPACITY];
        }
        if (case_insensitive) {
            start_char = (char)tolower(start_char);
            end_char = (char)tolower(end_char);
        }
        // h_{i - 1} = (h_{i} - c_{i + l - 1}) / 2 + c_{i - 1} * 2^{l - 1} mod 256
        hash = (((hash  + (RK_M - (end_char & (-1ul >> RK_SHIFT))) & (-1ul >> RK_SHIFT)) >> 1) + (start_char << (str_len - 1ul))) & (-1ul >> RK_SHIFT);
        --start;
    }
    return -1ll;
}

void string_replace(
    string *const restrict this,
    const char *const restrict from,
    const char *const restrict to
) {
    if (this == NULL || from == NULL || to == NULL || strcmp(from, to) == 0) {
        return;
    }
    const size_t from_len = strlen(from);
    if (from_len == 0ul || from_len > this->size) {
        return;
    }
    unsigned char not_copied = 1;
    const size_t to_len = strlen(to);
    const long long len_diff = (long long)to_len - (long long)from_len;
    // looking for from in initial string
    size_t start = 0ul;
    size_t end = from_len;
    while (end <= this->size) {
        const size_t new_end = start + to_len;
        if (start < STRING_STACK_BUFFER_CAPACITY) { // start is on stack
            if (end > STRING_STACK_BUFFER_CAPACITY) { // end is on heap
                const size_t stack_part_size = STRING_STACK_BUFFER_CAPACITY - start;
                const size_t heap_part_size = from_len - stack_part_size;
                const int stack_diff = memcmp(this->stack_buffer + start, from, stack_part_size);
                const int heap_diff = memcmp(this->control_block->heap_buffer, from + stack_part_size, heap_part_size);
                if (stack_diff == 0ul && heap_diff == 0ul) {
                    // lazy copy
                    if (not_copied) {
                        if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                            this->control_block = string_control_block_copy(this->control_block);
                        }
                        not_copied = 0;
                    }
                    //
                    const size_t heap_size = this->size - STRING_STACK_BUFFER_CAPACITY;
                    const size_t heap_end = end - STRING_STACK_BUFFER_CAPACITY;
                    if (from_len < to_len) { // new string will be bigger
                        // new end is on heap
                        // this string occupies both stack and heap
                        // new string needs heap
                        const size_t replacement_stack_size = STRING_STACK_BUFFER_CAPACITY - start;
                        const size_t heap_new_end = new_end - STRING_STACK_BUFFER_CAPACITY;
                        const size_t required = heap_size + len_diff;
                        // heap capacity check
                        if (required > this->control_block->heap_buffer_capacity) { // heap realloc
                            const size_t capacity = (required + 1ul) << 1;
                            char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                            if (heap_buffer == NULL) {
                                fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                                return;
                            }
                            // reassigning heap buffer
                            this->control_block->heap_buffer = heap_buffer;
                            this->control_block->heap_buffer_capacity = capacity;
                        }
                        // shifting part on heap
                        memcpy(this->control_block->heap_buffer + heap_new_end, this->control_block->heap_buffer + heap_end, heap_size - heap_end);
                        // replacing on heap
                        memcpy(this->control_block->heap_buffer, to + replacement_stack_size, to_len - replacement_stack_size);
                        // replacing on stack
                        memcpy(this->stack_buffer + start, to, replacement_stack_size);
                    } else if (from_len > to_len) { // new string will be smaller
                        if (new_end > STRING_STACK_BUFFER_CAPACITY) { // new end is on heap
                            // this string occupies both stack and heap
                            // new string needs heap
                            const size_t heap_new_end = new_end - STRING_STACK_BUFFER_CAPACITY;
                            const size_t replacement_stack_size = STRING_STACK_BUFFER_CAPACITY - start;
                            const size_t required = heap_size + len_diff;
                            // check if heap buffer realloc is required
                            if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                                const size_t capacity = (required + 1ul) << 1;
                                char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                                if (heap_buffer == NULL) {
                                    fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                                } else {
                                    // reassigning heap buffer
                                    this->control_block->heap_buffer = heap_buffer;
                                    this->control_block->heap_buffer_capacity = capacity;
                                }
                            }
                            // shifting part on heap
                            memcpy(this->control_block->heap_buffer + heap_new_end, this->control_block->heap_buffer + heap_end, heap_size - heap_end);
                            // replacing on heap
                            memcpy(this->control_block->heap_buffer, to + replacement_stack_size, to_len - replacement_stack_size);
                            // replacing on stack
                            memcpy(this->stack_buffer + start, to, replacement_stack_size);
                        } else { // new end is on stack
                            // this string occupies both stack and heap
                            const size_t new_size = this->size + len_diff;
                            if (new_size <= STRING_STACK_BUFFER_CAPACITY) { // new string fits on stack
                                // moving part from heap to stack
                                memcpy(this->stack_buffer + new_end, this->control_block->heap_buffer + heap_end, heap_size - heap_end);
                                // replacing
                                memcpy(this->stack_buffer + start, to, to_len);
                                // releasing control block
                                if (--this->control_block->ref_counter == 0ul) {
                                    free(this->control_block->heap_buffer);
                                    free(this->control_block);
                                }
                                this->control_block = NULL;
                            } else { // new string needs heap
                                const size_t fits = STRING_STACK_BUFFER_CAPACITY - new_end;
                                const size_t required = heap_size - fits;
                                // check if heap buffer realloc is required
                                if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                                    const size_t capacity = (required + 1ul) << 1;
                                    char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                                    if (heap_buffer == NULL) {
                                        fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                                    } else {
                                        // reassigning heap buffer
                                        this->control_block->heap_buffer = heap_buffer;
                                        this->control_block->heap_buffer_capacity = capacity;
                                    }
                                }
                                // moving part from heap to stack
                                memcpy(this->stack_buffer + new_end, this->control_block->heap_buffer, fits);
                                // shifting part on heap
                                memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + fits + heap_end, heap_size - heap_end - fits);
                                // replacing
                                memcpy(this->stack_buffer + start, to, to_len);
                            }
                        }
                    } else { // new string will have the same size
                        // new end is on heap
                        // this string occupies both stack and heap
                        // new string needs heap
                        memcpy(this->stack_buffer + start, to, stack_part_size);
                        memcpy(this->control_block->heap_buffer, to + stack_part_size, heap_part_size);
                    }
                    start = new_end;
                    this->size += len_diff;
                } else {
                    ++start;
                }
            } else { // end is on stack
                const int diff = memcmp(this->stack_buffer + start, from, from_len);
                if (diff == 0ul) {
                    // lazy copy
                    if (not_copied) {
                        if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                            this->control_block = string_control_block_copy(this->control_block);
                        }
                        not_copied = 0;
                    }
                    //
                    if (from_len < to_len) { // new string will be bigger
                        if (new_end > STRING_STACK_BUFFER_CAPACITY) { // new end is on heap
                            if (this->size <= STRING_STACK_BUFFER_CAPACITY) { // this string fits on stack
                                // new string needs heap
                                const size_t not_fits = this->size - end;
                                const size_t replacement_heap_size = new_end - STRING_STACK_BUFFER_CAPACITY;
                                const size_t required = not_fits + replacement_heap_size;
                                // heap capacity check
                                if (required > 0ul) { // allocating control block
                                    assert(this->control_block == NULL);
                                    const size_t capacity = (required + 1ul) << 1;
                                    this->control_block = string_control_block_init(capacity);
                                }
                                // moving non-fitting part to heap
                                memcpy(this->control_block->heap_buffer + replacement_heap_size, this->stack_buffer + end, not_fits);
                                // replacing on heap
                                memcpy(this->control_block->heap_buffer, to + to_len - replacement_heap_size, replacement_heap_size);
                                // replacing on stack
                                memcpy(this->stack_buffer + start, to, to_len - replacement_heap_size);
                            } else { // this string occupies both stack and heap
                                // new string needs heap
                                const size_t heap_size = this->size - STRING_STACK_BUFFER_CAPACITY;
                                const size_t not_fits = STRING_STACK_BUFFER_CAPACITY - end;
                                const size_t replacement_heap_size = new_end - STRING_STACK_BUFFER_CAPACITY;
                                const size_t required = heap_size + not_fits + replacement_heap_size;
                                // heap capacity check
                                if (required > this->control_block->heap_buffer_capacity) { // heap realloc
                                    const size_t capacity = (required + 1ul) << 1;
                                    char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                                    if (heap_buffer == NULL) {
                                        fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                                        return;
                                    }
                                    // reassigning heap buffer
                                    this->control_block->heap_buffer = heap_buffer;
                                    this->control_block->heap_buffer_capacity = capacity;
                                }
                                // shifting part on heap
                                memcpy(this->control_block->heap_buffer + replacement_heap_size + not_fits, this->control_block->heap_buffer, heap_size);
                                // moving non-fitting part to heap
                                memcpy(this->control_block->heap_buffer + replacement_heap_size, this->stack_buffer + end, not_fits);
                                // replacing on heap
                                memcpy(this->control_block->heap_buffer, to + to_len - replacement_heap_size, replacement_heap_size);
                                // replacing on stack
                                memcpy(this->stack_buffer + start, to, to_len - replacement_heap_size);
                            }
                        } else { // new end is on stack
                            if (this->size <= STRING_STACK_BUFFER_CAPACITY) { // this string fits on stack
                                const size_t new_size = this->size + len_diff;
                                if (new_size <= STRING_STACK_BUFFER_CAPACITY) { // new string fits on stack
                                    // shifting part after
                                    memcpy(this->stack_buffer + new_end, this->stack_buffer + end, this->size - end);
                                    // replacing
                                    memcpy(this->stack_buffer + start, to, to_len);
                                } else { // new string needs heap
                                    const size_t fits = STRING_STACK_BUFFER_CAPACITY - new_end;
                                    const size_t not_fits = this->size - end - fits;
                                    // heap capacity check
                                    if (not_fits > 0ul) { // allocating control block
                                        assert(this->control_block == NULL);
                                        const size_t capacity = (not_fits + 1ul) << 1;
                                        this->control_block = string_control_block_init(capacity);
                                    }
                                    // moving non-fitting part to heap
                                    memcpy(this->control_block->heap_buffer, this->stack_buffer + end + fits, not_fits);
                                    // shifting fitting part after
                                    memcpy(this->stack_buffer + new_end, this->stack_buffer + end, fits);
                                    // replacing
                                    memcpy(this->stack_buffer + start, to, to_len);
                                }
                            } else { // this string occupies both stack and heap
                                // new string needs heap
                                const size_t fits = STRING_STACK_BUFFER_CAPACITY - new_end;
                                const size_t not_fits = STRING_STACK_BUFFER_CAPACITY - end - fits;
                                const size_t heap_size = this->size - STRING_STACK_BUFFER_CAPACITY;
                                const size_t required = not_fits + heap_size;
                                // heap capacity check
                                if (required > this->control_block->heap_buffer_capacity) { // heap realloc
                                    const size_t capacity = (required + 1ul) << 1;
                                    char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                                    if (heap_buffer == NULL) {
                                        fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                                        return;
                                    }
                                    // reassigning heap buffer
                                    this->control_block->heap_buffer = heap_buffer;
                                    this->control_block->heap_buffer_capacity = capacity;
                                }
                                // shifting part on heap
                                memcpy(this->control_block->heap_buffer + not_fits, this->control_block->heap_buffer, heap_size);
                                // moving non-fitting part to heap
                                memcpy(this->control_block->heap_buffer, this->stack_buffer + end + fits, not_fits);
                                // shifting fitting part after
                                memcpy(this->stack_buffer + new_end, this->stack_buffer + end, fits);
                                // replacing
                                memcpy(this->stack_buffer + start, to, to_len);
                            }
                        }
                    } else if (from_len > to_len) { // new string will be smaller
                        // new end is on stack
                        if (this->size <= STRING_STACK_BUFFER_CAPACITY) { // this string fits on stack
                            // new string fits on stack
                            // shifting part after
                            memcpy(this->stack_buffer + new_end, this->stack_buffer + end, this->size - end);
                            // replacing
                            memcpy(this->stack_buffer + start, to, to_len);
                        } else { // this string occupies both stack and heap
                            const size_t new_size = this->size + len_diff;
                            if (new_size <= STRING_STACK_BUFFER_CAPACITY) { // new string fits on stack
                                const size_t stack_after_size = STRING_STACK_BUFFER_CAPACITY - end;
                                // shifting part after
                                memcpy(this->stack_buffer + new_end, this->stack_buffer + end, stack_after_size);
                                // moving part from heap to stack
                                memcpy(this->stack_buffer + new_end + stack_after_size, this->control_block->heap_buffer, this->size - STRING_STACK_BUFFER_CAPACITY);
                                // replacing
                                memcpy(this->stack_buffer + start, to, to_len);
                                // releasing control block
                                if (--this->control_block->ref_counter == 0ul) {
                                    free(this->control_block->heap_buffer);
                                    free(this->control_block);
                                }
                                this->control_block = NULL;
                            } else { // new string needs heap
                                const size_t stack_after_size = STRING_STACK_BUFFER_CAPACITY - end;
                                const size_t stack_freed_size = from_len - to_len;
                                const size_t required = this->size - STRING_STACK_BUFFER_CAPACITY - stack_freed_size;
                                // shifting part after
                                memcpy(this->stack_buffer + new_end, this->stack_buffer + end, stack_after_size);
                                // moving part from heap to stack
                                memcpy(this->stack_buffer + new_end + stack_after_size, this->control_block->heap_buffer, stack_freed_size);
                                // check if heap buffer realloc is required
                                if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                                    const size_t capacity = (required + 1ul) << 1;
                                    char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                                    if (heap_buffer == NULL) {
                                        fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                                    } else {
                                        // reassigning heap buffer
                                        this->control_block->heap_buffer = heap_buffer;
                                        this->control_block->heap_buffer_capacity = capacity;
                                    }
                                }
                                // shifting part remained on heap
                                memcpy(this->control_block->heap_buffer, this->control_block->heap_buffer + stack_freed_size, required);
                                // replacing
                                memcpy(this->stack_buffer + start, to, to_len);
                            }
                        }
                    } else { // new string will have the same size
                        // new end is on stack
                        // this string fits on stack
                        // new string fits on stack
                        // replacing
                        memcpy(this->stack_buffer + start, to, to_len);
                    }
                    start = new_end;
                    this->size += len_diff;
                } else {
                    ++start;
                }
            }
        } else { // start and end are on heap
            // new end is on heap
            // this string occupies both stack and heap
            // new string needs heap
            const size_t heap_size = this->size - STRING_STACK_BUFFER_CAPACITY;
            const size_t heap_start = start - STRING_STACK_BUFFER_CAPACITY;
            const size_t heap_end = end - STRING_STACK_BUFFER_CAPACITY;
            const size_t heap_new_end = new_end - STRING_STACK_BUFFER_CAPACITY;
            const int diff = memcmp(this->control_block->heap_buffer + heap_start, from, from_len);
            if (diff == 0ul) {
                // lazy copy
                if (not_copied) {
                    if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
                        this->control_block = string_control_block_copy(this->control_block);
                    }
                    not_copied = 0;
                }
                //
                if (from_len < to_len) { // new string will be bigger
                    const size_t required = heap_size + len_diff;
                    // heap capacity check
                    if (required > this->control_block->heap_buffer_capacity) { // heap realloc
                        const size_t capacity = (required + 1ul) << 1;
                        char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                        if (heap_buffer == NULL) {
                            fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                            return;
                        }
                        // reassigning heap buffer
                        this->control_block->heap_buffer = heap_buffer;
                        this->control_block->heap_buffer_capacity = capacity;
                    }
                    // shifting part on heap
                    memcpy(this->control_block->heap_buffer + heap_new_end, this->control_block->heap_buffer + heap_end, heap_size - heap_end);
                    // replacing
                    memcpy(this->control_block->heap_buffer + heap_start, to, to_len);
                } else if (from_len > to_len) { // new string will be smaller
                    const size_t required = heap_size + len_diff;
                    // check if heap buffer realloc is required
                    if (required + 1ul < this->control_block->heap_buffer_capacity >> 2) {
                        const size_t capacity = (required + 1ul) << 1;
                        char *const heap_buffer = realloc(this->control_block->heap_buffer, capacity);
                        if (heap_buffer == NULL) {
                            fprintf(stderr, "realloc NULL return in string_replace for capacity %lu\n", capacity);
                        } else {
                            // reassigning heap buffer
                            this->control_block->heap_buffer = heap_buffer;
                            this->control_block->heap_buffer_capacity = capacity;
                        }
                    }
                    // shifting part on heap
                    memcpy(this->control_block->heap_buffer + heap_new_end, this->control_block->heap_buffer + heap_end, heap_size - heap_end);
                    // replacing
                    memcpy(this->control_block->heap_buffer + heap_start, to, to_len);
                } else { // new string will have the same size
                    // new end is on heap
                    // this string occupies both stack and heap
                    // new string needs heap
                    memcpy(this->control_block->heap_buffer + start, to, to_len);
                }
                start = new_end;
                this->size += len_diff;
            } else {
                ++start;
            }
        }
        end = start + from_len;
    }
}

long long string_compare(
    const string *const this,
    const string *const other,
    const unsigned char case_insensitive
) {
    if (this == NULL) {
        if (other == NULL) {
            return 0ll;
        }
        return -(long long)other->size;
    }
    if (other == NULL) {
        return (long long)this->size;
    }
    const long long size_diff = (long long)this->size - (long long)other->size;
    if (size_diff != 0ll) {
        return size_diff;
    }
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        for (size_t i = 0; i < STRING_STACK_BUFFER_CAPACITY; ++i) {
            const char this_char_i = this->stack_buffer[i];
            const char other_char_i = other->stack_buffer[i];
            const signed char diff = compare_char(this_char_i, other_char_i, case_insensitive);
            if (diff != 0ul) {
                return diff;
            }
        }
        for (size_t i = 0; i < this->size - STRING_STACK_BUFFER_CAPACITY; ++i) {
            const char this_char_i = this->control_block->heap_buffer[i];
            const char other_char_i = other->control_block->heap_buffer[i];
            const signed char diff = compare_char(this_char_i, other_char_i, case_insensitive);
            if (diff != 0ul) {
                return diff;
            }
        }
    } else {
        for (size_t i = 0; i < this->size; ++i) {
            const char this_char_i = this->stack_buffer[i];
            const char other_char_i = other->stack_buffer[i];
            const signed char diff = compare_char(this_char_i, other_char_i, case_insensitive);
            if (diff != 0ul) {
                return diff;
            }
        }
    }
    return 0ll;
}

[[nodiscard]] char *string_cstr(const string *const this) {
    if (this == NULL) {
        return NULL;
    }
    assert(this->control_block != NULL);
    char *cstr = malloc(this->size + 1ul);
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        memcpy(cstr, this->stack_buffer, STRING_STACK_BUFFER_CAPACITY);
        memcpy(cstr + STRING_STACK_BUFFER_CAPACITY, this->control_block->heap_buffer, this->size - STRING_STACK_BUFFER_CAPACITY);
    } else {
        memcpy(cstr, this->stack_buffer, this->size);
    }
    cstr[this->size] = '\0';
    return cstr;
}

static void string_swap(
    string *const this,
    size_t i,
    size_t j
) {
    assert(this != NULL && this->size > 1ul);
    // i must be before j
    if (j < i) {
        const size_t tmp = i;
        i = j;
        j = tmp;
    }
    // swapping
    char *const ci = i < STRING_STACK_BUFFER_CAPACITY ? this->stack_buffer + i : this->control_block->heap_buffer + i - STRING_STACK_BUFFER_CAPACITY;
    char *const cj = j < STRING_STACK_BUFFER_CAPACITY ? this->stack_buffer + j : this->control_block->heap_buffer + j - STRING_STACK_BUFFER_CAPACITY;
    const char tmp = *ci;
    *ci = *cj;
    *cj = tmp;
}

void string_reverse(string *const this) {
    // incorrect arguments
    if (this == NULL || this->size < 2ul) {
        return;
    }
    // lazy copy
    if (this->size > STRING_STACK_BUFFER_CAPACITY && this->control_block != NULL && this->control_block->ref_counter > 1ul) {
        this->control_block = string_control_block_copy(this->control_block);
    }
    // reversing
    size_t i = 0ul;
    size_t j = this->size - 1ul;
    do {
        string_swap(this, i, j);
    } while (++i < --j);
}

void string_delete(string *const this) {
    if (this == NULL) {
        return;
    }
    if (this->control_block != NULL) {
        assert(this->control_block->ref_counter != 0ul);
        if (--this->control_block->ref_counter == 0ul) {
            free(this->control_block->heap_buffer);
            free(this->control_block);
        }
        this->control_block = NULL;
    }
    this->size = 0ul;
}

void string_print(const string *const this) {
    if (this == NULL) {
        return;
    }
    if (this->size == 0ul) {
        return;
    }
    if (this->size > STRING_STACK_BUFFER_CAPACITY) {
        for (size_t i = 0; i < STRING_STACK_BUFFER_CAPACITY; ++i) {
            printf("%c", this->stack_buffer[i]);
        }
        for (size_t i = 0; i < this->size - STRING_STACK_BUFFER_CAPACITY; ++i) {
            printf("%c", this->control_block->heap_buffer[i]);
        }
    } else {
        for (size_t i = 0; i < this->size; ++i) {
            printf("%c", this->stack_buffer[i]);
        }
    }
    printf("\n");
}
