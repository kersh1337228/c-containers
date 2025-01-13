#include "str.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define RK_M 256
#define RK_SHIFT (sizeof(size_t) * CHAR_BIT - 8) // mod (2^{8} = 256)

struct string_control_block_t {
    char *buffer;
    size_t capacity;
    size_t ref_counter;
};

[[nodiscard]] static string_control_block *string_control_block_init(const size_t capacity) {
    assert(capacity > 0);
    string_control_block *const control_block = malloc(sizeof(string_control_block));
    if (!control_block) {
        fprintf(stderr, "malloc NULL return in string_control_block_init for size %lu\n", sizeof(string_control_block));
        return NULL;
    }
    control_block->ref_counter = 1;
    control_block->buffer = NULL;
    control_block->capacity = capacity;
    char *buffer = malloc(capacity);
    if (!buffer) {
        fprintf(stderr, "malloc NULL return in string_control_block_init for capacity %lu\n", capacity);
        return NULL;
    }
    control_block->buffer = buffer;
    return control_block;
}

[[nodiscard]] string string_init(const char *const cstr) {
    string this = {
        .size = 0,
        .control_block = NULL
    };
    if (!cstr) {
        return this;
    }
    const size_t size = strlen(cstr);
    if (size <= sizeof(this.buffer)) {
        memcpy(this.buffer, cstr, size);
    } else {
        string_control_block *const control_block = string_control_block_init(size << 1);
        if (!control_block) {
            return this;
        }
        this.control_block = control_block;
        memcpy(control_block->buffer, cstr, size);
    }
    this.size = size;
    return this;
}

[[nodiscard]] size_t string_length(const string *this) {
    if (!this) {
        return 0;
    }
    return this->size;
}

void string_insert(
    string *const restrict this,
    const size_t index,
    const string *const restrict other
) {
    if (!this || !other || index > this->size || !other->size) {
        return;
    }
    const size_t size = this->size + other->size;
    if (size <= sizeof(this->buffer)) { // new string fits on stack
        char *const insertion = this->buffer + index;
        memcpy(insertion + other->size, insertion, this->size - index);
        memcpy(insertion, other->buffer, other->size);
    } else { // new string needs heap
        const char *const other_buffer = other->size <= sizeof(other->buffer) ? other->buffer : other->control_block->buffer;
        if (this->size <= sizeof(this->buffer)) { // this string fits on stack
            string_control_block *const control_block = string_control_block_init(size << 1);
            if (!control_block) {
                return;
            }
            memcpy(control_block->buffer, this->buffer, index);
            memcpy(control_block->buffer + index, other_buffer, other->size);
            memcpy(control_block->buffer + index + other->size, this->buffer + index, this->size - index);
            this->control_block = control_block;
        } else { // this string needs heap
            assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter);
            if (this->control_block->ref_counter > 1) { // lazy copy
                string_control_block *const control_block = string_control_block_init(size << 1);
                if (!control_block) {
                    return;
                }
                memcpy(control_block->buffer, this->control_block->buffer, index);
                memcpy(control_block->buffer + index, other_buffer, other->size);
                memcpy(control_block->buffer + index + other->size, this->control_block->buffer + index, this->size - index);
                --this->control_block->ref_counter;
                this->control_block = control_block;
            } else { // unique ownership
                if (this->control_block->capacity < size) { // realloc
                    size_t capacity = size << 1; // double the capacity in advance
                    char *buffer = realloc(this->control_block->buffer, capacity);
                    if (!buffer) {
                        fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                        capacity = size; // retrying with exact necessary capacity
                        buffer = realloc(this->control_block->buffer, capacity);
                    }
                    if (!buffer) {
                        fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                        return;
                    }
                    this->control_block->buffer = buffer;
                    this->control_block->capacity = capacity;
                }
                char *const insertion = this->control_block->buffer + index;
                memcpy(insertion + other->size, insertion, this->size - index);
                memcpy(insertion, other_buffer, other->size);
            }
        }
    }
    this->size = size;
}

void string_rconcat(
    string *const restrict this,
    const string *const restrict other
) {
    string_insert(this, this->size, other);
}

void string_lconcat(
    string *const restrict this,
    const string *const restrict other
) {
    string_insert(this, 0, other);
}

