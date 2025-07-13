#ifndef STR_H
#define STR_H

#include <stdlib.h>

struct string_control_block;
typedef struct string_control_block string_control_block_t;

struct string {
    size_t size;
    union { // Small String Optimization
        string_control_block_t *control_block; // Copy On Write
        char buffer[sizeof(string_control_block_t *)];
    };
};
typedef struct string string_t;

[[ nodiscard ]] string_t string_init(const char *cstr);
[[ nodiscard ]] size_t string_length(const string_t *this);
void string_insert(string_t *this, size_t index, const string_t *other);
void string_rconcat(string_t *this, const string_t *other);
void string_lconcat(string_t *this, const string_t *other);
void string_remove(string_t *this, size_t index, size_t count);
void string_rtrim(string_t *this, size_t count);
void string_ltrim(string_t *this, size_t count);
void string_rtrim_like(string_t *this, const char *like, unsigned char case_insensitive);
void string_ltrim_like(string_t *this, const char *like, unsigned char case_insensitive);
void string_trim_like(string_t *this, const char *like, unsigned char case_insensitive);
void string_set(string_t *this, size_t index, char c);
[[ nodiscard ]] char string_get(const string_t *this, size_t index);
[[ nodiscard ]] string_t string_copy(const string_t *this);
[[ nodiscard ]] string_t string_substring(const string_t *this, size_t index, size_t count);
[[ nodiscard ]] long long string_lfind(const string_t *this, const char* str, unsigned char case_insensitive);
[[ nodiscard ]] long long string_rfind(const string_t *this, const char* str, unsigned char case_insensitive);
void string_replace(string_t *this, const char *from, const char *to, unsigned char case_insensitive);
[[ nodiscard ]] long long string_compare(const string_t *this, const string_t *other, unsigned char case_insensitive);
[[ nodiscard ]] char *string_cstr(const string_t *this);
void string_reverse(string_t *this);
void string_delete(string_t *this);
void string_print(const string_t *this);

#endif // STR_H
