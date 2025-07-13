#include "bit_set.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define BIT_SET_STACK_CAPACITY 2ul

struct bit_set {
    unsigned char stack_buffer[BIT_SET_STACK_CAPACITY]; // Small Object Optimization
    unsigned char *heap_buffer;
    size_t heap_buffer_capacity;
    size_t size;
};

[[nodiscard]] static size_t bits_to_chunks(const size_t bits) {
    return bits / CHAR_BIT + !!(bits % CHAR_BIT);
}

[[nodiscard]] bit_set_t *bit_set_init(const size_t capacity) {
    bit_set_t *const bs = malloc(sizeof(bit_set_t));
    if (bs == NULL) {
        fprintf(stderr, "malloc NULL return in bit_set_init");
        return bs;
    }
    bs->heap_buffer = NULL;
    bs->heap_buffer_capacity = 0ul;
    bs->size = 0ul;
    const size_t chunks_capacity = bits_to_chunks(capacity);
    if (chunks_capacity > BIT_SET_STACK_CAPACITY) {
        const size_t heap_buffer_capacity = chunks_capacity - BIT_SET_STACK_CAPACITY;
        bs->heap_buffer = malloc(heap_buffer_capacity);
        if (bs->heap_buffer == NULL) {
            bs->heap_buffer_capacity = 0ul;
            fprintf(stderr, "malloc NULL return in bit_set_init for heap_buffer_capacity %lu\n", heap_buffer_capacity);
            return bs;
        }
        bs->heap_buffer_capacity = heap_buffer_capacity;
    }
    return bs;
}

[[nodiscard]] size_t bit_set_size(const bit_set_t *const this) {
    if (this == NULL) {
        return 0ul;
    }
    return this->size;
}

