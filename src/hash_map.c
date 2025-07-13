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

struct bucket {
    node_data_t key;
    node_data_t data;
    bucket_t *next; // coalesced hashing
};

struct hash_map {
    bucket_t stack_buffer[HASH_MAP_STACK_CAPACITY]; // Small Object Optimization
    bucket_t *heap_buffer;
    size_t heap_buffer_capacity;
    hash_t hash_function;
    comparator_t key_comparator;
};

[[nodiscard]] hash_map_t *hash_map_init(
    size_t capacity,
    const hash_t hash_function,
    const comparator_t key_comparator
) {
    if (hash_function == NULL || key_comparator == NULL) {
        return NULL;
    }
    hash_map_t *const hm = malloc(sizeof(hash_map_t));
    if (hm == NULL) {
        fprintf(stderr, "malloc NULL return in hash_map_init\n");
        return hm;
    }
    hm->hash_function = hash_function;
    hm->key_comparator = key_comparator;
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        bucket_t *const b = hm->stack_buffer + i;
        b->key.data = NULL;
        b->key.type_sz = 0;
        b->data.data = NULL;
        b->data.type_sz = 0ul;
        b->next = NULL;
    }
    bucket_t *heap_buffer = NULL;
    size_t heap_buffer_capacity = 0ul;
    if (capacity < HASH_MAP_MIN_CAPACITY) {
        capacity = HASH_MAP_MIN_CAPACITY;
    }
    if (capacity > HASH_MAP_STACK_CAPACITY) {
        heap_buffer_capacity = capacity - HASH_MAP_STACK_CAPACITY;
        heap_buffer = calloc(heap_buffer_capacity, sizeof(bucket_t));
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in hash_map_init for capacity %lu\n", capacity);
            hm->heap_buffer = NULL;
            hm->heap_buffer_capacity = 0ul;
            return hm;
        }
        for (size_t i = 0ul; i < heap_buffer_capacity; ++i) {
            bucket_t *const b = heap_buffer + i;
            b->key.data = NULL;
            b->key.type_sz = 0;
            b->data.data = NULL;
            b->data.type_sz = 0ul;
            b->next = NULL;
        }
    }
    hm->heap_buffer = heap_buffer;
    hm->heap_buffer_capacity = heap_buffer_capacity;
    return hm;
}

[[nodiscard]] size_t hash_map_size(const hash_map_t *const this) {
    if (this == NULL || this->heap_buffer_capacity == 0ul) {
        return 0;
    }
    size_t size = 0ul;
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        const bucket_t *const b = this->stack_buffer + i;
        if (b->key.data != NULL) {
            ++size;
        }
    }
    for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
        const bucket_t *const b = this->heap_buffer + i;
        if (b->key.data != NULL) {
            ++size;
        }
    }
    return size;
}

