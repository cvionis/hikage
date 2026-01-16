#pragma once

#include <stdint.h>

//
// Custom types and storage class aliases
//

typedef int8_t   S8;
typedef int16_t  S16;
typedef int32_t  S32;
typedef int64_t  S64;
typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef uint8_t  B8;
typedef uint16_t B16;
typedef uint32_t B32;
typedef float    F32;
typedef double   F64;

#define global   static

#if COMPILER_MSVC
# define threadlocal __declspec(thread)
#elif COMPILER_CLANG
# define threadlocal __thread
#elif COMPILER_GCC
# define threadlocal __thread
#endif

//
// General utility macros
//

#define Stringify_(x) #x
#define Stringify(x)  Stringify_(x)

#define Glue_(a, b) a##b
#define Glue(a, b) Glue_(a, b)

#define ArrayCount(a) (sizeof((a)) / sizeof(*(a)))

#define MemorySet(ptr,value,count) memset((ptr),(value),(count))
#define MemoryZero(ptr,size)       memset((ptr),0,(size))
#define MemoryZeroArray(ptr,count) memset(ptr, 0, sizeof(*ptr) * count)
#define MemoryZeroStruct(ptr)      memset(ptr, 0, sizeof(*ptr))

#define MemoryCopy(dst, src, count) memcpy((dst),(src),(count))
#define MemoryCopyStruct(dst, src)  MemoryCopy((dst),(src),sizeof((*dst)));

#define MemoryMove(dst, src, size) memmove((dst),(src),(size))

#define IntFromPtr(p) (unsigned long long)((char*)p - (char*)0)
#define PtrFromInt(p) (void*)((char*)0 + (p))

#define Member(T, m) (((T*)0)->m) // Can be used to abstractly refer to a structs's member; can't be read or written to.
#define OffsetOf(T, m) IntFromPtr(&Member(T,m))

#define KiB(n) ((n) << 10)
#define MiB(n) ((n) << 20)
#define GiB(n) ((n) << 30)

#define AlignPow2(size, pow2) (((size) + (pow2) - 1) & ~((pow2) - 1))

//
// Doubly- and singly-linked list operations
//

// Note on names and wording used:
//  f: first, l: last, n: node
//  SLL Queue: singly-linked list built from 'first', 'last' pointers.
//  SLL Stack: singly-linked list built from a single 'first' pointer.

#define DLLPushBackNP(f,l,n,next,prev) ((f)==0?\
((f)=(l)=(n), (n)->next=(n)->prev=0):\
((n)->prev=(l),(l)->next=(n),(l)=(n),(n)->next=0))
#define DLLPushBack(f,l,n) DLLPushBackNP(f,l,n,next,prev)
#define DLLPushFront(l,f,n) DLLPushBackNP(l,f,prev,next)

#define DLLRemoveNP(f,l,n,next,prev) ((f)==(n)?\
((f)==(l)?\
((f)=(l)=(0)):\
((f)=(f)->next,(f)->prev=0)):\
(l)==(n)?\
((l)=(l)->prev,(l)->next=0):\
((n)->next->prev=(n)->prev,\
(n)->prev->next=(n)->next))
#define DLLRemove(f,l,n) DLLRemoveNP(f,l,n,next,prev)

#define SLLQueuePushN(f,l,n,next) ((f)==0?\
(f)=(l)=(n):\
((l)->next=(n),(l)=(n)),\
(n)->next=0)
#define SLLQueuePush(f,l,n) SLLQueuePushN(f,l,n,next)

#define SLLQueuePushFrontN(f,l,n,next) ((f)==0?\
((f)=(l)=(n),(n)->next=0):\
((n)->next=(f),(f)=(n)))
#define SLLQueuePushFront(f,l,n) SLLQueuePushFront(f,l,n,next)

#define SLLQueuePopN(f,l,next) ((f)==(l)?\
(f)=(l)=0:\
((f)=(f)->next))
#define SLLQueuePop(f,l) SLLQueuePopN(f,l,next)

#define SLLStackPushN(f,n,next) ((f)==(0)?\
(f)=(n), (n)->next=0:\
((n)->next=(f),(f)=(n)))
#define SLLStackPush(f,n) SLLStackPushN(f,n,next)

#define SLLStackPopN(f,next) ((f)==(0)?0:((f)=(f)->next))
#define SLLStackPop(f) SLLStackPopN(f,next)

//
// Assertions
//

// NOTE: The purpose of the Statement() macro in Assert is to convert the if, which ends with a code-block,
// to something that requires a semicolon at the end (a statement). This makes the Assert() macro's usage more
// akin to a normal function call.
#define Statement(s) do{ s }while(0)

#if OS_WINDOWS
# define BreakDebugger() __debugbreak()           // Reduces to an INT 3 instruction
#else
# define BreakDebugger() (*(volatile int *)0 = 0) // Produces a SIGSEV and transfers control to debugger
#endif

#undef Assert
#if BUILD_DEBUG
# define Assert(c) Statement( if(!(c)) { BreakDebugger(); } )
#else
# define Assert(c) ((void)(c))
#endif

#define DeferLoop(begin, end) for(int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))

//
// Address sanitizer
//

void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);

#if ASAN_ENABLED
# define AsanPoisonMemoryRegion(base, size)   (__asan_poison_memory_region(base, size))
# define AsanUnpoisonMemoryRegion(base, size) (__asan_unpoison_memory_region(base, size))
#else
# define AsanPoisonMemoryRegion(base, size)
# define AsanUnpoisonMemoryRegion(base, size)
#endif
