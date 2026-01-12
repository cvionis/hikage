//
// JSON Parsing API
//

function B32 
json_parsing(JSON_Context *ctx)
{
  return !ctx->error;
}

function void 
json_error(JSON_Context *ctx, JSON_Token token, String8 msg)
{
  (void)msg;
  (void)token;
  (void)ctx; 
}

function void 
json_eat_whitespace(JSON_Context *ctx) 
{
  for (;;) {
    if (is_end_of_line(*ctx->at)) {
      ctx->line += 1;
      ctx->at += 1;
    }
    if (is_whitespace(*ctx->at)) {
      ctx->at += 1; 
    }
    else if (*ctx->at == ';') {
      while (*ctx->at && !is_end_of_line(*ctx->at)) {
        ctx->at += 1; 
      }
    }
    else {
      break;
    }
  }
}

function void 
json_make_token_number(JSON_Context *ctx, JSON_Token *t)
{
  // TODO: Negative numbers
  JSON_Token token = {0}; 
  (void)token; 
  
  t->kind = JSON_Token_Number; 
  ctx->at -= 1;
  
  F32 n = 0.f;
  while (is_numeric(ctx->at[0])) {
    F32 digit = (F32)(ctx->at[0] - '0');
    n = n * 10.f + digit;
    ctx->at += 1;
  }
  if (ctx->at[0] == '.') {
    F32 c = 0.1f;
    ctx->at += 1;
    while (is_numeric(ctx->at[0])) {
      F32 digit = (F32)(ctx->at[0] - '0');
      n += c * digit;
      c *= 0.1f;
      ctx->at += 1;
    }
  }
  t->number = n; 
  t->text.count = ctx->at - t->text.data;
}

function void 
json_make_token_string(JSON_Context *ctx, JSON_Token *t)
{
  t->kind = JSON_Token_String;
  
  while (ctx->at[0] && ctx->at[0] != '"') {
    ctx->at += 1;
  }
  t->text.data += 1;
  t->text.count = ctx->at - t->text.data;
  ctx->at += 1;
}

function void 
json_make_token_boolean(JSON_Context *ctx, JSON_Token *t)
{
  ctx->at -= 1;
  // TODO: Making sure ctx->at[i] isn't nil
  if ((ctx->at[0] == 't') && 
      (ctx->at[1] == 'r') && 
      (ctx->at[2] == 'u') && 
      (ctx->at[3] == 'e')) 
  {
    t->kind = JSON_Token_True;
    t->text.count = 4;
    ctx->at += 4;
  }
  else if ((ctx->at[0] == 'f') && 
           (ctx->at[1] == 'a') && 
           (ctx->at[2] == 'l') && 
           (ctx->at[3] == 's') && 
           (ctx->at[4] == 'e')) 
  {
    t->kind = JSON_Token_False;
    t->text.count = 5;
    ctx->at += 5;
  }
}

function JSON_Token 
json_get_token(JSON_Context *ctx)
{
  json_eat_whitespace(ctx);
  
  JSON_Token token = {0};
  token.text = str8(ctx->at, 1);
  token.line = ctx->line;
  
  char c = *ctx->at;
  ctx->at += 1; 
  
  switch (c) {
    case '\0': { token.kind = JSON_Token_EOF;          } break;
    case ':':  { token.kind = JSON_Token_Colon;        } break;
    case ',':  { token.kind = JSON_Token_Comma;        } break;
    case '{':  { token.kind = JSON_Token_OpenBrace;    } break;
    case '}':  { token.kind = JSON_Token_CloseBrace;   } break;
    case '[':  { token.kind = JSON_Token_OpenBracket;  } break;
    case ']':  { token.kind = JSON_Token_CloseBracket; } break;
    case '"':  { json_make_token_string(ctx, &token); } break;
    default: {
      if (is_numeric(c)) {
        json_make_token_number(ctx, &token);
      }
      else if (is_alpha(c)) {
        json_make_token_boolean(ctx, &token);
      }
      else {
        token.kind = JSON_Token_Error;
      }
    }
  }
  return token;
}

function JSON_Token 
json_require_token(JSON_Context *ctx, JSON_TokenKind kind, String8 msg)
{
  JSON_Token token = json_get_token(ctx);
  if (token.kind != kind) { 
    json_error(ctx, token, msg);
  }
  return token;
}