static void hash_map_rehash(
    hash_map_t *const this,
    const size_t capacity
) {
    assert(this != NULL && capacity != 0ul);
    bucket_t stack_buffer[HASH_MAP_STACK_CAPACITY];
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        bucket_t *const b = stack_buffer + i;
        b->key.data = NULL;
        b->key.type_sz = 0;
        b->data.data = NULL;
        b->data.type_sz = 0;
        b->next = NULL;
    }
    if (capacity > HASH_MAP_STACK_CAPACITY) { // both stack and heap reallocation required
        const size_t heap_buffer_capacity = capacity - HASH_MAP_STACK_CAPACITY;
        bucket_t *const heap_buffer = calloc(heap_buffer_capacity, sizeof(bucket_t));
        for (size_t i = 0ul; i < heap_buffer_capacity; ++i) {
            bucket_t *const b = heap_buffer + i;
            b->key.data = NULL;
            b->key.type_sz = 0;
            b->data.data = NULL;
            b->data.type_sz = 0;
            b->next = NULL;
        }
        for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
            const bucket_t *const b = this->stack_buffer + i;
            if (b->key.data != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), b->key.type_sz, b->key.data);
                bucket_t *prev_bucket;
                if (key_hash < HASH_MAP_STACK_CAPACITY) {
                    prev_bucket = stack_buffer + key_hash;
                } else {
                    prev_bucket = heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
                }
                if (prev_bucket->key.data == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket_t *new_bucket = heap_buffer + heap_buffer_capacity - 1ul;
                size_t heap_buffer_decrements = 0ul;
                while (new_bucket->key.data != NULL && ++heap_buffer_decrements < heap_buffer_capacity) {
                    --new_bucket;
                }
                if (new_bucket->key.data != NULL) {
                    new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                    while (new_bucket->key.data != NULL) {
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
            const bucket_t *const b = this->heap_buffer + i;
            if (b->key.data != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), b->key.type_sz, b->key.data);
                bucket_t *prev_bucket;
                if (key_hash < HASH_MAP_STACK_CAPACITY) {
                    prev_bucket = stack_buffer + key_hash;
                } else {
                    prev_bucket = heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
                }
                if (prev_bucket->key.data == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket_t *new_bucket = heap_buffer + heap_buffer_capacity - 1ul;
                size_t heap_buffer_decrements = 0ul;
                while (new_bucket->key.data != NULL && ++heap_buffer_decrements < heap_buffer_capacity) {
                    --new_bucket;
                }
                if (new_bucket->key.data != NULL) {
                    new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                    while (new_bucket->key.data != NULL) {
                        --new_bucket;
                    }
                }
                new_bucket->key = b->key;
                new_bucket->data = b->data;
                new_bucket->next = NULL;
                prev_bucket->next = new_bucket;
            }
        }
        memcpy(this->stack_buffer, stack_buffer, HASH_MAP_STACK_CAPACITY * sizeof(bucket_t));
        free(this->heap_buffer);
        this->heap_buffer = heap_buffer;
        this->heap_buffer_capacity = heap_buffer_capacity;
    } else { // stack reallocation only
        for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
            const bucket_t *const b = this->stack_buffer + i;
            if (b->key.data != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), b->key.type_sz, b->key.data);
                bucket_t *prev_bucket = stack_buffer + key_hash;
                if (prev_bucket->key.data == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket_t *new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                while (new_bucket->key.data != NULL) {
                    --new_bucket;
                }
                new_bucket->key = b->key;
                new_bucket->data = b->data;
                new_bucket->next = NULL;
                prev_bucket->next = new_bucket;
            }
        }
        for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
            const bucket_t *const b = this->heap_buffer + i;
            if (b->key.data != NULL) {
                const size_t key_hash = this->hash_function(HASH_MOD(capacity), b->key.type_sz, b->key.data);
                bucket_t *prev_bucket = stack_buffer + key_hash;
                if (prev_bucket->key.data == NULL) { // no collision
                    prev_bucket->key = b->key;
                    prev_bucket->data = b->data;
                    prev_bucket->next = NULL;
                    continue;
                }
                // collision
                while (prev_bucket->next != NULL) {
                    prev_bucket = prev_bucket->next;
                }
                bucket_t *new_bucket = stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
                while (new_bucket->key.data != NULL) {
                    --new_bucket;
                }
                new_bucket->key = b->key;
                new_bucket->data = b->data;
                new_bucket->next = NULL;
                prev_bucket->next = new_bucket;
            }
        }
        memcpy(this->stack_buffer, stack_buffer, HASH_MAP_STACK_CAPACITY * sizeof(bucket_t));
        free(this->heap_buffer);
        this->heap_buffer = NULL;
        this->heap_buffer_capacity = 0ul;
    }
}

[[nodiscard]] static bucket_t *hash_map_bucket_at(
    const hash_map_t *const restrict this,
    const size_t key_sz,
    const void *const restrict key
) {
    const size_t capacity = HASH_MAP_STACK_CAPACITY + this->heap_buffer_capacity;
    assert(this != NULL && capacity != 0ul);
    const size_t key_hash = this->hash_function(HASH_MOD(capacity), key_sz, key);
    bucket_t *b;
    if (key_hash < HASH_MAP_STACK_CAPACITY) {
        b = (bucket_t*)this->stack_buffer + key_hash;
    } else {
        b = this->heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
    }
    if (b->key.data == NULL) {
        return NULL;
    }
    do {
        const unsigned char key_comparison_result = this->key_comparator(key, b->key.data);
        if (key_comparison_result == 0) { // key == b->key
            return b;
        }
        b = b->next;
    } while (b != NULL);
    return NULL;
}