void bit_set_insert(
    bit_set_t *const this,
    const long long index,
    bit_t b
) {
    if (this == NULL) {
        return;
    }
    const size_t abs_index = index < 0 ? index % (long long)this->size : index;
    if (abs_index > this->size) {
        return;
    }
    if (b > ONE) {
        b = ONE;
    }
    // heap reallocation
    size_t heap_buffer_capacity = this->heap_buffer_capacity;
    unsigned char *heap_buffer = this->heap_buffer;
    const size_t required_capacity = bits_to_chunks(this->size + 1ul);
    if (required_capacity > BIT_SET_STACK_CAPACITY + this->heap_buffer_capacity) {
        heap_buffer_capacity = (required_capacity - BIT_SET_STACK_CAPACITY + 1ul) << 1;
        heap_buffer = realloc(this->heap_buffer, heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in bit_set_insert for heap_buffer_capacity %lu\n", heap_buffer_capacity);
            return;
        }
    }
    // insertion
    const size_t insertion_chunk_index = abs_index / CHAR_BIT;
    const unsigned char insertion_bit_index = abs_index % CHAR_BIT;
    if (insertion_chunk_index >= BIT_SET_STACK_CAPACITY) { // heap insertion
        const size_t insertion_offset = insertion_chunk_index - BIT_SET_STACK_CAPACITY;
        unsigned char *insertion_chunk = heap_buffer + insertion_offset;
        if (abs_index == this->size) {
            if (b == ZERO) {
                *insertion_chunk = *insertion_chunk & ~(1u << insertion_bit_index);
            } else {
                *insertion_chunk = *insertion_chunk | (1u << insertion_bit_index);
            }
        } else {
            // heap shifting
            size_t chunk_offset = insertion_offset;
            bit_t old_bit;
            bit_t new_bit = b;
            if (insertion_bit_index > 0) {
                for (unsigned char i = insertion_bit_index; i < CHAR_BIT; ++i) {
                    old_bit = (*insertion_chunk >> i) & 1u;
                    if (new_bit == ZERO) {
                        *insertion_chunk = *insertion_chunk & ~(1u << i);
                    } else {
                        *insertion_chunk = *insertion_chunk | (1u << i);
                    }
                    new_bit = old_bit;
                }
                ++chunk_offset;
            }
            const size_t occupied_heap_buffer_capacity = bits_to_chunks(this->size - BIT_SET_STACK_CAPACITY * CHAR_BIT);
            for (size_t i = chunk_offset; i < occupied_heap_buffer_capacity; ++i) {
                unsigned char *current_chunk = heap_buffer + i;
                old_bit = (*current_chunk >> (CHAR_BIT - 1)) & 1u;
                *current_chunk = *current_chunk << 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~1u;
                } else {
                    *current_chunk = *current_chunk | 1u;
                }
                new_bit = old_bit;
            }
            const unsigned char last_chunk_unoccupied = !(this->size % CHAR_BIT);
            if (last_chunk_unoccupied) {
                unsigned char *last_chunk = heap_buffer + occupied_heap_buffer_capacity;
                if (new_bit == ZERO) {
                    *last_chunk = *last_chunk & ~1u;
                } else {
                    *last_chunk = *last_chunk | 1u;
                }
            }
        }
    } else { // stack insertion
        unsigned char *insertion_chunk = this->stack_buffer + insertion_chunk_index;
        if (abs_index == this->size) {
            if (b == ZERO) {
                *insertion_chunk = *insertion_chunk & ~(1u << insertion_bit_index);
            } else {
                *insertion_chunk = *insertion_chunk | (1u << insertion_bit_index);
            }
        } else {
            // stack shifting
            size_t chunk_offset = insertion_chunk_index;
            bit_t old_bit;
            bit_t new_bit = b;
            if (insertion_bit_index > 0) {
                for (unsigned char i = insertion_bit_index; i < CHAR_BIT; ++i) {
                    old_bit = (*insertion_chunk >> i) & 1u;
                    if (new_bit == ZERO) {
                        *insertion_chunk = *insertion_chunk & ~(1u << i);
                    } else {
                        *insertion_chunk = *insertion_chunk | (1u << i);
                    }
                    new_bit = old_bit;
                }
                ++chunk_offset;
            }
            for (size_t i = chunk_offset; i < BIT_SET_STACK_CAPACITY; ++i) {
                unsigned char *current_chunk = this->stack_buffer + i;
                old_bit = (*current_chunk >> (CHAR_BIT - 1)) & 1u;
                *current_chunk = *current_chunk << 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~1u;
                } else {
                    *current_chunk = *current_chunk | 1u;
                }
                new_bit = old_bit;
            }
            // heap shifting
            if (required_capacity > BIT_SET_STACK_CAPACITY) {
                const size_t occupied_heap_buffer_capacity = bits_to_chunks(this->size - BIT_SET_STACK_CAPACITY * CHAR_BIT);
                for (size_t i = 0; i < occupied_heap_buffer_capacity; ++i) {
                    unsigned char *current_chunk = heap_buffer + i;
                    old_bit = (*current_chunk >> (CHAR_BIT - 1)) & 1u;
                    *current_chunk = *current_chunk << 1;
                    if (new_bit == ZERO) {
                        *current_chunk = *current_chunk & ~1u;
                    } else {
                        *current_chunk = *current_chunk | 1u;
                    }
                    new_bit = old_bit;
                }
                const unsigned char last_chunk_unoccupied = !(this->size % CHAR_BIT);
                if (last_chunk_unoccupied) {
                    unsigned char *last_chunk = heap_buffer + occupied_heap_buffer_capacity;
                    if (new_bit == ZERO) {
                        *last_chunk = *last_chunk & ~1u;
                    } else {
                        *last_chunk = *last_chunk | 1u;
                    }
                }
            }
        }
    }
    this->heap_buffer = heap_buffer;
    this->heap_buffer_capacity = heap_buffer_capacity;
    ++this->size;
}

void bit_set_push_back(
    bit_set_t *const this,
    bit_t b
) {
    if (this == NULL) {
        return;
    }
    if (b > ONE) {
        b = ONE;
    }
    // heap reallocation
    size_t heap_buffer_capacity = this->heap_buffer_capacity;
    unsigned char *heap_buffer = this->heap_buffer;
    const size_t required_capacity = bits_to_chunks(this->size + 1ul);
    if (required_capacity > BIT_SET_STACK_CAPACITY + this->heap_buffer_capacity) {
        heap_buffer_capacity = (required_capacity - BIT_SET_STACK_CAPACITY + 1ul) << 1;
        heap_buffer = realloc(this->heap_buffer, heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in bit_set_push_back for heap_buffer_capacity %lu\n", heap_buffer_capacity);
            return;
        }
    }
    // insertion
    const size_t insertion_chunk_index = this->size / CHAR_BIT;
    const size_t bit_index = this->size % CHAR_BIT;
    unsigned char *insertion_chunk;
    if (insertion_chunk_index >= BIT_SET_STACK_CAPACITY) {
        insertion_chunk = heap_buffer + insertion_chunk_index - BIT_SET_STACK_CAPACITY;
    } else {
        insertion_chunk = this->stack_buffer + insertion_chunk_index;
    }
    if (b == ZERO) {
        *insertion_chunk = *insertion_chunk & ~(1u << bit_index);
    } else {
        *insertion_chunk = *insertion_chunk | (1u << bit_index);
    }
    this->heap_buffer = heap_buffer;
    this->heap_buffer_capacity = heap_buffer_capacity;
    ++this->size;
}


