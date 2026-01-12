//
// Single-byte strings 
//

function String8 
str8(U8 *data, U64 count)
{
  String8 str = {0};
  str.data = data;
  str.count = count;
  return str;
}

function B32 
str8_equal(String8 a, String8 b)
{
  B32 equal = 1; 
  
  for (U64 idx = 0; idx < a.count; idx += 1) {
    if (a.data[idx] != b.data[idx]) { 
      equal = 0;
      break;
    }
  }
  
  return equal;
}

// NOTE: Implementations of str8_pushfv, str8_pushf based on those found in Mr4th's codebase:
// https://git.mr4th.com/mr4th-public/mr4th/src/branch/main/src/base/base_big_functions.c

function String8
str8_pushfv(Arena *arena, char *fmt, va_list args)
{
  // In case we need to get args a second time
  va_list args2;
  va_copy(args2, args);
  
  // First try to create the string using 1024 bytes
  U64 buffer_size = 1024;
  U8 *buffer = ArenaPushArray(arena, U8, buffer_size);
  U64 actual_size = vsnprintf((char*)buffer, buffer_size, fmt, args);
  
  String8 result = {0};
  if (actual_size < buffer_size) {
    // If we were able to create the string using our predicted size, get rid of any extra bytes.
    arena_pop(arena, buffer_size - actual_size - 1);
    result = str8(buffer, actual_size);
  }
  else {
    // If our initial size was too small, reset and try again with the correct size.
    arena_pop(arena, buffer_size);
    U8 *corrected_buffer = ArenaPushArray(arena, U8, actual_size + 1);
    U64 final_size = vsnprintf((char*)corrected_buffer, actual_size + 1, fmt, args2);
    result = str8(corrected_buffer, final_size);
  }
  
  va_end(args2);
  return result;
}

function String8
str8_pushf(Arena *arena, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  
  String8 result = str8_pushfv(arena, fmt, args);
  
  va_end(args);
  return result;
}

function B32 
str8_read(void *dst, String8 str, U64 off, U64 size)
{
  B32 result = 0;
  
  if (off + size <= str.count) {
    result = 1;
    MemoryCopy(dst, str.data + off, size);
  }
  
  return result; 
}

//
// Two-byte strings
//

function String16 
str16(U16 *data, U64 count)
{
  String16 str = {0};
  str.data = data;
  str.count = count;
  return str;
}

function B32
str16_equal(String16 a, String16 b)
{
  B32 equal = 1; 
  
  for (U64 idx = 0; idx < a.count; idx += 1) {
    if (a.data[idx] != b.data[idx]) { 
      equal = 0;
      break;
    }
  }
  
  return equal;
}

//
// String lists
//

function void
str8_list_push(Arena *arena, String8List *list, String8 str)
{
  String8Node *node = ArenaPushStruct(arena, String8Node);
  node->str = str;
  
  SLLQueuePush(list->first, list->last, node);
  list->count += 1;
  list->size  += str.count;
}

function String8 
str8_list_join(Arena *arena, String8List *list)
{
  String8 result = {0};
  
  U64 total_size = list->size; 
  U8 *buffer = ArenaPushArray(arena, U8, total_size);
  
  U64 pos = 0;
  for (String8Node *n = list->first; n != 0; n = n->next) {
    String8 *str = &n->str;
    U64 size = str->count;
    MemoryCopy(buffer + pos, str->data, size);
    pos += size; 
  }
  
  result.data = buffer;
  result.count = total_size;
  
  return result;
}

//
// C-string helpers
//

function U64 
cstr_count(const char *cstr)
{
  U64 count = 0;
  while (*cstr++ != 0) { 
    count += 1;
  }
  return count;
}

function U32 
cstr_cmp(const char *a, const char *b)
{
  for (;*a && (*a == *b); a++, b++);
  return *a - *b;
}

function U32 
cstr_cmp_n(const char *a, const char *b, U64 count)
{
  U64 idx = 0;
  while (idx < count) {
    if (a[idx] != b[idx]) break;
    idx++;
  }
  return a[idx] - b[idx];
}

