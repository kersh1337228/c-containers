#include "hash_map.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#define LOAD_FACTOR_MAX 0.75l
#define LOAD_FACTOR_MIN 0.25l
#define HASH_MAP_STACK_CAPACITY 1ul
#define HASH_MAP_MIN_CAPACITY (2ul)
#define HASH_MOD(capacity) ((capacity) * 86ul / 100ul)

struct bucket_t {
    void *key;
    node_data data;
    bucket *next; // coalesced hashing
};

struct hash_map_t {
    bucket stack_buffer[HASH_MAP_STACK_CAPACITY]; // Small Object Optimization
    bucket *heap_buffer;
    size_t heap_buffer_capacity;
    hash_t hash_function;
    comparator_t key_comparator;
    size_t key_sz;
};

[[nodiscard]] hash_map *hash_map_init(
    size_t capacity,
    const hash_t hash_function,
    const comparator_t key_comparator,
    const size_t key_sz
) {
    if (hash_function == NULL || key_comparator == NULL || key_sz == 0ul) {
        return NULL;
    }
    hash_map *const hm = malloc(sizeof(hash_map));
    if (hm == NULL) {
        fprintf(stderr, "malloc NULL return in hash_map_init\n");
        return hm;
    }
    hm->hash_function = hash_function;
    hm->key_comparator = key_comparator;
    hm->key_sz = key_sz;
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        bucket *const b = hm->stack_buffer + i;
        b->key = NULL;
        b->data.data = NULL;
        b->data.type_sz = 0ul;
        b->next = NULL;
    }
    bucket *heap_buffer = NULL;
    size_t heap_buffer_capacity = 0ul;
    if (capacity < HASH_MAP_MIN_CAPACITY) {
        capacity = HASH_MAP_MIN_CAPACITY;
    }
    if (capacity > HASH_MAP_STACK_CAPACITY) {
        heap_buffer_capacity = capacity - HASH_MAP_STACK_CAPACITY;
        heap_buffer = calloc(heap_buffer_capacity, sizeof(bucket));
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in hash_map_init for capacity %lu\n", capacity);
            hm->heap_buffer = NULL;
            hm->heap_buffer_capacity = 0ul;
            return hm;
        }
        for (size_t i = 0ul; i < heap_buffer_capacity; ++i) {
            bucket *const b = heap_buffer + i;
            b->key = NULL;
            b->data.data = NULL;
            b->data.type_sz = 0ul;
            b->next = NULL;
        }
    }
    hm->heap_buffer = heap_buffer;
    hm->heap_buffer_capacity = heap_buffer_capacity;
    return hm;
}

[[nodiscard]] size_t hash_map_size(const hash_map *const this) {
    if (this == NULL || this->heap_buffer_capacity == 0ul) {
        return 0;
    }
    size_t size = 0ul;
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        const bucket *const b = this->stack_buffer + i;
        if (b->key != NULL) {
            ++size;
        }
    }
    for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
        const bucket *const b = this->heap_buffer + i;
        if (b->key != NULL) {
            ++size;
        }
    }
    return size;
}