void string_remove(
    string *const this,
    const size_t index,
    size_t count
) {
    if (!this || index >= this->size || !count) {
        return;
    }
    size_t end = index + count;
    if (end > this->size) {
        end = this->size;
        count = end - index;
    }
    const size_t size = this->size - count;
    if (this->size <= sizeof(this->buffer)) { // this string fits on stack
        char *const removal = this->buffer + index;
        memcpy(removal, removal + count, this->size - end);
    } else { // this string needs heap
        assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter);
        if (size <= sizeof(this->buffer)) { // new string fits on stack
            string_control_block *const control_block = this->control_block;
            memcpy(this->buffer, control_block->buffer, index);
            memcpy(this->buffer + index, control_block->buffer + index + count, this->size - end);
            if (!--control_block->ref_counter) { // freeing control block
                free(control_block->buffer);
                free(control_block);
            }
        } else { // new string needs heap
            if (this->control_block->ref_counter > 1) { // lazy copy
                string_control_block *const control_block = string_control_block_init(size << 1);
                if (!control_block) {
                    return;
                }
                memcpy(control_block->buffer, this->control_block->buffer, index);
                memcpy(control_block->buffer + index, this->control_block->buffer + index + count, this->size - end);
                --this->control_block->ref_counter;
                this->control_block = control_block;
            } else { // unique ownership
                if (size < this->control_block->capacity >> 2) { // realloc
                    size_t capacity = this->size << 1; // half the capacity
                    char *buffer = realloc(this->control_block->buffer, capacity);
                    if (!buffer) {
                        fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                        capacity = size; // retrying with exact necessary capacity
                        buffer = realloc(this->control_block->buffer, capacity);
                    }
                    if (!buffer) {
                        fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                        return;
                    }
                    this->control_block->buffer = buffer;
                    this->control_block->capacity = capacity;
                }
                char *const removal = this->control_block->buffer + index;
                memcpy(removal, removal + count, this->size - end);
            }
        }
    }
    this->size = size;
}

void string_rtrim(
    string *const this,
    size_t count
) {
    if (!this || !this->size || !count) {
        return;
    }
    if (this->size < count) {
        count = this->size;
    }
    string_remove(this, this->size - count, count);
}

void string_ltrim(
    string *const this,
    const size_t count
) {
    string_remove(this, 0, count);
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

void string_rtrim_like(
    string *const restrict this,
    const char *const restrict like,
    const unsigned char case_insensitive
) {
    if (!this || !like) {
        return;
    }
    const size_t like_len = strlen(like);
    if (!like_len || like_len > this->size) {
        return;
    }
    const char *const current_buffer = this->size <= sizeof(this->buffer) ? this->buffer : this->control_block->buffer;
    size_t trim_size = 0;
    while (this->size - trim_size >= like_len) {
        if (compare_string(current_buffer + this->size - like_len - trim_size, like, like_len, case_insensitive)) { // no match
            break;
        }
        trim_size += like_len;
    }
    string_rtrim(this, trim_size);
}

void string_ltrim_like(
    string *const restrict this,
    const char *const restrict like,
    const unsigned char case_insensitive
) {
    if (!this || !like) {
        return;
    }
    const size_t like_len = strlen(like);
    if (!like_len || like_len > this->size) {
        return;
    }
    const char *const current_buffer = this->size <= sizeof(this->buffer) ? this->buffer : this->control_block->buffer;
    size_t trim_size = 0;
    while (this->size - trim_size >= like_len) {
        if (compare_string(current_buffer + trim_size, like, like_len, case_insensitive)) { // no match
            break;
        }
        trim_size += like_len;
    }
    string_ltrim(this, trim_size);
}

void string_trim_like(
    string *const restrict this,
    const char *const restrict like,
    const unsigned char case_insensitive
) {
    string_ltrim_like(this, like, case_insensitive);
    string_rtrim_like(this, like, case_insensitive);
}

void string_set(
    string *const this,
    const size_t index,
    const char c
) {
    if (!this || index >= this->size) {
        return;
    }
    if (this->size <= sizeof(this->buffer)) { // this string fits on stack
        this->buffer[index] = c;
    } else { // this string needs heap
        assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter);
        if (this->control_block->ref_counter > 1) { // lazy copy
            string_control_block *const control_block = string_control_block_init(this->control_block->capacity);
            if (!control_block) {
                return;
            }
            memcpy(control_block->buffer, this->control_block->buffer, this->size);
            --this->control_block->ref_counter;
            this->control_block = control_block;
        }
        this->control_block->buffer[index] = c;
    }
}

[[nodiscard]] char string_get(
    const string *const this,
    const size_t index
) {
    if (!this || index >= this->size) {
        return '\0';
    }
    return this->size <= sizeof(this->buffer) ? this->buffer[index] : this->control_block->buffer[index];
}

