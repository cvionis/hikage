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

function String8 str8(U8 *data, U64 count);
function B32 str8_equal(String8 a, String8 b);
function String8 str8_pushfv(Arena *arena, char *fmt, va_list args);
function String8 str8_pushf(Arena *arena, char *fmt, ...);
function B32 str8_read(void *dst, String8 str, U64 off, U64 size);

#define S8(lit) {(U8 *)lit, sizeof(lit)-1}

//
// Two-byte strings
//

struct String16 {
  U16 *data;
  U64 count;
};

function String16 str16(U16 *data, U64 count);
function B32 str16_equal(String16 a, String16 b);
function String16 str16_pushfv(Arena *arena, char *fmt, va_list args);
function String16 str16_pushf(Arena *arena, char *fmt, ...);
function B32 str16_read(void *dst, String16 str, U64 off, U64 size);

#define S16(lit) {(U16 *)lit, sizeof(lit)-1}

//
// Unicode
//

struct UnicodeDecode {
  U32 codepoint;
  U32 adv;
};

function UnicodeDecode utf8_decode(U8 *in, U64 max);
function UnicodeDecode utf16_decode(U16 *in, U64 max);
function U32 utf8_encode(U8 *out, U32 codepoint);
function U32 utf16_encode(U16 *out, U32 codepoint);

//
// String conversions
//

// Note: Values returned by these functions are nul-terminated.
function String8 str8_from_str16(Arena *arena, String16 str);
function String16 str16_from_str8(Arena *arena, String8 str);

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

function void str8_list_push(Arena *arena, String8List *list, String8 str);
function String8 str8_list_join(Arena *arena, String8List *list); 

//
// C-string helpers
//

function U64 cstr_count(const char *cstr);
function U32 cstr_cmp(const char *a, const char *b);
function U32 cstr_cmp_n(const char *a, const char *b, U64 count);
function B32 cstr_equal(const char *a, const char *b);
function B32 cstr_equal_n(const char *a, const char *b, U64 count);

//
// Character helpers 
//

function B32 is_numeric(char c);
function B32 is_alpha(char c);
function B32 is_lowercase(char c);
function B32 is_uppercase(char c);
function B32 is_end_of_line(char c);
function B32 is_whitespace(char c);  

//
// Hashes
//

function U64 hash_from_str8(String8 str);