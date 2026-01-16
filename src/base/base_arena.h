#pragma once

#define ARENA_RESERVE_GRANULARITY MiB(64)
#define ARENA_COMMIT_GRANULARITY  KiB(4)
#define ARENA_DECOMMIT_THRESHOLD  KiB(512)
#define ARENA_DEFAULT_ALIGNMENT   8
#define ARENA_SCRATCH_POOL_COUNT  3

//
// Arenas
//

struct Arena {
  U64 pos;
  U64 commit_pos;
  U64 align;
  U64 reserve_size;
};

#define ARENA_HEADER_SIZE sizeof(Arena)

threadlocal Arena *scratch_arena_pool[ARENA_SCRATCH_POOL_COUNT] = {0};

static Arena *arena_alloc(U64 size);
static Arena *arena_alloc_default(void);
static void arena_release(Arena *arena);

static void *arena_push_nozero(Arena *arena, U64 size);
static void *arena_push(Arena *arena, U64 size);
static void *arena_push_align(Arena *arena, U64 alignment);

static void arena_pop(Arena *arena, U64 size);
static void arena_pop_to(Arena *arena, U64 pos);
static void arena_clear(Arena *arena);

static void arena_set_align(Arena *arena, U64 align);

#define ArenaPushStruct(arena, type) \
(type *)arena_push((arena), sizeof(type))
#define ArenaPushArray(arena, type, count) \
(type *)arena_push((arena), sizeof(type)*(count))
#define ArenaPushArrayNoZero(arena, type, count) \
(type *)arena_push_nozero((arena), sizeof(type)*(count))
#define ArenaPopStruct(arena, type) \
arena_pop((arena), sizeof(type))
#define ArenaPopArray(arena, type, count) \
arena_pop((arena), sizeof(type)*(count))

//
// Temporary arenas
//

struct TempArena {
  Arena *arena;
  U64 initial_pos;
};

static TempArena arena_temp_begin(Arena *arena);
static void arena_temp_end(TempArena temp);

//
// Scratch arenas
//

static Arena *arena_get_scratch(Arena **conflict_array, U32 count);
static TempArena arena_scratch_begin(Arena **conflict_array, U32 count);
static void arena_scratch_end(TempArena scratch);