void bit_set_push_front(
    bit_set_t *const this,
    bit_t b
) {
    if (this == NULL) {
        return;
    }
    if (b > ONE) {
        b = ONE;
    }
    // heap reallocation
    size_t heap_buffer_capacity = this->heap_buffer_capacity;
    unsigned char *heap_buffer = this->heap_buffer;
    const size_t required_capacity = bits_to_chunks(this->size + 1ul);
    if (required_capacity > BIT_SET_STACK_CAPACITY + this->heap_buffer_capacity) {
        heap_buffer_capacity = (required_capacity - BIT_SET_STACK_CAPACITY + 1ul) << 1;
        heap_buffer = realloc(this->heap_buffer, heap_buffer_capacity);
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in bit_set_push_front for heap_buffer_capacity %lu\n", heap_buffer_capacity);
            return;
        }
    }
    // insertion
    if (BIT_SET_STACK_CAPACITY == 0ul) {
        unsigned char *insertion_chunk = heap_buffer;
        if (this->size == 0ul) {
            if (b == ZERO) {
                *insertion_chunk = *insertion_chunk & ~1u;
            } else {
                *insertion_chunk = *insertion_chunk | 1u;
            }
        } else {
            // heap shifting
            bit_t old_bit;
            bit_t new_bit = b;
            const size_t occupied_heap_buffer_capacity = bits_to_chunks(this->size - BIT_SET_STACK_CAPACITY * CHAR_BIT);
            for (size_t i = 0ul; i < occupied_heap_buffer_capacity; ++i) {
                unsigned char *current_chunk = heap_buffer + i;
                old_bit = (*current_chunk >> (CHAR_BIT - 1)) & 1u;
                *current_chunk = *current_chunk << 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~1u;
                } else {
                    *current_chunk = *current_chunk | 1u;
                }
                new_bit = old_bit;
            }
            const unsigned char last_chunk_unoccupied = !(this->size % CHAR_BIT);
            if (last_chunk_unoccupied) {
                unsigned char *last_chunk = heap_buffer + occupied_heap_buffer_capacity;
                if (new_bit == ZERO) {
                    *last_chunk = *last_chunk & ~1u;
                } else {
                    *last_chunk = *last_chunk | 1u;
                }
            }
        }
    } else {
        unsigned char *insertion_chunk = this->stack_buffer;
        if (this->size == 0) {
            if (b == ZERO) {
                *insertion_chunk = *insertion_chunk & ~1u;
            } else {
                *insertion_chunk = *insertion_chunk | 1u;
            }
        } else {
            // stack shifting
            bit_t old_bit;
            bit_t new_bit = b;
            for (size_t i = 0; i < BIT_SET_STACK_CAPACITY; ++i) {
                unsigned char *current_chunk = this->stack_buffer + i;
                old_bit = (*current_chunk >> (CHAR_BIT - 1)) & 1u;
                *current_chunk = *current_chunk << 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~1u;
                } else {
                    *current_chunk = *current_chunk | 1u;
                }
                new_bit = old_bit;
            }
            // heap shifting
            if (required_capacity > BIT_SET_STACK_CAPACITY) {
                const size_t occupied_heap_buffer_capacity = bits_to_chunks(this->size - BIT_SET_STACK_CAPACITY * CHAR_BIT);
                for (size_t i = 0; i < occupied_heap_buffer_capacity; ++i) {
                    unsigned char *current_chunk = heap_buffer + i;
                    old_bit = (*current_chunk >> (CHAR_BIT - 1)) & 1u;
                    *current_chunk = *current_chunk << 1;
                    if (new_bit == ZERO) {
                        *current_chunk = *current_chunk & ~1u;
                    } else {
                        *current_chunk = *current_chunk | 1u;
                    }
                    new_bit = old_bit;
                }
                const unsigned char last_chunk_unoccupied = !(this->size % CHAR_BIT);
                if (last_chunk_unoccupied) {
                    unsigned char *last_chunk = heap_buffer + occupied_heap_buffer_capacity;
                    if (new_bit == ZERO) {
                        *last_chunk = *last_chunk & ~1u;
                    } else {
                        *last_chunk = *last_chunk | 1u;
                    }
                }
            }
        }
    }
    this->heap_buffer = heap_buffer;
    this->heap_buffer_capacity = heap_buffer_capacity;
    ++this->size;
}