function JSON_Array *
json_parse_array(JSON_Context *ctx)
{
  JSON_Array *array = ArenaPushStruct(ctx->arena, JSON_Array);
  // TODO: check if (value.kind != JSON_ValueKind_Null) -- since an empty array really isn't erroneous; it
  // just has a single null member.
  while (json_parsing(ctx)) {
    JSON_Value value = json_parse_value(ctx);
    if (value.kind != JSON_ValueKind_Error) {
      JSON_ValueNode *n = ArenaPushStruct(ctx->arena, JSON_ValueNode);
      n->v = value; 
      SLLQueuePush(array->first, array->last, n);
      array->count += 1; 
      
      JSON_Token token = json_get_token(ctx);
      if (token.kind == JSON_Token_Comma) {
        continue;
      }
      else if (token.kind == JSON_Token_CloseBracket) {
        break;
      }
      else {
        json_error(ctx, token, S8("JSON: Invalid token after value in array"));
      }
    }
    else {
      break;
    }
  }
  
  return array;
}

function JSON_Value 
json_parse_value(JSON_Context *ctx)
{
  JSON_Value value = {0}; 
  
  JSON_Token next = json_get_token(ctx); 
  switch (next.kind) {
    case JSON_Token_OpenBrace: {
      value.kind = JSON_ValueKind_Object;
      value.v.obj = json_parse_object(ctx);
    }break;
    case JSON_Token_OpenBracket: {
      value.kind = JSON_ValueKind_Array;
      value.v.array = json_parse_array(ctx);
    }break;
    case JSON_Token_String: {
      value.kind = JSON_ValueKind_String;
      value.v.str = next.text; 
    }break;
    case JSON_Token_Number: {
      value.kind = JSON_ValueKind_Number;
      value.v.number = next.number;
    }break;
    case JSON_Token_True: {
      value.kind = JSON_ValueKind_Boolean;
      value.v.boolean = 1;
    }break;
    case JSON_Token_False: {
      value.kind = JSON_ValueKind_Boolean;
      value.v.boolean = 0;
    }break;
    default: {
      value.kind = JSON_ValueKind_Error;
      json_error(ctx, next, S8("JSON: Unexpected value type in object member")); 
    }break;
  }
  return value; 
}

function JSON_Member *
json_parse_member(JSON_Context *ctx, String8 key)
{
  JSON_Member *member = ArenaPushStruct(ctx->arena, JSON_Member);
  member->key = key; 
  
  json_require_token(ctx, JSON_Token_Colon, S8("JSON: Missing ':' after object member's key")); 
  
  JSON_Value value = json_parse_value(ctx); 
  member->value = value;
  return member;
}

function JSON_Object *
json_parse_object(JSON_Context *ctx)
{
  JSON_Object *obj = ArenaPushStruct(ctx->arena, JSON_Object);
  obj->members = ArenaPushStruct(ctx->arena, JSON_MemberList);
  
  while (json_parsing(ctx)) {
    JSON_Token key = json_require_token(ctx, JSON_Token_String, S8("JSON: Attempting to parse object member with no name"));
    
    JSON_Member *member = json_parse_member(ctx, key.text);
    SLLQueuePush(obj->members->first, obj->members->last, member);
    obj->member_count += 1; 
    
    JSON_Token next = json_get_token(ctx);
    if (next.kind == JSON_Token_Comma) {
      continue;
    }
    else if (next.kind == JSON_Token_CloseBrace) {
      break;
    }
    else {
      json_error(ctx, next, S8("JSON: Unexpected character after object member"));
      break;
    }
  }
  return obj;
}

//
// Core JSON API 
//

function JSON_Context *
json_ctx_alloc(void)
{
  Arena *arena = arena_alloc_default();
  JSON_Context *ctx = ArenaPushStruct(arena, JSON_Context);
  ctx->arena = arena;
  return ctx;
}

function void 
json_ctx_release(JSON_Context *ctx)
{
  if (ctx) {
    arena_release(ctx->arena);
  }
}

function JSON_Object *
json_parse(JSON_Context *ctx, String8 str)
{
  JSON_Object *root = 0;
  
  if (str.count != 0) {
    ctx->at = str.data; 
    
    while (json_parsing(ctx)) {
      JSON_Token token = json_get_token(ctx); 
      if (token.kind == JSON_Token_OpenBrace) {
        root = json_parse_object(ctx); 
      }
      else if (token.kind == JSON_Token_EOF) {
        break;
      }
      else {
        json_error(ctx, token, S8("JSON: Unexpected token at root level"));
      }
    }
  }
  
  return root;
}

function JSON_Member *
json_member_from_object(JSON_Object *obj, String8 member_name)
{
  JSON_Member *member = 0;
  if (obj) {
    for (JSON_Member *m = obj->members->first; m != 0; m = m->next) {
      if (str8_equal(m->key, member_name)) {
        member = m;
        break;
      }
    }
  }
  return member;
}