static void hash_map_rehash(
    hash_map *const this,
    const size_t capacity
) {
    assert(this != NULL && capacity != 0ul);
    bucket stack_buffer[HASH_MAP_STACK_CAPACITY];
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        bucket *const b = stack_buffer + i;
        b->key = NULL;
        b->data.data = NULL;
        b->data.type_sz = 0;
        b->next = NULL;
    }
    if (capacity > HASH_MAP_STACK_CAPACITY) { // both stack and heap reallocation required
        const size_t heap_buffer_capacity = capacity - HASH_MAP_STACK_CAPACITY;
        bucket *const heap_buffer = calloc(heap_buffer_capacity, sizeof(bucket));
        for (size_t i = 0ul; i < heap_buffer_capacity; ++i) {
            bucket *const b = heap_buffer + i;
            b->key = NULL;
            b->data.data = NULL;
            b->data.type_sz = 0;
            b->next = NULL;
        }
        for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
            const bucket *const b = this->stack_buffer + i;
            if (b->key != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), this->key_sz, b->key);
                bucket *prev_bucket;
                if (key_hash < HASH_MAP_STACK_CAPACITY) {
                    prev_bucket = stack_buffer + key_hash;
                } else {
                    prev_bucket = heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
                }
                if (prev_bucket->key == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket *new_bucket = heap_buffer + heap_buffer_capacity - 1ul;
                size_t heap_buffer_decrements = 0ul;
                while (new_bucket->key != NULL && ++heap_buffer_decrements < heap_buffer_capacity) {
                    --new_bucket;
                }
                if (new_bucket->key != NULL) {
                    new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                    while (new_bucket->key != NULL) {
                        --new_bucket;
                    }
                }
                new_bucket->key = b->key;
                new_bucket->data = b->data;
                new_bucket->next = NULL;
                prev_bucket->next = new_bucket;
            }
        }
        for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
            const bucket *const b = this->heap_buffer + i;
            if (b->key != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), this->key_sz, b->key);
                bucket *prev_bucket;
                if (key_hash < HASH_MAP_STACK_CAPACITY) {
                    prev_bucket = stack_buffer + key_hash;
                } else {
                    prev_bucket = heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
                }
                if (prev_bucket->key == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket *new_bucket = heap_buffer + heap_buffer_capacity - 1ul;
                size_t heap_buffer_decrements = 0ul;
                while (new_bucket->key != NULL && ++heap_buffer_decrements < heap_buffer_capacity) {
                    --new_bucket;
                }
                if (new_bucket->key != NULL) {
                    new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                    while (new_bucket->key != NULL) {
                        --new_bucket;
                    }
                }
                new_bucket->key = b->key;
                new_bucket->data = b->data;
                new_bucket->next = NULL;
                prev_bucket->next = new_bucket;
            }
        }
        memcpy(this->stack_buffer, stack_buffer, HASH_MAP_STACK_CAPACITY * sizeof(bucket));
        free(this->heap_buffer);
        this->heap_buffer = heap_buffer;
        this->heap_buffer_capacity = heap_buffer_capacity;
    } else { // stack reallocation only
        for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
            const bucket *const b = this->stack_buffer + i;
            if (b->key != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), this->key_sz, b->key);
                bucket *prev_bucket = stack_buffer + key_hash;
                if (prev_bucket->key == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket *new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                while (new_bucket->key != NULL) {
                    --new_bucket;
                }
                new_bucket->key = b->key;
                new_bucket->data = b->data;
                new_bucket->next = NULL;
                prev_bucket->next = new_bucket;
            }
        }
        for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
            const bucket *const b = this->heap_buffer + i;
            if (b->key != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), this->key_sz, b->key);
                bucket *prev_bucket = stack_buffer + key_hash;
                if (prev_bucket->key == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket *new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                while (new_bucket->key != NULL) {
                    --new_bucket;
                }
                new_bucket->key = b->key;
                new_bucket->data = b->data;
                new_bucket->next = NULL;
                prev_bucket->next = new_bucket;
            }
        }
        memcpy(this->stack_buffer, stack_buffer, HASH_MAP_STACK_CAPACITY * sizeof(bucket));
        free(this->heap_buffer);
        this->heap_buffer = NULL;
        this->heap_buffer_capacity = 0ul;
    }
}

[[nodiscard]] static bucket *hash_map_bucket_at(
    const hash_map *const restrict this,
    const void *const restrict key
) {
    const size_t capacity = HASH_MAP_STACK_CAPACITY + this->heap_buffer_capacity;
    assert(this != NULL && capacity != 0ul);
    const size_t key_hash = this->hash_function(HASH_MOD(capacity), this->key_sz, key);
    bucket *b;
    if (key_hash < HASH_MAP_STACK_CAPACITY) {
        b = (bucket*)this->stack_buffer + key_hash;
    } else {
        b = this->heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
    }
    if (b->key == NULL) {
        return NULL;
    }
    do {
        const unsigned char key_comparison_result = this->key_comparator(key, b->key);
        if (key_comparison_result == 0) { // key == b->key
            return b;
        }
        b = b->next;
    } while (b != NULL);
    return NULL;
}

[[nodiscard]] node_data *hash_map_at(
    const hash_map *const restrict this,
    const void *const restrict key
) {
    const size_t capacity = HASH_MAP_STACK_CAPACITY + this->heap_buffer_capacity;
    if (this == NULL || capacity == 0ul) {
        return NULL;
    }
    bucket *const bucket_at = hash_map_bucket_at(this, key);
    if (bucket_at == NULL) {
        return NULL;
    }
    return &bucket_at->data;
}