[[nodiscard]] string string_copy(const string *const this) {
    string copy = {
        .control_block = NULL,
        .size = 0
    };
    if (!this) {
        return copy;
    }
    copy.control_block = this->control_block;
    copy.size = this->size;
    if (this->size > sizeof(this->buffer)) {
        ++this->control_block->ref_counter;
    }
    return copy;
}

[[nodiscard]] string string_substring(
    const string *const this,
    const size_t index,
    size_t count
) {
    string substring = {
        .control_block = NULL,
        .size = 0
    };
    if (!this || index >= this->size || !count) {
        return substring;
    }
    size_t end = index + count;
    if (end > this->size) {
        end = this->size;
        count = end - index;
    }
    const char *const buffer = this->size <= sizeof(this->buffer) ? this->buffer : this->control_block->buffer;
    if (count <= sizeof(substring.buffer)) { // substring fits on stack
        memcpy(substring.buffer, buffer + index, count);
    } else { // substring needs heap
        string_control_block *const control_block = string_control_block_init(count << 1);
        if (!control_block) {
            return substring;
        }
        memcpy(control_block->buffer, buffer + index, count);
        substring.control_block = control_block;
    }
    substring.size = count;
    return substring;
}

[[nodiscard]] long long string_lfind(
    const string *const restrict this,
    const char *const restrict str,
    const unsigned char case_insensitive
) {
    if (!this || !str) {
        return -1;
    }
    const size_t str_len = strlen(str);
    if (!str_len || str_len > this->size) {
        return -1;
    }
    // calculating str hash
    size_t str_hash = 0;
    for (size_t i = 0; i < str_len; ++i) {
        // h += c_{i} * 2^{l - i - 1} mod 256
        const int ci = case_insensitive ? tolower(str[i]) : str[i];
        str_hash = (str_hash + (ci << (str_len - i - 1))) & (-1ul >> RK_SHIFT);
    }
    // calculating initial string hash
    const char *const buffer = this->size <= sizeof(this->buffer) ? this->buffer : this->control_block->buffer;
    size_t hash = 0;
    for (size_t i = 0; i < str_len; ++i) {
        const int ci = case_insensitive ? tolower(buffer[i]) : buffer[i];
        hash = (hash + (ci << (str_len - i - 1))) & (-1ul >> RK_SHIFT);
    }
    // looking for str in initial string
    long long start = 0;
    for (;;) {
        // replacing if from found
        if (hash == str_hash && !compare_string(buffer + start, str, str_len, case_insensitive)) { // checking hash collision
            return start;
        }
        if (start + str_len > this->size) {
            break;
        }
        // recalculating hash
        const int start_char = case_insensitive ? tolower(buffer[start]) : buffer[start];
        const int end_char = case_insensitive ? tolower(buffer[start + str_len]) : buffer[start + str_len];
        // h_{i + 1} = (h_{i} - c_{i} * 2^{l - 1}) * 2 + c_{i + l} mod 256
        hash = (((hash + (RK_M - (start_char << (str_len - 1)) & (-1ul >> RK_SHIFT)) & (-1ul >> RK_SHIFT)) << 1) + end_char) & (-1ul >> RK_SHIFT);
        ++start;
    }
    return -1;
}

[[nodiscard]] long long string_rfind(
    const string *const restrict this,
    const char *const restrict str,
    const unsigned char case_insensitive
) {
    if (!this || !str) {
        return -1;
    }
    const size_t str_len = strlen(str);
    if (!str_len || str_len > this->size) {
        return -1;
    }
    // calculating str hash
    size_t str_hash = 0;
    for (size_t i = 0; i < str_len; ++i) {
        // h += c_{i} * 2^{l - i - 1} mod 256
        const int ci = case_insensitive ? tolower(str[i]) : str[i];
        str_hash = (str_hash + (ci << (str_len - i - 1))) & (-1ul >> RK_SHIFT);
    }
    // substring borders
    long long start = (long long)this->size - (long long)str_len;
    // calculating initial string hash
    const char *const buffer = this->size <= sizeof(this->buffer) ? this->buffer : this->control_block->buffer;
    size_t hash = 0;
    for (size_t i = 0; i < str_len; ++i) {
        const int ci = case_insensitive ? tolower(buffer[i]) : buffer[i];
        hash = (hash + (ci << (str_len - i - 1))) & (-1ul >> RK_SHIFT);
    }
    // looking for str in initial string
    for (;;) {
        if (hash == str_hash && !compare_string(buffer + start, str, str_len, case_insensitive)) { // checking hash collision
            return start;
        }
        if (--start < 0) {
            break;
        }
        // recalculating hash
        const int start_char = case_insensitive ? tolower(buffer[start]) : buffer[start];
        const int end_char = case_insensitive ? tolower(buffer[start + str_len]) : buffer[start + str_len];
        // h_{i - 1} = (h_{i} - c_{i + l - 1}) / 2 + c_{i - 1} * 2^{l - 1} mod 256
        hash = (((hash  + (RK_M - (end_char & (-1ul >> RK_SHIFT))) & (-1ul >> RK_SHIFT)) >> 1) + (start_char << (str_len - 1))) & (-1ul >> RK_SHIFT);
    }
    return -1;
}

