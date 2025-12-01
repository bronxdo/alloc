/*
 * stack.h - Single header lifo stack allocator library
 *
 * OPTIONS:
 *
 * CONFIGURATION MACROS (define before including):
 *
 *   STACK_IMPLEMENTATION   - Include the implementation
 *   STACK_STATIC           - Make all functions static (for single-file usage)
 *   STACK_DEBUG            - Enable debug features (LIFO validation, poisoning, etc.)
 *   STACK_ASSERT(x)        - Custom assert macro (default: assert(x))
 *   STACK_MEMSET(d,v,n)    - Custom memset (default: memset)
 *   STACK_MALLOC(n)        - Custom malloc for debug infrastructure (default: malloc)
 *   STACK_REALLOC(p,n)     - Custom realloc for debug infrastructure (default: realloc)
 *   STACK_FREE(p)          - Custom free for debug infrastructure (default: free)
 *   STACK_VALIDATE_LIFO    - (Debug only) Enforce strict LIFO free order
 *
 * EXAMPLE:
 *
 *     uint8_t buffer[4096];
 *     stack_t stack;
 *
 *     stack_init(&stack, buffer, sizeof(buffer));
 *
 *     void *a = stack_alloc(&stack, 100);
 *     void *b = stack_alloc(&stack, 200);
 *
 *     stack_marker_t mark = stack_save(&stack);
 *     void *c = stack_alloc(&stack, 50);
 *     stack_restore(&stack, mark);  // Frees c
 *
 *     stack_free(&stack, b);  // Must free in LIFO order
 *     stack_free(&stack, a);
 *
 *     stack_destroy(&stack);
 *
 * THE LIFO CONTRACT:
 *
 *   stack_free(ptr) only works correctly if ptr was the most recent allocation.
 *   In debug mode (STACK_DEBUG + STACK_VALIDATE_LIFO), this is enforced with asserts.
 *   In release mode, violating LIFO order is undefined behavior.
 *
 *
 *   This library is not thread safe, external synchronization is required for
 *   concurrent access.
 *
 */


/*
 * usage:
 *   #define STACK_IMPLEMENTATION
 *   #include "stack.h"
 */

#ifndef STACK_H
#define STACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STACK_ASSERT
    #include <assert.h>
    #define STACK_ASSERT(x) assert(x)
#endif

#ifndef STACK_MEMSET
    #include <string.h>
    #define STACK_MEMSET(dst, val, n) memset((dst), (val), (n))
#endif

#if defined(STACK_DEBUG) && !defined(STACK_MALLOC)
    #include <stdlib.h>
    #define STACK_MALLOC(n)       malloc(n)
    #define STACK_REALLOC(p, n)   realloc((p), (n))
    #define STACK_FREE(p)         free(p)
#endif

#ifdef STACK_STATIC
    #define STACK_DEF static
#else
    #define STACK_DEF extern
#endif

#define STACK_MIN_ALIGNMENT     (sizeof(size_t))
#define STACK_HEADER_SIZE       (sizeof(size_t))

#ifdef STACK_DEBUG
    #define STACK_DEBUG_INITIAL_CAPACITY  64
    #define STACK_POISON_VALUE  0xCD
#endif

typedef struct stack_t {
    uint8_t    *buffer;
    size_t      capacity;
    size_t      offset;

#ifdef STACK_DEBUG
    void      **alloc_stack;
    size_t      alloc_count;
    size_t      alloc_capacity;
    size_t      peak_usage;
#endif
} stack_t;

typedef struct stack_marker_t {
    size_t offset;
#ifdef STACK_DEBUG
    size_t alloc_count;
#endif
} stack_marker_t;

typedef struct stack_stats_t {
    size_t capacity;
    size_t used;
    size_t remaining;
#ifdef STACK_DEBUG
    size_t allocation_count;
    size_t peak_usage;
#endif
} stack_stats_t;

// initializes stack with buffer. caller retains ownership of buffer.
STACK_DEF int stack_init(stack_t *stack, void *buffer, size_t size);

// destroys stack resources. does not free backing buffer.
STACK_DEF void stack_destroy(stack_t *stack);

// allocates memory with default alignment.
STACK_DEF void *stack_alloc(stack_t *stack, size_t size);

// allocates memory with specified alignment.
STACK_DEF void *stack_alloc_aligned(stack_t *stack, size_t size, size_t alignment);

// frees most recent allocation. must follow lifo order.
STACK_DEF void stack_free(stack_t *stack, void *ptr);

// saves current stack state.
STACK_DEF stack_marker_t stack_save(stack_t *stack);