[[nodiscard]] node_data_t *hash_map_at(
    const hash_map_t *const restrict this,
    const size_t key_sz,
    const void *const restrict key
) {
    const size_t capacity = HASH_MAP_STACK_CAPACITY + this->heap_buffer_capacity;
    if (this == NULL || capacity == 0ul) {
        return NULL;
    }
    bucket_t *const bucket_at = hash_map_bucket_at(this, key_sz, key);
    if (bucket_at == NULL) {
        return NULL;
    }
    return &bucket_at->data;
}

void hash_map_insert(
    hash_map_t *const restrict this,
    const size_t key_sz,
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
        bucket_t *const heap_buffer = calloc(HASH_MAP_MIN_CAPACITY, sizeof(bucket_t));
        if (heap_buffer == NULL) {
            fprintf(stderr, "malloc NULL return in hash_map_insert for capacity %lu\n", HASH_MAP_MIN_CAPACITY);
            return;
        }
        for (size_t i = 0ul; i < HASH_MAP_MIN_CAPACITY; ++i) {
            bucket_t *const b = heap_buffer + i;
            b->key.data = NULL;
            b->key.type_sz = 0;
            b->data.data = NULL;
            b->data.type_sz = 0ul;
            b->next = NULL;
        }
        this->heap_buffer = heap_buffer;
        this->heap_buffer_capacity = HASH_MAP_MIN_CAPACITY;
    }
    // insertion
    const size_t key_hash = this->hash_function(HASH_MOD(capacity), key_sz, key);
    bucket_t *b;
    if (key_hash < HASH_MAP_STACK_CAPACITY) {
        b = this->stack_buffer + key_hash;
    } else {
        b = this->heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
    }
    if (b->key.data == NULL) { // not present (no collision) -> new
        if (intrusive) {
            b->key.data = (void*)key;
            b->data.data = (void*)data;
        } else {
            b->key.data = malloc(key_sz);
            memcpy(b->key.data, key, key_sz);
            b->data.data = malloc(data_sz);
            memcpy(b->data.data, data, data_sz);
        }
        b->key.type_sz = key_sz;
        b->data.type_sz = data_sz;
        b->next = NULL;
        return;
    }
    // collision or reassignment
    bucket_t *prev_bucket;
    do {
        const unsigned char key_comparison_result = this->key_comparator(key, b->key.data);
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
    bucket_t *new_bucket = this->heap_buffer + this->heap_buffer_capacity - 1ul;
    size_t heap_buffer_decrements = 0ul;
    while (new_bucket->key.data != NULL && ++heap_buffer_decrements < this->heap_buffer_capacity) {
        --new_bucket;
    }
    if (new_bucket->key.data != NULL) {
        new_bucket = this->stack_buffer + HASH_MAP_STACK_CAPACITY - 1ul;
        while (new_bucket->key.data != NULL) {
            --new_bucket;
        }
    }
    if (intrusive) {
        new_bucket->key.data = (void*)key;
        new_bucket->data.data = (void*)data;
    } else {
        new_bucket->key.data = malloc(key_sz);
        memcpy(new_bucket->key.data, key, key_sz);
        new_bucket->data.data = malloc(data_sz);
        memcpy(new_bucket->data.data, data, data_sz);
    }
    new_bucket->key.type_sz = key_sz;
    new_bucket->data.type_sz = data_sz;
    new_bucket->next = NULL;
    prev_bucket->next = new_bucket;
}

void hash_map_remove(
    hash_map_t *const restrict this,
    const size_t key_sz,
    const void *const restrict key,
    const unsigned char intrusive
) {
    size_t capacity = HASH_MAP_STACK_CAPACITY + this->heap_buffer_capacity;
    if (this == NULL || capacity == 0ul) {
        return;
    }
    const size_t key_hash = this->hash_function(HASH_MOD(capacity), key_sz, key);
    bucket_t *b;
    if (key_hash < HASH_MAP_STACK_CAPACITY) {
        b = this->stack_buffer + key_hash;
    } else {
        b = this->heap_buffer + key_hash - HASH_MAP_STACK_CAPACITY;
    }
    if (b->key.data == NULL) { // not present -> exit
        return;
    }
    // lookup
    bucket_t *prev_bucket = NULL;
    do {
        const unsigned char key_comparison_result = this->key_comparator(key, b->key.data);
        if (key_comparison_result == 0) { // key == b->key
            // present -> removing
            if (!intrusive) {
                free(b->key.data);
                free(b->data.data);
            }
            if (prev_bucket != NULL) {
                prev_bucket->next = b->next;
            }
            b->key.data = NULL;
            b->key.type_sz = 0;
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
    hash_map_t *const this,
    const unsigned char intrusive
) {
    if (!intrusive) {
        for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
            const bucket_t *const b = this->stack_buffer + i;
            if (b->key.data != NULL) {
                free(b->key.data);
                free(b->data.data);
            }
        }
        for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
            const bucket_t *const b = this->heap_buffer + i;
            if (b->key.data != NULL) {
                free(b->key.data);
                free(b->data.data);
            }
        }
    }
    free(this->heap_buffer);
    free(this);
}