void bit_set_remove(
    bit_set_t *const this,
    const long long index
) {
    if (this == NULL || !this->size) {
        return;
    }
    const size_t abs_index = index < 0 ? index % (long long)this->size : index;
    if (abs_index >= this->size) {
        return;
    }
    // removal
    if (abs_index < this->size - 1ul) {
        const size_t removal_chunk_index = abs_index / CHAR_BIT;
        const unsigned char removal_bit_index = abs_index % CHAR_BIT;
        const size_t occupied_heap_buffer_capacity = bits_to_chunks(this->size - BIT_SET_STACK_CAPACITY * CHAR_BIT);
        bit_t old_bit;
        bit_t new_bit = ZERO;
        if (removal_chunk_index >= BIT_SET_STACK_CAPACITY) { // heap removal
            const size_t heap_removal_chunk_index = removal_chunk_index - BIT_SET_STACK_CAPACITY;
            // heap shifting
            if (removal_bit_index == 0) {
                for (long long i = (long long)occupied_heap_buffer_capacity - 1ll; i >= (long long)heap_removal_chunk_index; --i) {
                    unsigned char *current_chunk = this->heap_buffer + i;
                    old_bit = *current_chunk & 1u;
                    *current_chunk = *current_chunk >> 1;
                    if (new_bit == ZERO) {
                        *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                    } else {
                        *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                    }
                    new_bit = old_bit;
                }
            } else {
                for (long long i = (long long)occupied_heap_buffer_capacity - 1ll; i > (long long)heap_removal_chunk_index; --i) {
                    unsigned char *current_chunk = this->heap_buffer + i;
                    old_bit = *current_chunk & 1u;
                    *current_chunk = *current_chunk >> 1;
                    if (new_bit == ZERO) {
                        *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                    } else {
                        *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                    }
                    new_bit = old_bit;
                }
                unsigned char *removal_chunk = this->heap_buffer + heap_removal_chunk_index;
                for (short i = CHAR_BIT - 1; i >= (short)removal_bit_index; --i) {
                    old_bit = (*removal_chunk >> i) & 1u;
                    if (new_bit == ZERO) {
                        *removal_chunk = *removal_chunk & ~(1u << i);
                    } else {
                        *removal_chunk = *removal_chunk | (1u << i);
                    }
                    new_bit = old_bit;
                }
            }
        } else { // stack removal
            // heap shifting
            for (long long i = (long long)occupied_heap_buffer_capacity - 1ll; i >= 0ll; --i) {
                unsigned char *current_chunk = this->heap_buffer + i;
                old_bit = *current_chunk & 1u;
                *current_chunk = *current_chunk >> 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                } else {
                    *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                }
                new_bit = old_bit;
            }
            // stack shifting
            if (removal_bit_index == 0) {
                for (long long i = (long long)BIT_SET_STACK_CAPACITY - 1ll; i >= (long long)removal_chunk_index; --i) {
                    unsigned char *current_chunk = this->stack_buffer + i;
                    old_bit = *current_chunk & 1u;
                    *current_chunk = *current_chunk >> 1;
                    if (new_bit == ZERO) {
                        *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                    } else {
                        *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                    }
                    new_bit = old_bit;
                }
            } else {
                for (long long i = (long long)BIT_SET_STACK_CAPACITY - 1ll; i > (long long)removal_chunk_index; --i) {
                    unsigned char *current_chunk = this->stack_buffer + i;
                    old_bit = *current_chunk & 1u;
                    *current_chunk = *current_chunk >> 1;
                    if (new_bit == ZERO) {
                        *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                    } else {
                        *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                    }
                    new_bit = old_bit;
                }
                unsigned char *removal_chunk = this->stack_buffer + removal_chunk_index;
                for (short i = CHAR_BIT - 1; i >= (short)removal_bit_index; --i) {
                    old_bit = (*removal_chunk >> i) & 1u;
                    if (new_bit == ZERO) {
                        *removal_chunk = *removal_chunk & ~(1u << i);
                    } else {
                        *removal_chunk = *removal_chunk | (1u << i);
                    }
                    new_bit = old_bit;
                }
            }
        }
    }
    // heap reallocation
    const size_t required_capacity = bits_to_chunks(this->size - 1ul);
    if (required_capacity + 1ul < (BIT_SET_STACK_CAPACITY + this->heap_buffer_capacity) >> 2) {
        if (required_capacity + 1ul <= BIT_SET_STACK_CAPACITY) {
            free(this->heap_buffer);
            this->heap_buffer = NULL;
            this->heap_buffer_capacity = 0;
        } else {
            const size_t heap_buffer_capacity = (required_capacity - BIT_SET_STACK_CAPACITY + 1ul) << 1;
            unsigned char *const heap_buffer = realloc(this->heap_buffer, heap_buffer_capacity);
            if (heap_buffer == NULL) {
                --this->size;
                fprintf(stderr, "malloc NULL return in bit_set_remove for heap_buffer_capacity %lu\n", heap_buffer_capacity);
                return;
            }
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
    --this->size;
}

void bit_set_pop_back(bit_set_t *const this) {
    if (this == NULL || !this->size) {
        return;
    }
    // heap reallocation
    const size_t required_capacity = bits_to_chunks(this->size - 1ul);
    if (required_capacity + 1ul < (BIT_SET_STACK_CAPACITY + this->heap_buffer_capacity) >> 2) {
        if (required_capacity + 1ul <= BIT_SET_STACK_CAPACITY) {
            free(this->heap_buffer);
            this->heap_buffer = NULL;
            this->heap_buffer_capacity = 0;
        } else {
            const size_t heap_buffer_capacity = (required_capacity - BIT_SET_STACK_CAPACITY + 1ul) << 1;
            unsigned char *const heap_buffer = realloc(this->heap_buffer, heap_buffer_capacity);
            if (heap_buffer == NULL) {
                --this->size;
                fprintf(stderr, "malloc NULL return in bit_set_pop_back for heap_buffer_capacity %lu\n", heap_buffer_capacity);
                return;
            }
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
    --this->size;
}

void bit_set_pop_front(bit_set_t *const this) {
    if (this == NULL || !this->size) {
        return;
    }
    // removal
    if (this->size > 1ul) {
        const size_t occupied_heap_buffer_capacity = bits_to_chunks(this->size - BIT_SET_STACK_CAPACITY * CHAR_BIT);
        bit_t old_bit;
        bit_t new_bit = ZERO;
        if (BIT_SET_STACK_CAPACITY == 0ul) { // heap removal
            // heap shifting
            for (long long i = (long long)occupied_heap_buffer_capacity - 1ll; i >= 0ll; --i) {
                unsigned char *current_chunk = this->heap_buffer + i;
                old_bit = *current_chunk & 1u;
                *current_chunk = *current_chunk >> 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                } else {
                    *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                }
                new_bit = old_bit;
            }
        } else { // stack removal
            // heap shifting
            for (long long i = (long long)occupied_heap_buffer_capacity - 1ll; i >= 0ll; --i) {
                unsigned char *current_chunk = this->heap_buffer + i;
                old_bit = *current_chunk & 1u;
                *current_chunk = *current_chunk >> 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                } else {
                    *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                }
                new_bit = old_bit;
            }
            // stack shifting
            for (long long i = (long long)BIT_SET_STACK_CAPACITY - 1ll; i >= 0ll; --i) {
                unsigned char *current_chunk = this->stack_buffer + i;
                old_bit = *current_chunk & 1u;
                *current_chunk = *current_chunk >> 1;
                if (new_bit == ZERO) {
                    *current_chunk = *current_chunk & ~(1u << (CHAR_BIT - 1));
                } else {
                    *current_chunk = *current_chunk | (1u << (CHAR_BIT - 1));
                }
                new_bit = old_bit;
            }
        }
    }
    // heap reallocation
    const size_t required_capacity = bits_to_chunks(this->size - 1ul);
    if (required_capacity + 1ul < (BIT_SET_STACK_CAPACITY + this->heap_buffer_capacity) >> 2) {
        if (required_capacity + 1ul <= BIT_SET_STACK_CAPACITY) {
            free(this->heap_buffer);
            this->heap_buffer = NULL;
            this->heap_buffer_capacity = 0;
        } else {
            const size_t heap_buffer_capacity = (required_capacity - BIT_SET_STACK_CAPACITY + 1ul) << 1;
            unsigned char *const heap_buffer = realloc(this->heap_buffer, heap_buffer_capacity);
            if (heap_buffer == NULL) {
                --this->size;
                fprintf(stderr, "malloc NULL return in bit_set_pop_front for heap_buffer_capacity %lu\n", heap_buffer_capacity);
                return;
            }
            this->heap_buffer = heap_buffer;
            this->heap_buffer_capacity = heap_buffer_capacity;
        }
    }
    --this->size;
}

void bit_set_set(
    bit_set_t *const this,
    const long long index,
    bit_t b
) {
    if (this == NULL) {
        return;
    }
    const size_t abs_index = index < 0 ? index % (long long)this->size : index;
    if (abs_index >= this->size) {
        return;
    }
    if (b > ONE) {
        b = ONE;
    }
    const size_t chunk_index = abs_index / CHAR_BIT;
    const size_t bit_index = abs_index % CHAR_BIT;
    unsigned char *chunk;
    if (chunk_index < BIT_SET_STACK_CAPACITY) {
        chunk = this->stack_buffer + chunk_index;
    } else {
        chunk = this->heap_buffer + chunk_index - BIT_SET_STACK_CAPACITY;
    }
    if (b == ZERO) {
        *chunk = *chunk & ~(1u << bit_index);
    } else {
        *chunk = *chunk | (1u << bit_index);
    }
}

[[nodiscard]] bit_t bit_set_get(
    const bit_set_t *const this,
    const long long index
) {
    if (this == NULL) {
        return ZERO;
    }
    const size_t abs_index = index < 0 ? index % (long long)this->size : index;
    if (abs_index > this->size) {
        return ZERO;
    }
    const size_t chunk_index = abs_index / CHAR_BIT;
    const size_t bit_index = abs_index % CHAR_BIT;
    const unsigned char *chunk;
    if (chunk_index < BIT_SET_STACK_CAPACITY) {
        chunk = this->stack_buffer + chunk_index;
    } else {
        chunk = this->heap_buffer + chunk_index - BIT_SET_STACK_CAPACITY;
    }
    return (*chunk >> bit_index) & 1u;
}

static void bit_set_bit_reverse(unsigned char *chunk) {
    unsigned char i = 0;
    unsigned char j = CHAR_BIT - 1;
    while (i < j) {
        const bit_t bit_i = (*chunk >> i) & 1u;
        const bit_t bit_j = (*chunk >> j) & 1u;
        if (bit_j == ZERO) {
            *chunk = *chunk & ~(1u << i);
        } else {
            *chunk = *chunk | (1u << i);
        }
        if (bit_i == ZERO) {
            *chunk = *chunk & ~(1u << j);
        } else {
            *chunk = *chunk | (1u << j);
        }
        ++i;
        --j;
    }
}

void bit_set_reverse(bit_set_t *const this) {
    if (this == NULL || this->size < 2ul) {
        return;
    }
    // shifting
    const unsigned char last_chunk_occupied_size = this->size % CHAR_BIT;
    if (last_chunk_occupied_size) {
        const unsigned char last_chunk_free_size = CHAR_BIT - last_chunk_occupied_size;
        unsigned char old_chunk;
        unsigned char new_chunk = 0;
        // stack shifting
        for (size_t i = 0; i < BIT_SET_STACK_CAPACITY; ++i) {
            unsigned char *current_chunk = this->stack_buffer + i;
            old_chunk = *current_chunk;
            *current_chunk = *current_chunk << last_chunk_free_size;
            for (unsigned char j = last_chunk_occupied_size; j < CHAR_BIT; ++j) {
                const bit_t current_bit = (new_chunk >> j) & 1u;
                if (current_bit == ZERO) {
                    *current_chunk = *current_chunk & ~(1u << (j - last_chunk_occupied_size));
                } else {
                    *current_chunk = *current_chunk | (1u << (j - last_chunk_occupied_size));
                }
            }
            new_chunk = old_chunk;
        }
        // heap_shifting
        const size_t occupied_heap_buffer_capacity = bits_to_chunks(this->size - BIT_SET_STACK_CAPACITY * CHAR_BIT);
        for (size_t i = 0; i < occupied_heap_buffer_capacity; ++i) {
            unsigned char *current_chunk = this->heap_buffer + i;
            old_chunk = *current_chunk;
            *current_chunk = *current_chunk << last_chunk_free_size;
            for (unsigned char j = last_chunk_occupied_size; j < CHAR_BIT; ++j) {
                const bit_t current_bit = (new_chunk >> j) & 1u;
                if (current_bit == ZERO) {
                    *current_chunk = *current_chunk & ~(1u << (j - last_chunk_occupied_size));
                } else {
                    *current_chunk = *current_chunk | (1u << (j - last_chunk_occupied_size));
                }
            }
            new_chunk = old_chunk;
        }
    }
    // chunks and bits reverse
    const size_t chunk_size = bits_to_chunks(this->size);
    size_t i = 0;
    size_t j = chunk_size - 1ul;
    while (i < j) {
        if (i < BIT_SET_STACK_CAPACITY) {
            unsigned char chunk_i = this->stack_buffer[i];
            bit_set_bit_reverse(&chunk_i);
            if (j < BIT_SET_STACK_CAPACITY) {
                unsigned char chunk_j = this->stack_buffer[j];
                bit_set_bit_reverse(&chunk_j);
                this->stack_buffer[i] = chunk_j;
                this->stack_buffer[j] = chunk_i;
            } else {
                const size_t j_heap = j - BIT_SET_STACK_CAPACITY;
                unsigned char chunk_j = this->heap_buffer[j_heap];
                bit_set_bit_reverse(&chunk_j);
                this->stack_buffer[i] = chunk_j;
                this->heap_buffer[j_heap] = chunk_i;
            }
        } else {
            const size_t i_heap = i - BIT_SET_STACK_CAPACITY;
            const size_t j_heap = j - BIT_SET_STACK_CAPACITY;
            unsigned char chunk_i = this->heap_buffer[i_heap];
            bit_set_bit_reverse(&chunk_i);
            unsigned char chunk_j = this->heap_buffer[j_heap];
            bit_set_bit_reverse(&chunk_j);
            this->heap_buffer[i_heap] = chunk_j;
            this->heap_buffer[j_heap] = chunk_i;
        }
        ++i;
        --j;
    }
    // middle chunk reverse, if odd chunk size
    if (chunk_size % 2) {
        const size_t m = chunk_size >> 1;
        if (m < BIT_SET_STACK_CAPACITY) {
            bit_set_bit_reverse(this->stack_buffer + m);
        } else {
            bit_set_bit_reverse(this->heap_buffer + m - BIT_SET_STACK_CAPACITY);
        }
    }
}

void bit_set_delete(bit_set_t *const this) {
    if (this != NULL) {
        free(this->heap_buffer);
        free(this);
    }
}

void bit_set_print(const bit_set_t *const this) {
    printf("[");
    if (this != NULL && this->size) {
        for (size_t i = 0; i < this->size - 1ul; ++i) {
            const size_t current_chunk_index = i / CHAR_BIT;
            const unsigned char *current_chunk;
            if (current_chunk_index < BIT_SET_STACK_CAPACITY) {
                current_chunk = this->stack_buffer + current_chunk_index;
            } else {
                current_chunk = this->heap_buffer + current_chunk_index - BIT_SET_STACK_CAPACITY;
            }
            const size_t bit_index = i % CHAR_BIT;
            printf("%u, ", (*current_chunk >> bit_index) & 1u);
        }
        const size_t last_chunk_index = (this->size - 1ul) / CHAR_BIT;
        const unsigned char *last_chunk;
        if (last_chunk_index < BIT_SET_STACK_CAPACITY) {
            last_chunk = this->stack_buffer + last_chunk_index;
        } else {
            last_chunk = this->heap_buffer + last_chunk_index - BIT_SET_STACK_CAPACITY;
        }
        const size_t bit_index = (this->size - 1ul) % CHAR_BIT;
        printf("%u", (*last_chunk >> bit_index) & 1u);
    }
    printf("]\n");
}