// restores stack to saved state, freeing subsequent allocations.
STACK_DEF void stack_restore(stack_t *stack, stack_marker_t marker);

// resets stack to initial state.
STACK_DEF void stack_reset(stack_t *stack);

// gets remaining capacity.
STACK_DEF size_t stack_remaining(const stack_t *stack);

// gets stack statistics.
STACK_DEF stack_stats_t stack_stats(const stack_t *stack);

// checks if pointer belongs to stack buffer.
STACK_DEF int stack_owns(const stack_t *stack, const void *ptr);

// allocates zero initialized memory.
STACK_DEF void *stack_calloc(stack_t *stack, size_t num, size_t size);

#define stack_init_buffer(stack_ptr, buff) \
    stack_init((stack_ptr), (buff), sizeof(buff))

#ifdef __cplusplus
}
#endif

#endif

#ifdef STACK_IMPLEMENTATION

static int stack__is_power_of_two(size_t n) {
    return n && !(n & (n - 1));
}

static size_t *stack__get_header(void *ptr) {
    return (size_t *)((uint8_t *)ptr - STACK_HEADER_SIZE);
}

#ifdef STACK_DEBUG
static int stack__grow_alloc_stack(stack_t *stack) {
    size_t new_capacity;
    void **new_array;

    new_capacity = stack->alloc_capacity * 2;
    if (new_capacity < STACK_DEBUG_INITIAL_CAPACITY) {
        new_capacity = STACK_DEBUG_INITIAL_CAPACITY;
    }

    new_array = (void **)STACK_REALLOC(stack->alloc_stack, new_capacity * sizeof(void *));
    if (!new_array) return -1;

    stack->alloc_stack = new_array;
    stack->alloc_capacity = new_capacity;
    return 0;
}

static int stack__debug_push(stack_t *stack, void *ptr) {
    if (stack->alloc_count >= stack->alloc_capacity) {
        if (stack__grow_alloc_stack(stack) != 0) return -1;
    }
    stack->alloc_stack[stack->alloc_count++] = ptr;
    return 0;
}

static void stack__debug_pop(stack_t *stack, void *ptr) {
    #ifdef STACK_VALIDATE_LIFO
    STACK_ASSERT(stack->alloc_count > 0 && "stack_free: no allocations to free");
    STACK_ASSERT(stack->alloc_stack[stack->alloc_count - 1] == ptr && "stack_free: lifo order violated");
    #else
    (void)ptr;
    if (stack->alloc_count == 0) return;
    #endif
    stack->alloc_count--;
}

static void stack__poison(void *ptr, size_t size) {
    STACK_MEMSET(ptr, STACK_POISON_VALUE, size);
}
#endif

STACK_DEF int stack_init(stack_t *stack, void *buffer, size_t size) {
    if (!stack || !buffer || size == 0) return -1;

    stack->buffer = (uint8_t *)buffer;
    stack->capacity = size;
    stack->offset = 0;

#ifdef STACK_DEBUG
    stack->alloc_stack = NULL;
    stack->alloc_count = 0;
    stack->alloc_capacity = 0;
    stack->peak_usage = 0;

    stack->alloc_stack = (void **)STACK_MALLOC(STACK_DEBUG_INITIAL_CAPACITY * sizeof(void *));
    if (!stack->alloc_stack) return -1;
    stack->alloc_capacity = STACK_DEBUG_INITIAL_CAPACITY;
#endif

    return 0;
}

STACK_DEF void stack_destroy(stack_t *stack) {
    if (!stack) return;

#ifdef STACK_DEBUG
    if (stack->alloc_stack) {
        STACK_FREE(stack->alloc_stack);
        stack->alloc_stack = NULL;
    }
    stack->alloc_count = 0;
    stack->alloc_capacity = 0;
#endif

    stack->buffer = NULL;
    stack->capacity = 0;
    stack->offset = 0;
}

STACK_DEF void *stack_alloc(stack_t *stack, size_t size) {
    return stack_alloc_aligned(stack, size, STACK_MIN_ALIGNMENT);
}