void hash_map_insert(
    hash_map *const restrict this,
    const void *const restrict key,
    const size_t data_sz,
    const void *const restrict data,
    const unsigned char intrusive
) {
    // rehash
    size_t capacity = HASH_MAP_STACK_CAPACITY + this->heap_buffer_capacity;
    if (capacity > 0ul) {
        const size_t occupied_size = hash_map_size(this);
        const long double load_factor = (long double)occupied_size / (long double)capacity;
        if (load_factor > LOAD_FACTOR_MAX) {
            capacity = (capacity + 1ul) << 1;
            hash_map_rehash(this, capacity);
        }
    } else { // first allocation
        assert(this->heap_buffer == NULL);
        const size_t heap_buffer_capacity = HASH_MAP_MIN_CAPACITY;
        bucket *const heap_buffer = calloc(heap_buffer_capacity, sizeof(bucket));
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in hash_map_insert for capacity %lu\n", heap_buffer_capacity);
            return;
        }
        for (size_t i = 0ul; i < heap_buffer_capacity; ++i) {
            bucket *const b = heap_buffer + i;
            b->key = NULL;
            b->data.data = NULL;
            b->data.type_sz = 0ul;
            b->next = NULL;
        }
        this->heap_buffer = heap_buffer;
        this->heap_buffer_capacity = heap_buffer_capacity;
    }
    // insertion
    const size_t key_hash = this->hash_function(HASH_MOD(capacity), this->key_sz, key);
    bucket *b;
    if (key_hash < HASH_MAP_STACK_CAPACITY) {
        b = this->stack_buffer + key_hash;
    } else {
        b = this->heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
    }
    if (b->key == NULL) { // not present (no collision) -> new
        if (intrusive) {
            b->key = (void*)key;
            b->data.data = (void*)data;
        } else {
            b->key = malloc(this->key_sz);
            memcpy(b->key, key, this->key_sz);
            b->data.data = malloc(data_sz);
            memcpy(b->data.data, data, data_sz);
        }
        b->data.type_sz = data_sz;
        b->next = NULL;
        return;
    }
    // collision or reassignment
    bucket *prev_bucket;
    do {
        const unsigned char key_comparison_result = this->key_comparator(key, b->key);
        if (key_comparison_result == 0) { // key == b->key
            // already present -> reassigning
            if (b->data.type_sz != data_sz) {
                free(b->data.data);
                void *const new_data = malloc(data_sz);
                b->data.data = new_data;
                b->data.type_sz = data_sz;
            }
            memcpy(b->data.data, data, data_sz);
            return;
        }
        prev_bucket = b;
        b = b->next;
    } while (b != NULL);
    // collision
    bucket *new_bucket = this->heap_buffer + this->heap_buffer_capacity - 1ul;
    size_t heap_buffer_decrements = 0ul;
    while (new_bucket->key != NULL && ++heap_buffer_decrements < this->heap_buffer_capacity) {
        --new_bucket;
    }
    if (new_bucket->key != NULL) {
        new_bucket = this->stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
        while (new_bucket->key != NULL) {
            --new_bucket;
        }
    }
    if (intrusive) {
        new_bucket->key = (void*)key;
        new_bucket->data.data = (void*)data;
    } else {
        new_bucket->key = malloc(this->key_sz);
        memcpy(new_bucket->key, key, this->key_sz);
        new_bucket->data.data = malloc(data_sz);
        memcpy(new_bucket->data.data, data, data_sz);
    }
    new_bucket->data.type_sz = data_sz;
    new_bucket->next = NULL;
    prev_bucket->next = new_bucket;
}

