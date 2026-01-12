function Arena *
arena_alloc(U64 size) 
{
  // Round up the allocation size to ensure it's a multiple
  // of the reserve granularity. 
  size = AlignPow2(size, ARENA_RESERVE_GRANULARITY);
  
  void *memory = os_reserve(size);
  U64 upfront_commit_size = ARENA_COMMIT_GRANULARITY;
  Assert(upfront_commit_size >= sizeof(Arena));
  os_commit(memory, upfront_commit_size);
  
  // Initialize arena members so that the arena can be used to 
  // access the committed memory.
  Arena *arena = (Arena *)memory;
  if (arena) {
    arena->pos = ARENA_HEADER_SIZE;
    arena->commit_pos = upfront_commit_size;
    arena->align = ARENA_DEFAULT_ALIGNMENT;
    arena->reserve_size = size;
    AsanPoisonMemoryRegion((void*)arena->pos, arena->reserve_size);
  }
  
  return arena;
}

function Arena *
arena_alloc_default(void) 
{
  return arena_alloc(ARENA_RESERVE_GRANULARITY);
}

function void 
arena_release(Arena *arena) 
{
  os_release(arena);
}

function void *
arena_push_nozero(Arena *arena, U64 size) 
{
  void *memory = 0;
  U64 pos_aligned = AlignPow2(arena->pos, arena->align);
  
  if (pos_aligned + size <= arena->reserve_size) {
    U8 *base = (U8 *)arena;
    
    U64 align_offset = pos_aligned - arena->pos;
    memory = base + arena->pos + align_offset;
    arena->pos += size + align_offset;
    
    if (arena->pos > arena->commit_pos) {
      U64 commit_size = arena->pos - arena->commit_pos;
      commit_size = AlignPow2(commit_size, ARENA_COMMIT_GRANULARITY);
      os_commit(base + arena->commit_pos, commit_size);
      arena->commit_pos += commit_size;
      
      AsanUnpoisonMemoryRegion((void*)pos_aligned, size + align_offset);
    }
  } 
  else {
    os_exit_process(1);
  }
  
  return memory;
}

function void *
arena_push(Arena *arena, U64 size) 
{
  void *memory = arena_push_nozero(arena, size);
  MemoryZero(memory, size);
  return memory;
}

function void 
arena_pop_to(Arena *arena, U64 target) 
{
  U64 pos_init = arena->pos; 
  (void)pos_init; // Temp
  
  // If the target position is greater than the minimum, it can be used.
  U64 pos_min = ARENA_HEADER_SIZE;
  U64 target_new = pos_min;
  if (target > pos_min) target_new = target; 
  arena->pos = target_new;
  
  AsanPoisonMemoryRegion((void*)target_new, pos_init - target_new);
  
  // The new current position aligned to the size of a commit block.
  U64 pos_commit_block_aligned = AlignPow2(arena->pos, ARENA_COMMIT_GRANULARITY);
  
  // We don't want to decommit on every pop. Only when our commit position
  // passes a particular threshold do we actually decommit memory during a pop,
  // since at that threshold the arena might be taking up too much memory.
  if (arena->commit_pos >= pos_commit_block_aligned + ARENA_DECOMMIT_THRESHOLD) {
    U8 *base = (U8 *)arena;
    U64 decommit_size = arena->commit_pos - pos_commit_block_aligned;
    os_decommit(base + pos_commit_block_aligned, decommit_size);
    arena->commit_pos -= decommit_size;
    
  }
}

function void 
arena_pop(Arena *arena, U64 size)
{
  // Make sure the pop size isn't larger than our current position.
  U64 pop_size = size;
  if (size > arena->pos) pop_size = arena->pos;
  
  // Make sure the target position is after the arena's header. 
  U64 pos_min = ARENA_HEADER_SIZE;
  U64 target = arena->pos - pop_size;
  if (target < pos_min) target = pos_min;
  
  arena_pop_to(arena, target);
}

function void 
arena_clear(Arena *arena) 
{
  arena_pop_to(arena, ARENA_HEADER_SIZE);
}

function void 
arena_set_align(Arena *arena, U64 align)
{
  arena->align = align;
}

//
// Temporary arenas
//

function TempArena 
arena_temp_begin(Arena *arena)
{
  TempArena temp = {0};
  temp.arena = arena;
  temp.initial_pos = arena->pos;
  return temp;
}

function void
arena_temp_end(TempArena temp)
{
  arena_pop_to(temp.arena, temp.initial_pos);
}

//
// Scratch Arenas
//

function Arena *
arena_get_scratch(Arena **conflict_array, U32 count)
{
  Arena *result = 0;
  
  // Initialize scratch pool on first use
  if (!scratch_arena_pool[0]) {
    for (U32 idx = 0; idx < ARENA_SCRATCH_POOL_COUNT; idx += 1) {
      scratch_arena_pool[idx] = arena_alloc_default();
    }
  }
  
  // Find first non-conflicting scratch arena
  for (U32 pool_idx = 0; pool_idx < ARENA_SCRATCH_POOL_COUNT; pool_idx += 1) {
    B32 scratch_conflict = 0;
    Arena *scratch_slot = scratch_arena_pool[pool_idx];
    
    for (U32 conflict_idx = 0; conflict_idx < count; conflict_idx += 1) {
      Arena *conflict_slot = conflict_array[conflict_idx];
      if (scratch_slot == conflict_slot) {
        scratch_conflict = 1;
        break;
      }
    }
    
    if (!scratch_conflict) {
      result = scratch_slot;
      break;
    }
  }
  
  return result;
}

function TempArena
arena_scratch_begin(Arena **conflict_array, U32 count)
{
  Arena *arena = arena_get_scratch(conflict_array, count);
  TempArena tmp = arena_temp_begin(arena);
  return tmp;
}

function void
arena_scratch_end(TempArena scratch)
{
  arena_temp_end(scratch);
}
