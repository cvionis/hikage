#pragma once

// TODO: Use stb_fprintf
#include <stdio.h>

//
// Single-byte strings
//

struct String8 {
  U8 *data;
  U64 count;
};

static String8 str8(U8 *data, U64 count);
static B32 str8_equal(String8 a, String8 b);
static String8 str8_pushfv(Arena *arena, char *fmt, va_list args);
static String8 str8_pushf(Arena *arena, char *fmt, ...);
static B32 str8_read(void *dst, String8 str, U64 off, U64 size);

#define S8(lit) {(U8 *)lit, sizeof(lit)-1}
#define chr_from_str8(str) (char *)str.data

//
// Two-byte strings
//

struct String16 {
  U16 *data;
  U64 count;
};

static String16 str16(U16 *data, U64 count);
static B32 str16_equal(String16 a, String16 b);
static String16 str16_pushfv(Arena *arena, char *fmt, va_list args);
static String16 str16_pushf(Arena *arena, char *fmt, ...);
static B32 str16_read(void *dst, String16 str, U64 off, U64 size);

#define S16(lit) {(U16 *)lit, sizeof(lit)-1}
#define chr_from_str16(str) (char *)str.data

//
// Unicode
//

struct UnicodeDecode {
  U32 codepoint;
  U32 adv;
};

static UnicodeDecode utf8_decode(U8 *in, U64 max);
static UnicodeDecode utf16_decode(U16 *in, U64 max);
static U32 utf8_encode(U8 *out, U32 codepoint);
static U32 utf16_encode(U16 *out, U32 codepoint);

//
// String conversions
//

// Note: Values returned by these statics are nul-terminated.
static String8 str8_from_str16(Arena *arena, String16 str);
static String16 str16_from_str8(Arena *arena, String8 str);

//
// String lists
//

struct String8Node {
  String8Node *next;
  String8 str;
};

struct String8List {
  String8Node *first;
  String8Node *last;
  U32 count;
  U64 size;
};

static void str8_list_push(Arena *arena, String8List *list, String8 str);
static String8 str8_list_join(Arena *arena, String8List *list);

//
// C-string helpers
//

static U64 cstr_count(const char *cstr);
static U32 cstr_cmp(const char *a, const char *b);
static U32 cstr_cmp_n(const char *a, const char *b, U64 count);
static B32 cstr_equal(const char *a, const char *b);
static B32 cstr_equal_n(const char *a, const char *b, U64 count);

//
// Character helpers
//

static B32 is_numeric(char c);
static B32 is_alpha(char c);
static B32 is_lowercase(char c);
static B32 is_uppercase(char c);
static B32 is_end_of_line(char c);
static B32 is_whitespace(char c);

//
// Hashes
//

static U64 hash_from_str8(String8 str);