STACK_DEF void *stack_alloc_aligned(stack_t *stack, size_t size, size_t alignment) {
    size_t prev_offset;
    size_t user_offset;
    size_t end_offset;
    uintptr_t user_addr_unaligned;
    uintptr_t user_addr_aligned;
    uint8_t *user_ptr;

    STACK_ASSERT(stack != NULL);
    STACK_ASSERT(stack->buffer != NULL);

    if (size == 0) return NULL;

    if (alignment < STACK_MIN_ALIGNMENT) alignment = STACK_MIN_ALIGNMENT;
    STACK_ASSERT(stack__is_power_of_two(alignment) && "alignment must be power of two");

    prev_offset = stack->offset;

    user_addr_unaligned = (uintptr_t)(stack->buffer + prev_offset + STACK_HEADER_SIZE);
    user_addr_aligned = (user_addr_unaligned + alignment - 1) & ~(alignment - 1);
    user_offset = (size_t)(user_addr_aligned - (uintptr_t)stack->buffer);
    end_offset = user_offset + size;

    if (end_offset < user_offset || end_offset > stack->capacity) return NULL;

    user_ptr = stack->buffer + user_offset;
    *stack__get_header(user_ptr) = prev_offset;

    stack->offset = end_offset;

#ifdef STACK_DEBUG
    if (stack__debug_push(stack, user_ptr) != 0) {
        stack->offset = prev_offset;
        return NULL;
    }
    if (stack->offset > stack->peak_usage) {
        stack->peak_usage = stack->offset;
    }
#endif

    return user_ptr;
}

STACK_DEF void stack_free(stack_t *stack, void *ptr) {
    size_t prev_offset;
    uint8_t *user_ptr;

    STACK_ASSERT(stack != NULL);

    if (!ptr) return;

    user_ptr = (uint8_t *)ptr;

    STACK_ASSERT(user_ptr >= stack->buffer && user_ptr < stack->buffer + stack->capacity);

#ifdef STACK_DEBUG
    stack__debug_pop(stack, ptr);
#endif

    prev_offset = *stack__get_header(ptr);
    STACK_ASSERT(prev_offset <= stack->offset);

#ifdef STACK_DEBUG
    {
        size_t freed_size = stack->offset - prev_offset;
        stack__poison(stack->buffer + prev_offset, freed_size);
    }
#endif

    stack->offset = prev_offset;
}

STACK_DEF stack_marker_t stack_save(stack_t *stack) {
    stack_marker_t marker;
    STACK_ASSERT(stack != NULL);
    marker.offset = stack->offset;
#ifdef STACK_DEBUG
    marker.alloc_count = stack->alloc_count;
#endif
    return marker;
}

STACK_DEF void stack_restore(stack_t *stack, stack_marker_t marker) {
    STACK_ASSERT(stack != NULL);
    STACK_ASSERT(marker.offset <= stack->offset);
    STACK_ASSERT(marker.offset <= stack->capacity);

#ifdef STACK_DEBUG
    STACK_ASSERT(marker.alloc_count <= stack->alloc_count);
    if (stack->offset > marker.offset) {
        stack__poison(stack->buffer + marker.offset, stack->offset - marker.offset);
    }
    stack->alloc_count = marker.alloc_count;
#endif

    stack->offset = marker.offset;
}

STACK_DEF void stack_reset(stack_t *stack) {
    STACK_ASSERT(stack != NULL);

#ifdef STACK_DEBUG
    if (stack->offset > 0) {
        stack__poison(stack->buffer, stack->offset);
    }
    stack->alloc_count = 0;
#endif

    stack->offset = 0;
}

STACK_DEF size_t stack_remaining(const stack_t *stack) {
    STACK_ASSERT(stack != NULL);
    if (stack->offset >= stack->capacity) return 0;
    return stack->capacity - stack->offset;
}

STACK_DEF stack_stats_t stack_stats(const stack_t *stack) {
    stack_stats_t stats;
    STACK_ASSERT(stack != NULL);

    stats.capacity = stack->capacity;
    stats.used = stack->offset;
    stats.remaining = stack->capacity - stack->offset;

#ifdef STACK_DEBUG
    stats.allocation_count = stack->alloc_count;
    stats.peak_usage = stack->peak_usage;
#endif

    return stats;
}

STACK_DEF int stack_owns(const stack_t *stack, const void *ptr) {
    const uint8_t *p;
    STACK_ASSERT(stack != NULL);
    if (!ptr) return 0;
    p = (const uint8_t *)ptr;
    return (p >= stack->buffer && p < stack->buffer + stack->capacity);
}

STACK_DEF void *stack_calloc(stack_t *stack, size_t num, size_t size) {
    size_t total;
    void *ptr;

    if (size != 0 && num > (size_t)-1 / size) return NULL;

    total = num * size;
    ptr = stack_alloc(stack, total);

    if (ptr) {
        STACK_MEMSET(ptr, 0, total);
    }

    return ptr;
}

#endif // STACK_IMPLEMENTATION