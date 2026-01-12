#pragma once

// TODO: Error stream
// TODO: Parsing negative numbers
// TODO: JSON_ValueKind_Null
// TODO: More rigorous testing/fuzzing

struct JSON_Context {
  Arena *arena; 
  U8 *at; 
  B32 error; 
  U32 line; 
};

typedef struct JSON_Object JSON_Object;
typedef struct JSON_Member JSON_Member;
typedef struct JSON_Array JSON_Array;
typedef struct JSON_Value JSON_Value;

//
// JSON Parsing API
//

enum JSON_TokenKind {
  JSON_Token_Error,
  
  JSON_Token_Comma, 
  JSON_Token_OpenBrace,
  JSON_Token_CloseBrace, 
  JSON_Token_OpenBracket,
  JSON_Token_CloseBracket,
  JSON_Token_Colon,
  
  JSON_Token_Null, 
  JSON_Token_String,
  JSON_Token_Number, 
  JSON_Token_True,
  JSON_Token_False, 
  
  JSON_Token_EOF, 
}; 

struct JSON_Token {
  JSON_TokenKind kind; 
  String8 text;
  F32 number; 
  U32 line; 
};

function B32 json_parsing(JSON_Context *ctx);
function void json_error(JSON_Context *ctx, JSON_Token token, String8 msg);
function void json_eat_whitespace(JSON_Context *ctx);

function void json_make_token_number(JSON_Context *ctx, JSON_Token *t);
function void json_make_token_string(JSON_Context *ctx, JSON_Token *t);
function void json_make_token_boolean(JSON_Context *ctx, JSON_Token *t);

function JSON_Token json_get_token(JSON_Context *ctx);
function JSON_Token json_require_token(JSON_Context *ctx, JSON_TokenKind kind, String8 msg);

function JSON_Object *json_parse_object(JSON_Context *ctx);
function JSON_Value json_parse_value(JSON_Context *ctx);
function JSON_Array *json_parse_array(JSON_Context *ctx);
function JSON_Member *json_parse_member(JSON_Context *ctx, String8 key);
function JSON_Object *json_parse_object(JSON_Context *ctx);

//
// Core JSON API 
//

enum JSON_ValueKind {
  JSON_ValueKind_Error, 
  JSON_ValueKind_Object,
  JSON_ValueKind_String,
  JSON_ValueKind_Number,
  JSON_ValueKind_Boolean,
  JSON_ValueKind_Array, 
};

struct JSON_Value {
  JSON_ValueKind kind; 
  union {
    JSON_Object *obj;
    JSON_Array *array;
    String8 str;
    F32 number; 
    B32 boolean;
  }v;
};

struct JSON_ValueNode {
  JSON_ValueNode *next; 
  JSON_Value v;
};

struct JSON_Array {
  JSON_ValueNode *first;
  JSON_ValueNode *last; 
  U32 count; 
};

struct JSON_Member {
  JSON_Member *next;
  String8 key; 
  JSON_Value value; 
}; 

struct JSON_MemberList {
  JSON_Member *first;
  JSON_Member *last;
};

struct JSON_Object {
  JSON_MemberList *members;
  U32 member_count;
};

function JSON_Context *json_ctx_alloc(void);
function void json_ctx_release(JSON_Context *ctx);

function JSON_Object *json_parse(JSON_Context *ctx, String8 str);
function JSON_Member *json_member_from_object(JSON_Object *obj, String8 member_name);