void string_replace(
    string *const restrict this,
    const char *const restrict from,
    const char *const restrict to,
    const unsigned char case_insensitive
) {
    if (!this || !from || !to || strcmp(from, to) == 0) {
        return;
    }
    const size_t from_len = strlen(from);
    if (!from_len || from_len > this->size) {
        return;
    }
    const size_t to_len = strlen(to);
    const long long len_diff = (long long)to_len - (long long)from_len;
    const unsigned char is_small = this->size <= sizeof(this->buffer);
    char *current_buffer = is_small ? this->buffer : this->control_block->buffer;
    size_t start = 0;
    if (len_diff > 0) { // new string will be bigger
        while (start + from_len <= this->size) {
            if (compare_string(current_buffer + start, from, from_len, case_insensitive)) {
                ++start;
            } else {
                const size_t new_size = this->size + len_diff;
                if (this->size <= sizeof(this->buffer)) { // this string fits on stack
                    if (new_size > sizeof(this->buffer)) { // new string needs heap
                        string_control_block *const control_block = string_control_block_init(new_size << 1);
                        if (!control_block) {
                            return;
                        }
                        memcpy(control_block->buffer, this->buffer, start);
                        memcpy(control_block->buffer + start, to, to_len);
                        memcpy(control_block->buffer + start + to_len, this->buffer + start + from_len, this->size - start - from_len);
                        this->control_block = control_block;
                        start += to_len;
                        this->size += len_diff;
                        current_buffer = control_block->buffer;
                        continue;
                    }
                } else { // this string needs heap
                    assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter);
                    if (this->control_block->ref_counter > 1) { // lazy copy
                        string_control_block *const control_block = string_control_block_init(new_size << 1);
                        if (!control_block) {
                            return;
                        }
                        memcpy(control_block->buffer, this->control_block->buffer, start);
                        memcpy(control_block->buffer + start, to, to_len);
                        memcpy(control_block->buffer + start + to_len, this->control_block->buffer + start + from_len, this->size - start - from_len);
                        --this->control_block->ref_counter;
                        this->control_block = control_block;
                        start += to_len;
                        this->size += len_diff;
                        current_buffer = control_block->buffer;
                        continue;
                    } // unique ownership
                    if (this->control_block->capacity < new_size) { // realloc
                        size_t capacity = new_size << 1; // double the capacity in advance
                        char *buffer = realloc(this->control_block->buffer, capacity);
                        if (!buffer) {
                            fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                            capacity = new_size; // retrying with exact necessary capacity
                            buffer = realloc(this->control_block->buffer, capacity);
                        }
                        if (!buffer) {
                            fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                            return;
                        }
                        this->control_block->buffer = buffer;
                        this->control_block->capacity = capacity;
                        current_buffer = buffer;
                    }
                }
                char *const replacement = current_buffer + start;
                memcpy(replacement + to_len, replacement + from_len, this->size - start - from_len);
                memcpy(replacement, to, to_len);
                start += to_len;
                this->size += len_diff;
            }
        }
    } else {
        while (start + from_len <= this->size) {
            if (compare_string(current_buffer + start, from, from_len, case_insensitive)) { // no match
                ++start;
            } else {
                if (this->size > sizeof(this->buffer) && this->control_block->ref_counter > 1) { // this string needs heap
                    // lazy copy
                    string_control_block *const control_block = string_control_block_init((this->size + len_diff) << 1);
                    if (!control_block) {
                        return;
                    }
                    memcpy(control_block->buffer, this->control_block->buffer, start);
                    memcpy(control_block->buffer + start + to_len, this->control_block->buffer + start + from_len, this->size - start - from_len);
                    memcpy(control_block->buffer + start, to, to_len);
                    --this->control_block->ref_counter;
                    this->control_block = control_block;
                    current_buffer = control_block->buffer;
                } else {
                    char *const replacement = current_buffer + start;
                    memcpy(replacement + to_len, replacement + from_len, this->size - start - from_len);
                    memcpy(replacement, to, to_len);
                }
                start += to_len;
                this->size += len_diff;
            }

        }
        if (len_diff < 0) { // new string will be smaller
            if (!is_small && this->size <= sizeof(this->buffer)) { // new string fits on stack
                string_control_block *const control_block = this->control_block;
                memcpy(this->buffer, control_block->buffer, this->size);
                if (!--control_block->ref_counter) { // freeing control block
                    free(control_block->buffer);
                    free(control_block);
                }
            } else { // new string needs heap
                if (this->control_block->ref_counter == 1 && this->size < this->control_block->capacity >> 2) { // unique ownership & smaller size
                    size_t capacity = this->size << 1; // half the capacity
                    char *buffer = realloc(this->control_block->buffer, capacity);
                    if (!buffer) {
                        fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                        capacity = this->size; // retrying with exact necessary capacity
                        buffer = realloc(this->control_block->buffer, capacity);
                    }
                    if (!buffer) {
                        fprintf(stderr, "realloc NULL return in string_insert for capacity %lu\n", capacity);
                        return;
                    }
                    this->control_block->buffer = buffer;
                    this->control_block->capacity = capacity;
                }
            }
        }
    }
}