function B32 
cstr_equal(const char *a, const char *b)
{
  return (cstr_cmp(a, b) == 0);
}

function B32 
cstr_equal_n(const char *a, const char *b, U64 count)
{
  return (cstr_cmp_n(a, b, count) == 0);
}

//
// Character helpers 
//

function B32 
is_numeric(char c)
{
  return ((c >= '0') && (c <= '9'));
}

function B32 
is_alpha(char c)
{
  return (is_lowercase(c) || is_uppercase(c)); 
}

function B32 
is_lowercase(char c)
{
  return ((c >= 'a') && (c <= 'z'));
}

function B32 
is_uppercase(char c)
{
  return ((c >= 'A') && (c <= 'Z'));
}

function B32 
is_end_of_line(char c)
{
  return ((c == '\n') || (c == '\r'));
}

function B32 
is_whitespace(char c)  
{
  return ((c == ' ') || (c == '\t') || (c == '\f') || (c == '\v') || is_end_of_line(c));
}

//
// Unicode
//

// true if the byte is a valid continuation byte
#define cont(byte) (((byte) & 0xC0) == 0x80)

function UnicodeDecode 
utf8_decode(U8 *in, U64 max)
{
  UnicodeDecode result = { MAX_U32, 1 };
  
  // 1-byte encoding
  if (max >= 1 && in[0] <= 0x7F) {
    result.codepoint = (U32)in[0];
  }
  // 2-byte encoding
  else if (max >= 2 && ((in[0] & 0xE0) == 0xC0)) {
    if (cont(in[1])) {
      result.codepoint  = ((in[0] & 0x1F) << 6);
      result.codepoint |= ((in[1] & 0x3F) << 0);
    }
    result.adv = 2;
  }
  // 3-byte encoding
  else if (max >= 3 && ((in[0] & 0xF0) == 0xE0)) {
    if (cont(in[1]) && cont(in[2])) {
      result.codepoint  = ((in[0] & 0x0F) << 12);
      result.codepoint |= ((in[1] & 0x3F) << 6);
      result.codepoint |= ((in[2] & 0x3F) << 0);
      result.adv = 3;
    }
  }
  // 4-byte encoding
  else if (max >= 4 && ((in[0] & 0xF8) == 0xF0)) {
    if (cont(in[1]) && cont(in[2]) && cont(in[3])) {
      result.codepoint  = ((in[0] & 0x07) << 18);
      result.codepoint |= ((in[1] & 0x3F) << 12);
      result.codepoint |= ((in[2] & 0x3F) << 6);
      result.codepoint |= ((in[3] & 0x3F) << 0);
      result.adv = 4;
    }
  }
  
  return result;
}

#undef cont 

function UnicodeDecode
utf16_decode(U16 *in, U64 max)
{
  UnicodeDecode result = { MAX_U32, 1 };
  
  // Single-unit encoding
  if (max >= 1 && (((in[0] >= 0x0000) && (in[0] <= 0xD7FF)) || ((in[0] >= 0xE000) && (in[1] <= 0xFFFF)))) {
    result.codepoint = (U32)in[0];
  }
  // Two-unit encoding
  else if (max >= 2 && (in[0] >= 0xD800) && (in[0] <= 0xDBFF) && (in[1] >= 0xDC00) && (in[1] <= 0xDFFF)) {
    // Get the payloads from the high and low surrogates and combine them.
    result.codepoint = ((in[0] - 0xD800) << 10) | (in[1] - 0xDC00); 
    // Convert the result from a 16-bit value to a 32-bit value.
    result.codepoint += 0x10000;
    
    result.adv = 2;
  }
  return result;
}