void hash_map_remove(
    hash_map *const restrict this,
    const void *const restrict key,
    const unsigned char intrusive
) {
    size_t capacity = HASH_MAP_STACK_CAPACITY + this->heap_buffer_capacity;
    if (this == NULL || capacity == 0ul) {
        return;
    }
    const size_t key_hash = this->hash_function(HASH_MOD(capacity), this->key_sz, key);
    bucket *b;
    if (key_hash < HASH_MAP_STACK_CAPACITY) {
        b = this->stack_buffer + key_hash;
    } else {
        b = this->heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
    }
    if (b->key == NULL) { // not present -> exit
        return;
    }
    // lookup
    bucket *prev_bucket = NULL;
    do {
        const unsigned char key_comparison_result = this->key_comparator(key, b->key);
        if (key_comparison_result == 0) { // key == b->key
            // present -> removing
            if (!intrusive) {
                free(b->key);
                free(b->data.data);
            }
            if (prev_bucket != NULL) {
                prev_bucket->next = b->next;
            }
            b->key = NULL;
            b->data.data = NULL;
            b->data.type_sz = 0ul;
            b->next = NULL;
            // rehash
            const size_t occupied_size = hash_map_size(this);
            const long double load_factor = (long double)occupied_size / (long double)capacity;
            if (load_factor < LOAD_FACTOR_MIN && capacity > HASH_MAP_MIN_CAPACITY) {
                capacity >>= 1;
                if (capacity < HASH_MAP_MIN_CAPACITY) {
                    capacity = HASH_MAP_MIN_CAPACITY;
                }
                hash_map_rehash(this, capacity);
            }
            return;
        }
        prev_bucket = b;
        b = b->next;
    } while (b != NULL);
    // not present -> exit
}

void hash_map_delete(
    hash_map *const this,
    const unsigned char intrusive
) {
    if (!intrusive) {
        for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
            const bucket *const b = this->stack_buffer + i;
            if (b->key != NULL) {
                free(b->key);
                free(b->data.data);
            }
        }
        for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
            const bucket *const b = this->heap_buffer + i;
            if (b->key != NULL) {
                free(b->key);
                free(b->data.data);
            }
        }
    }
    free(this->heap_buffer);
    free(this);
}

void hash_map_print(
    const hash_map *const this,
    const print_t print_key,
    const print_t print_data
) {
    printf("{");
    unsigned char not_first = 0;
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        const bucket *const b = this->stack_buffer + i;
        if (b->key != NULL) {
            if (not_first) {
                printf(", ");
            } else {
                not_first = 1;
            }
            print_key(b->key);
            printf(": ");
            print_data(b->data.data);
        }
    }
    for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
        const bucket *const b = this->heap_buffer + i;
        if (b->key != NULL) {
            if (not_first) {
                printf(", ");
            } else {
                not_first = 1;
            }
            print_key(b->key);
            printf(": ");
            print_data(b->data.data);
        }
    }
    printf("}\n");
}

// hash functions

[[nodiscard]] static unsigned long long pow_mod(
    const unsigned long long n,
    unsigned long long p,
    const unsigned long long m
) {
    unsigned long long mult = n % m;
    unsigned long long prod = 1;
    while (p > 0) {
        if (p % 2) {
            prod = prod * mult % m;
            --p;
        }
        mult = mult * mult % m;
        p /= 2;
    }
    return prod;
}

#ifdef __x86_64__
[[nodiscard]] static unsigned long long rdrand(void) {
    unsigned long long r;
    asm volatile (
        "rdrand %0"
        : "=r" (r)
        :
    );
    return r;
}
#else
[[nodiscard]] static unsigned long long rdrand(void) {
    return lrand48();
}
#endif

[[nodiscard]] static unsigned char is_prime(
    const unsigned long long n,
    unsigned long long attempts
) {
    if (n == 2 || n == 3) {
        return 1;
    }
    if (n < 2 || n % 2 == 0 || n % 3 == 0 || ((n - 1) % 6 != 0 && (n + 1) % 6 != 0)) {
        return 0;
    }
    const unsigned long long nm1 = n - 1;
    do {
        const unsigned long long a = (unsigned long long)((long double)rdrand() / (long double)ULLONG_MAX) * nm1 + 1ull;
        if (pow_mod(a, nm1, n) != 1) {
            return 0;
        }
    } while (--attempts);
    return 1;
}

#define HASH_A 228
#define HASH_B 1337
#define HASH_ANY_LOOP(type) while (key_sz >= sizeof(type)) { \
    hash += (HASH_A * *(type*)key + HASH_B) % p % m; \
    key = (char*)key + sizeof(type); \
    key_sz -= sizeof(type); \
}
#define FERMAT_TEST_ATTEMPTS 50

[[nodiscard]] size_t hash_any(
    const size_t m,
    size_t key_sz,
    const void *restrict key
) {
    size_t p = m;
    while (!is_prime(++p, FERMAT_TEST_ATTEMPTS)) {}
    size_t hash = 0;
    HASH_ANY_LOOP(long long);
    HASH_ANY_LOOP(long);
    HASH_ANY_LOOP(int);
    HASH_ANY_LOOP(short);
    HASH_ANY_LOOP(signed char);
    return hash;
}
