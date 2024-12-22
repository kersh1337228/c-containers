#ifndef STR_H
#define STR_H

#include <stdlib.h>

#define STRING_STACK_BUFFER_CAPACITY 8ul

struct string_control_block_t;
typedef struct string_control_block_t string_control_block;

struct string_t {
    char stack_buffer[STRING_STACK_BUFFER_CAPACITY]; // Small String Optimization
    size_t size;
    string_control_block *control_block; // Copy On Write
};
typedef struct string_t string;

[[nodiscard]] string string_init(const char *init_str);
[[nodiscard]] size_t string_length(const string *this);
void string_insert(string *this, size_t index, const string *other);
void string_rconcat(string *this, const string *other);
void string_lconcat(string *this, const string *other);
void string_remove(string *this, size_t index, size_t count);
void string_rtrim(string *this, size_t count);
void string_ltrim(string *this, size_t count);
unsigned char string_rtrim_like(string *this, const char *like, unsigned char case_insensitive);
unsigned char string_ltrim_like(string *this, const char *like, unsigned char case_insensitive);
unsigned char string_trim_like(string *this, const char *like, unsigned char case_insensitive);
void string_set(string *this, size_t index, char c);
[[nodiscard]] char string_get(const string *this, size_t index);
[[nodiscard]] string string_copy(const string *this);
[[nodiscard]] string string_substring(const string *this, size_t index, size_t count);
[[nodiscard]] long long string_lfind(const string *this, const char* str, unsigned char case_insensitive);
[[nodiscard]] long long string_rfind(const string *this, const char* str, unsigned char case_insensitive);
void string_replace(string *this, const char *from, const char *to);
long long string_compare(const string *this, const string *other, unsigned char case_insensitive);
[[nodiscard]] char *string_cstr(const string *this);
void string_reverse(string *this);
void string_delete(string *this);
void string_print(const string *this);

#endif // STR_H