function U32 
utf8_encode(U8 *out, U32 codepoint)
{
  U32 adv = 0;
  
  if (codepoint <= 0x7F) {
    out[0] = (U8)codepoint;
    adv = 1;
  }
  else if (codepoint <= 0x7FF) {
    out[0] = 0xC0 | ((codepoint >> 6) & 0x1F);
    out[1] = 0x80 | (codepoint & 0x3F);
    adv = 2;
  }
  else if (codepoint <= 0xFFFF) {
    out[0] = 0xE0 | ((codepoint >> 12) & 0xF); 
    out[1] = 0x80 | ((codepoint >> 6) & 0x3F);
    out[2] = 0x80 | (codepoint & 0x3F);
    adv = 3;
  }
  else if (codepoint <= 0x10FFFF) {
    out[0] = 0xF0 | ((codepoint >> 18) & 0x7);
    out[1] = 0x80 | ((codepoint >> 12) & 0x3F);
    out[2] = 0x80 | ((codepoint >> 6)  & 0x3F);
    out[3] = 0x80 | (codepoint & 0x3F);
    adv = 4;
  }
  
  return adv;
}

function U32 
utf16_encode(U16 *out, U32 codepoint)
{
  U32 adv = 0;
  
  // Codepoint is inside the Basic Multilingual Plane
  if (codepoint < 0x10000) {
    out[0] = (U16)codepoint;
    adv = 1;
  }
  // Codepoint is outside the BMP (represented by two 16-bit "surrogate" values) 
  else {
    // Each surrogate consists of a 6-bit header and 10-bit payload.
    // The high surrogate's header consists of bits 110110 (0xD800).
    // The low surrogate's header consists of bits 110111 (0xDC00).
    
    // Convert the codepoint to a 20-bit number (two 10-bit payloads).
    U32 v = codepoint - 0x10000;
    
    // Get the final surrogate values by combining the headers and payloads.
    out[0] = (U16)(0xD800 + (v >> 10));
    out[1] = (U16)(0xDC00 + (v & 0x3FF)); 
    
    adv = 2;
  }
  
  return adv;
}

//
// String conversions
//

// NOTE: Result is nul-terminated
function String8
str8_from_str16(Arena *arena, String16 str)
{
  String8 result = {0};
  
  U16 *ptr16 = str.data;
  U64 count16 = str.count;
  U16 *opl16 = ptr16 + count16;
  
  U64 bound = count16 * 4;
  U8 *data8 = ArenaPushArray(arena, U8, bound + 1);
  U8 *ptr8 = data8;
  U64 count8 = 0;
  
  UnicodeDecode dec;
  
  while (ptr16 < opl16) {
    U64 max = opl16 - ptr16;
    dec = utf16_decode(ptr16, max);
    ptr16 += dec.adv;
    
    U32 adv = utf8_encode(ptr8 + count8, dec.codepoint);
    count8 += adv;
  }
  
  data8[count8] = 0;
  
  U64 extra = (bound - count8) * sizeof(U8);
  arena_pop(arena, extra);
  
  result.data = data8;
  result.count = count8;
  return result;
}

// NOTE: Result is nul-terminated
function String16
str16_from_str8(Arena *arena, String8 str)
{
  String16 result = {0};
  
  U8 *ptr8 = str.data;
  U64 count8 = str.count;
  U8 *opl8 = ptr8 + count8;
  
  U64 bound = count8 * 2;
  U16 *data16 = ArenaPushArray(arena, U16, bound + 1);
  U16 *ptr16 = data16;
  U64 count16 = 0;
  
  UnicodeDecode dec;
  
  while (ptr8 < opl8) {
    U64 max = opl8 - ptr8;
    dec = utf8_decode(ptr8, max);
    ptr8 += dec.adv;
    
    U32 adv = utf16_encode(ptr16 + count16, dec.codepoint);
    count16 += adv;
  }
  
  data16[count16] = 0;
  
  U64 extra = (bound - count16) * sizeof(U16);
  arena_pop(arena, extra);
  
  result.data = data16;
  result.count = count16;
  return result;
}

//
// Hashes 
//

// DJB2 hash
function U64 
hash_from_str8(String8 str)
{
  U64 result = 5381;
  
  U64 count = str.count;
  for (U64 idx = 0; idx < count; idx += 1) {
    result = ((result << 5) + result) + str.data[idx];
  }
  
  return result;
}