void hash_map_print(
    const hash_map_t *const this,
    const print_t print_key,
    const print_t print_data
) {
    printf("{");
    unsigned char not_first = 0;
    for (size_t i = 0ul; i < HASH_MAP_STACK_CAPACITY; ++i) {
        const bucket_t *const b = this->stack_buffer + i;
        if (b->key.data != NULL) {
            if (not_first) {
                printf(", ");
            } else {
                not_first = 1;
            }
            print_key(b->key.data);
            printf(": ");
            print_data(b->data.data);
        }
    }
    for (size_t i = 0ul; i < this->heap_buffer_capacity; ++i) {
        const bucket_t *const b = this->heap_buffer + i;
        if (b->key.data != NULL) {
            if (not_first) {
                printf(", ");
            } else {
                not_first = 1;
            }
            print_key(b->key.data);
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
[[nodiscard]] static unsigned long long log2ull(unsigned long long ull) {
    unsigned long long bsr = 0ull;
    asm volatile (
        "bsr %1, %0"
        : "=r" (bsr)
        : "r" (ull)
    );
    return bsr;
}
#else
[[nodiscard]] static unsigned long long rdrand(void) {
    return lrand48();
}
[[nodiscard]] static unsigned long long log2ull(unsigned long long ull) {
    unsigned long long bsr = 0ull;
    while (ull >>= 1) {
        ++bsr;
    }
    return bsr;
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

#define HASH_A 228ull
#define HASH_B 1337ull
#define HASH_ANY_LOOP(type) while (key_sz >= sizeof(type)) { \
    hash = (hash + (HASH_A * *(type*)key + HASH_B) % p) % m; \
    key = (unsigned char*)key + sizeof(type);                \
    key_sz -= sizeof(type);                                  \
}
#define FERMAT_TEST_ATTEMPTS 50ull

[[nodiscard]] size_t hash_any(
    const size_t m,
    size_t key_sz,
    const void *key
) {
    size_t p = m;
    while (!is_prime(++p, FERMAT_TEST_ATTEMPTS)) {}
    size_t hash = 0ul;
    HASH_ANY_LOOP(unsigned long long);
    HASH_ANY_LOOP(unsigned long);
    HASH_ANY_LOOP(unsigned);
    HASH_ANY_LOOP(unsigned short);
    HASH_ANY_LOOP(unsigned char);
    return hash;
}

[[nodiscard]] static size_t mod2(const size_t x, const size_t m) {
    const size_t l2m = log2ull(m) + (m < 2ul);
    const size_t shift = sizeof(size_t) * CHAR_BIT - l2m;
    // return (x << shift) >> shift;
    return x & (-1ul >> shift);
}

[[nodiscard]] size_t hash_ul(
    const size_t m,
    const size_t _,
    const void *key
) {
    assert(m != 0ul);
    const size_t x = *(size_t*)key;
    return mod2(HASH_A * x + HASH_B, m); // ~> (HASH_A * x + HASH_B) % m;
}

[[nodiscard]] size_t hash_str(
    const size_t m,
    const size_t key_sz,
    const void *key
) {
    size_t sum = 0ull;
    for (size_t i = 0ul; i < key_sz; ++i, ++key) {
        const size_t pow = mod2(key_sz - i, sizeof(size_t)); // ~> (l - i) % sizeof(size_t);
        sum += (size_t)*(char*)key << pow;
    }
    return hash_ul(m, key_sz, &sum);
}