[[nodiscard]] long long string_compare(
    const string *const this,
    const string *const other,
    const unsigned char case_insensitive
) {
    if (!this) {
        if (!other) {
            return 0;
        }
        return -(long long)other->size;
    }
    if (!other) {
        return (long long)this->size;
    }
    const long long size_diff = (long long)this->size - (long long)other->size;
    if (size_diff) {
        return size_diff;
    }
    if (this->size <= sizeof(this->buffer)) { // this string fits on stack
        for (size_t i = 0; i < this->size; ++i) {
            const signed char diff = compare_char(this->buffer[i], other->buffer[i], case_insensitive);
            if (diff) {
                return diff;
            }
        }
    } else { // this string neads heap
        assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter
            && other->control_block && other->control_block->capacity && other->control_block->ref_counter);
        for (size_t i = 0; i < this->size; ++i) {
            const signed char diff = compare_char(this->control_block->buffer[i], other->control_block->buffer[i], case_insensitive);
            if (diff) {
                return diff;
            }
        }
    }
    return 0;
}

[[nodiscard]] char *string_cstr(const string *const this) {
    if (!this) {
        return NULL;
    }
    assert(this->control_block != NULL);
    char *cstr = malloc(this->size + 1);
    if (this->size <= sizeof(this->buffer)) { // this string fits on stack
        memcpy(cstr, this->buffer, this->size);
    } else { // this string neads heap
        assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter);
        memcpy(cstr, this->control_block->buffer, this->size);
    }
    cstr[this->size] = '\0';
    return cstr;
}

void string_reverse(string *const this) {
    if (!this || this->size < 2) {
        return;
    }
    if (this->size <= sizeof(this->buffer)) { // this string fits on stack
        char *i = this->buffer;
        char *j = i + this->size - 1;
        do {
            const char tmp = *i;
            *i = *j;
            *j = tmp;
        } while (++i < --j);
    } else { // this string neads heap
        assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter);
        if (this->control_block->ref_counter > 1) { // lazy copy
            string_control_block *const control_block = string_control_block_init(this->control_block->capacity);
            if (!control_block) {
                return;
            }
            for (size_t i = 0; i < this->size; ++i) {
                control_block->buffer[i] = this->control_block->buffer[this->size - 1 - i];
            }
            --this->control_block->ref_counter;
            this->control_block = control_block;
        } else { // unique ownership
            char *i = this->control_block->buffer;
            char *j = i + this->size - 1;
            do {
                const char tmp = *i;
                *i = *j;
                *j = tmp;
            } while (++i < --j);
        }
    }
}

void string_delete(string *const this) {
    if (!this) {
        return;
    }
    if (this->size > sizeof(this->buffer)) { // this string neads heap
        assert(this->control_block && this->control_block->capacity && this->control_block->ref_counter);
        if (!--this->control_block->ref_counter) {
            free(this->control_block->buffer);
            free(this->control_block);
        }
    }
    this->size = 0;
}

void string_print(const string *const this) {
    if (!this || !this->size) {
        return;
    }
    if (this->size <= sizeof(this->buffer)) { // this string fits on stack
        for (size_t i = 0; i < this->size; ++i) {
            printf("%c", this->buffer[i]);
        }
    } else { // this string neads heap
        for (size_t i = 0; i < this->size; ++i) {
            printf("%c", this->control_block->buffer[i]);
        }
    }
}
