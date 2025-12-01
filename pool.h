/*
 * pool.h , this ia a single header fixed size pool allocator
 *
 * this is fixed size slot allocator with o(1) allocation and free
 * with an embedded free list stored directly in free slots for zero overhead
 *
 * this is not thread safe, if used across threads, you must provide
 * your own external synchronization (mutex, spinlock, etc.).
 *
 * notes on release mode:
 * double free in release mode (without POOL_DEBUG) corrupts the free list
 * and leads to undefined behavior (infinite loops, crashes), always test
 * with POOL_DEBUG enabled during developmenet
 *
 * OPTIONS :
 *   #define POOL_STATIC
 *     make all functions static (for including in multiple translation units).
 *
 *   #define POOL_DEBUG
 *     enable debug tooling, double free detection, use after free
 *     detection, leak reporting, statistics, and source location trackin,.
 *
 *   #define POOL_ASSERT(x)
 *     custom assert macro, defaults to standard assert().
 *
 *   #define POOL_MEMSET
 *     custom memset function, defaults to standard memset().
 *
 *   #define POOL_MEMCPY
 *     custom memcpy function, defaults to standard memcpy().
 *
 *   #define POOL_ZERO_ON_ALLOC
 *     zero memory when allocating a slot.
 *
 *   #define POOL_ZERO_ON_FREE
 *     zero memory when freeing a slot (security feature).
 *
 *   #define POOL_ALIGN n
 *     minimum alignment for slots, defaults to sizeof(void*).
 *     set to 16 for sse, 32 for avx, etc.
 *
 * SMALL EXAMPLE:
 *   #define POOL_IMPLEMENTATION
 *   #include "pool.h"
 *
 *   int main(void) {
 *       // allocate buffer for pool
 *       #define SLOT_SIZE 64
 *       #define SLOT_COUNT 100
 *       uint8_t buffer[SLOT_SIZE * SLOT_COUNT + 64]; // extra for alignment
 *
 *       pool_t pool;
 *       int err = pool_init(&pool, buffer, sizeof(buffer), SLOT_SIZE);
 *       if (err != POOL_OK) {
 *           fprintf(stderr, "pool init failed: %s\n", pool_error_string(err));
 *           return 1;
 *       }
 *
 *       // allocate slots
 *       void *slot1 = pool_alloc(&pool);
 *       void *slot2 = pool_alloc(&pool);
 *
 *       // use slots however you want...
 *
 *       // free slots
 *       pool_free(&pool, slot1);
 *       pool_free(&pool, slot2);
 *
 *       pool_destroy(&pool);
 *       return 0;
 *   }
 *
 */


#ifndef POOL_H_INCLUDED
#define POOL_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef POOL_STATIC
    #define POOL_API static
#else
    #define POOL_API extern
#endif

#ifndef POOL_ALIGN
    #define POOL_ALIGN sizeof(void*)
#endif

typedef enum pool_error {
    POOL_OK = 0,
    POOL_ERR_NULL_POOL,
    POOL_ERR_NULL_BUFFER,
    POOL_ERR_BUFFER_TOO_SMALL,
    POOL_ERR_INVALID_SLOT_SIZE,
    POOL_ERR_INVALID_ALIGNMENT,
    POOL_ERR_NULL_PTR,
    POOL_ERR_INVALID_PTR,
    POOL_ERR_DOUBLE_FREE,
    POOL_ERR_COUNT
} pool_error_t;

typedef struct pool_stats {
    size_t slot_size;
    size_t slot_count;
    size_t free_count;
    size_t used_count;
#ifdef POOL_DEBUG
    size_t total_allocs;
    size_t total_frees;
    size_t peak_used;
#endif
} pool_stats_t;

typedef struct pool {
    uint8_t *buffer;
    uint8_t *buffer_end;
    void    *free_list;
    size_t   slot_size;
    size_t   slot_count;
    size_t   free_count;

#ifdef POOL_DEBUG
    uint8_t *alloc_bitmap;
    size_t   bitmap_size;
    size_t   total_allocs;
    size_t   total_frees;
    size_t   peak_used;
    uint8_t *user_buffer;
    size_t   user_buffer_size;
#endif
} pool_t;

// initializes pool using provided buffer. size is total bytes, returns error if too small.
POOL_API int pool_init(pool_t *pool, void *buffer, size_t size, size_t slot_size);

// checks leaks in debug mode. does not free the user provided buffer.
POOL_API void pool_destroy(pool_t *pool);

// allocates a slot. returns null if exhausted.
POOL_API void *pool_alloc(pool_t *pool);

// returns slot to pool. ptr must be owned by pool.
POOL_API int pool_free(pool_t *pool, void *ptr);

// invalidates all allocations and resets free list.
POOL_API void pool_reset(pool_t *pool);

// returns non zero if no slots available.
POOL_API int pool_is_full(const pool_t *pool);

// returns non-zero if all slots available.
POOL_API int pool_is_empty(const pool_t *pool);

// gets effective slot size including alignment padding.
POOL_API size_t pool_slot_size(const pool_t *pool);

// gets total slot count.
POOL_API size_t pool_capacity(const pool_t *pool);

// gets count of free slots.
POOL_API size_t pool_available(const pool_t *pool);

// gets count of allocated slots.
POOL_API size_t pool_used(const pool_t *pool);

// verifies ptr is within pool memory range and correctly aligned to a slot.
POOL_API int pool_owns(const pool_t *pool, const void *ptr);

// populates stats structure.
POOL_API void pool_stats(const pool_t *pool, pool_stats_t *stats);

// converts error code to static string.
POOL_API const char *pool_error_string(int error);

// calculates buffer size required for init including alignment overhead.
POOL_API size_t pool_required_size(size_t slot_size, size_t slot_count);

#ifdef POOL_DEBUG

POOL_API void *pool_alloc_debug(pool_t *pool, const char *file, int line);
POOL_API int pool_free_debug(pool_t *pool, void *ptr, const char *file, int line);
// checks if specific pointer is currently marked allocated.
POOL_API int pool_is_allocated(const pool_t *pool, const void *ptr);

#define POOL_ALLOC(pool) pool_alloc_debug((pool), __FILE__, __LINE__)
#define POOL_FREE(pool, ptr) pool_free_debug((pool), (ptr), __FILE__, __LINE__)

#else

#define POOL_ALLOC(pool) pool_alloc(pool)
#define POOL_FREE(pool, ptr) pool_free((pool), (ptr))

#endif

#ifdef __cplusplus
}
#endif

#endif

#ifdef POOL_IMPLEMENTATION

#ifndef POOL_ASSERT
    #include <assert.h>
    #define POOL_ASSERT(x) assert(x)
#endif

#ifndef POOL_MEMSET
    #include <string.h>
    #define POOL_MEMSET memset
#endif

#ifndef POOL_MEMCPY
    #include <string.h>
    #define POOL_MEMCPY memcpy
#endif

#ifdef POOL_DEBUG_PRINTF
    #include <stdio.h>
    #define POOL_DBG_PRINTF(...) fprintf(stderr, __VA_ARGS__)
#else
    #define POOL_DBG_PRINTF(...) ((void)0)
#endif

#ifdef POOL_DEBUG
#define POOL_MAGIC_FREE     ((uintptr_t)0xDEADC0DEDEADC0DEULL)
#define POOL_POISON_BYTE    0xFE
#endif

static size_t pool__align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint8_t *pool__align_ptr(uint8_t *ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (uint8_t *)aligned;
}

#ifdef POOL_DEBUG
static size_t pool__slot_index(const pool_t *pool, const void *ptr) {
    return (size_t)((const uint8_t *)ptr - pool->buffer) / pool->slot_size;
}
#endif

static void *pool__slot_ptr(const pool_t *pool, size_t index) {
    return pool->buffer + index * pool->slot_size;
}

#ifdef POOL_DEBUG

static void pool__bitmap_set(pool_t *pool, size_t index) {
    size_t byte_idx = index / 8;
    size_t bit_idx = index % 8;
    pool->alloc_bitmap[byte_idx] |= (uint8_t)(1 << bit_idx);
}

static void pool__bitmap_clear(pool_t *pool, size_t index) {
    size_t byte_idx = index / 8;
    size_t bit_idx = index % 8;
    pool->alloc_bitmap[byte_idx] &= (uint8_t)~(1 << bit_idx);
}

static int pool__bitmap_get(const pool_t *pool, size_t index) {
    size_t byte_idx = index / 8;
    size_t bit_idx = index % 8;
    return (pool->alloc_bitmap[byte_idx] >> bit_idx) & 1;
}

static int pool__has_free_magic(const void *slot) {
    uintptr_t value;
    POOL_MEMCPY(&value, slot, sizeof(uintptr_t));
    return value == POOL_MAGIC_FREE;
}

static void pool__write_free_magic(void *slot) {
    uintptr_t magic = POOL_MAGIC_FREE;
    POOL_MEMCPY(slot, &magic, sizeof(uintptr_t));
}

static void pool__poison_slot(pool_t *pool, void *slot) {
    // preserve next pointer, poison rest
    size_t offset = sizeof(void *);
    if (pool->slot_size > offset) {
        POOL_MEMSET((uint8_t *)slot + offset, POOL_POISON_BYTE,
                    pool->slot_size - offset);
    }
}

#endif

static void pool__build_free_list(pool_t *pool) {
    pool->free_list = NULL;

    // build backwards so slot 0 is head (cache locality)
    for (size_t i = pool->slot_count; i > 0; i--) {
        void *slot = pool__slot_ptr(pool, i - 1);

        // store next pointer inside the slot itself
        void **next_ptr = (void **)slot;
        *next_ptr = pool->free_list;

        pool->free_list = slot;

#ifdef POOL_DEBUG
        // write magic after next pointer
        if (pool->slot_size >= sizeof(void *) + sizeof(uintptr_t)) {
            pool__write_free_magic((uint8_t *)slot + sizeof(void *));
        }
        pool__poison_slot(pool, slot);
        pool__bitmap_clear(pool, i - 1);
#endif
    }

    pool->free_count = pool->slot_count;
}

POOL_API int pool_init(pool_t *pool, void *buffer, size_t size, size_t slot_size) {
    if (pool == NULL) return POOL_ERR_NULL_POOL;
    if (buffer == NULL) return POOL_ERR_NULL_BUFFER;
    if (slot_size == 0) return POOL_ERR_INVALID_SLOT_SIZE;

    // check power of two
    if ((POOL_ALIGN & (POOL_ALIGN - 1)) != 0) {
        return POOL_ERR_INVALID_ALIGNMENT;
    }

    POOL_MEMSET(pool, 0, sizeof(pool_t));

    // ensure slot fits a pointer
    size_t effective_slot_size = slot_size;
    if (effective_slot_size < sizeof(void *)) {
        effective_slot_size = sizeof(void *);
    }
    effective_slot_size = pool__align_up(effective_slot_size, POOL_ALIGN);

#ifdef POOL_DEBUG
    size_t min_debug_size = sizeof(void *) + sizeof(uintptr_t);
    if (effective_slot_size < min_debug_size) {
        effective_slot_size = pool__align_up(min_debug_size, POOL_ALIGN);
    }
#endif

    uint8_t *aligned_start = pool__align_ptr((uint8_t *)buffer, POOL_ALIGN);
    size_t alignment_overhead = (size_t)(aligned_start - (uint8_t *)buffer);

    if (alignment_overhead >= size) {
        return POOL_ERR_BUFFER_TOO_SMALL;
    }

    size_t usable_size = size - alignment_overhead;

#ifdef POOL_DEBUG
    // reserve end of buffer for debug bitmap
    size_t temp_slot_count = usable_size / effective_slot_size;
    size_t bitmap_size = (temp_slot_count + 7) / 8;
    bitmap_size = pool__align_up(bitmap_size, POOL_ALIGN);

    if (usable_size <= bitmap_size) {
        return POOL_ERR_BUFFER_TOO_SMALL;
    }

    usable_size -= bitmap_size;
#endif

    size_t slot_count = usable_size / effective_slot_size;
    if (slot_count == 0) {
        return POOL_ERR_BUFFER_TOO_SMALL;
    }

    pool->buffer = aligned_start;
    pool->buffer_end = aligned_start + slot_count * effective_slot_size;
    pool->slot_size = effective_slot_size;
    pool->slot_count = slot_count;

#ifdef POOL_DEBUG
    pool->alloc_bitmap = pool->buffer_end;
    pool->bitmap_size = bitmap_size;
    POOL_MEMSET(pool->alloc_bitmap, 0, bitmap_size);
    pool->user_buffer = (uint8_t *)buffer;
    pool->user_buffer_size = size;
#endif

    pool__build_free_list(pool);

    return POOL_OK;
}

POOL_API void pool_destroy(pool_t *pool) {
    if (pool == NULL) return;

#ifdef POOL_DEBUG
    size_t leaked = pool->slot_count - pool->free_count;
    if (leaked > 0) {
        POOL_DBG_PRINTF("POOL: Memory leak detected! %zu slots not freed.\n", leaked);

        int first = 1;
        POOL_DBG_PRINTF("POOL: Leaked slot indices: ");
        for (size_t i = 0; i < pool->slot_count; i++) {
            if (pool__bitmap_get(pool, i)) {
                if (!first) POOL_DBG_PRINTF(", ");
                POOL_DBG_PRINTF("%zu", i);
                first = 0;
            }
        }
        POOL_DBG_PRINTF("\n");
        POOL_ASSERT(leaked == 0 && "Pool destroyed with leaked allocations");
    }
#endif

    POOL_MEMSET(pool, 0, sizeof(pool_t));
}

POOL_API void *pool_alloc(pool_t *pool) {
    if (pool == NULL) return NULL;

    if (pool->free_list == NULL) {
        return NULL;
    }

    // pop from free list
    void *slot = pool->free_list;
    void **next_ptr = (void **)slot;
    pool->free_list = *next_ptr;
    pool->free_count--;

#ifdef POOL_DEBUG
    size_t index = pool__slot_index(pool, slot);
    POOL_ASSERT(!pool__bitmap_get(pool, index) && "Allocating already-allocated slot");
    pool__bitmap_set(pool, index);

    pool->total_allocs++;
    size_t used = pool->slot_count - pool->free_count;
    if (used > pool->peak_used) pool->peak_used = used;
#endif

#ifdef POOL_ZERO_ON_ALLOC
    POOL_MEMSET(slot, 0, pool->slot_size);
#endif

    return slot;
}

POOL_API int pool_free(pool_t *pool, void *ptr) {
    if (pool == NULL) return POOL_ERR_NULL_POOL;
    if (ptr == NULL) return POOL_ERR_NULL_PTR;

    if (!pool_owns(pool, ptr)) {
#ifdef POOL_DEBUG
        POOL_DBG_PRINTF("POOL: Invalid free - pointer %p not owned by pool\n", ptr);
        POOL_ASSERT(0 && "Freeing pointer not owned by pool");
#endif
        return POOL_ERR_INVALID_PTR;
    }

#ifdef POOL_DEBUG
    size_t index = pool__slot_index(pool, ptr);

    // check double free via bitmap
    if (!pool__bitmap_get(pool, index)) {
        POOL_DBG_PRINTF("POOL: Double free detected at slot %zu (ptr=%p)\n", index, ptr);
        POOL_ASSERT(0 && "Double free detected");
        return POOL_ERR_DOUBLE_FREE;
    }

    // check double free via magic
    if (pool->slot_size >= sizeof(void *) + sizeof(uintptr_t)) {
        if (pool__has_free_magic((uint8_t *)ptr + sizeof(void *))) {
            POOL_DBG_PRINTF("POOL: Double free detected via magic at slot %zu\n", index);
            POOL_ASSERT(0 && "Double free detected via magic number");
            return POOL_ERR_DOUBLE_FREE;
        }
    }

    pool__bitmap_clear(pool, index);
    pool->total_frees++;
#endif

#ifdef POOL_ZERO_ON_FREE
    POOL_MEMSET(ptr, 0, pool->slot_size);
#endif

    // push to free list
    void **next_ptr = (void **)ptr;
    *next_ptr = pool->free_list;
    pool->free_list = ptr;
    pool->free_count++;

#ifdef POOL_DEBUG
    if (pool->slot_size >= sizeof(void *) + sizeof(uintptr_t)) {
#ifndef POOL_ZERO_ON_FREE
        pool__write_free_magic((uint8_t *)ptr + sizeof(void *));
        pool__poison_slot(pool, ptr);
#endif
    }
#endif

    return POOL_OK;
}

POOL_API void pool_reset(pool_t *pool) {
    if (pool == NULL) return;

#ifdef POOL_ZERO_ON_FREE
    size_t total_size = pool->slot_count * pool->slot_size;
    POOL_MEMSET(pool->buffer, 0, total_size);
#endif

#ifdef POOL_DEBUG
    POOL_MEMSET(pool->alloc_bitmap, 0, pool->bitmap_size);
    pool->total_allocs = 0;
    pool->total_frees = 0;
    pool->peak_used = 0;
#endif

    pool__build_free_list(pool);
}

POOL_API int pool_is_full(const pool_t *pool) {
    if (pool == NULL) return 1;
    return pool->free_count == 0;
}

POOL_API int pool_is_empty(const pool_t *pool) {
    if (pool == NULL) return 1;
    return pool->free_count == pool->slot_count;
}

POOL_API size_t pool_slot_size(const pool_t *pool) {
    if (pool == NULL) return 0;
    return pool->slot_size;
}

POOL_API size_t pool_capacity(const pool_t *pool) {
    if (pool == NULL) return 0;
    return pool->slot_count;
}

POOL_API size_t pool_available(const pool_t *pool) {
    if (pool == NULL) return 0;
    return pool->free_count;
}

POOL_API size_t pool_used(const pool_t *pool) {
    if (pool == NULL) return 0;
    return pool->slot_count - pool->free_count;
}

POOL_API int pool_owns(const pool_t *pool, const void *ptr) {
    if (pool == NULL || ptr == NULL) return 0;

    const uint8_t *p = (const uint8_t *)ptr;

    if (p < pool->buffer || p >= pool->buffer_end) return 0;

    // check alignment offset from start
    size_t offset = (size_t)(p - pool->buffer);
    if (offset % pool->slot_size != 0) return 0;

    return 1;
}

POOL_API void pool_stats(const pool_t *pool, pool_stats_t *stats) {
    if (stats == NULL) return;
    POOL_MEMSET(stats, 0, sizeof(pool_stats_t));

    if (pool == NULL) return;

    stats->slot_size = pool->slot_size;
    stats->slot_count = pool->slot_count;
    stats->free_count = pool->free_count;
    stats->used_count = pool->slot_count - pool->free_count;

#ifdef POOL_DEBUG
    stats->total_allocs = pool->total_allocs;
    stats->total_frees = pool->total_frees;
    stats->peak_used = pool->peak_used;
#endif
}

POOL_API const char *pool_error_string(int error) {
    switch ((pool_error_t)error) {
        case POOL_OK:                   return "Success";
        case POOL_ERR_NULL_POOL:        return "Pool pointer is NULL";
        case POOL_ERR_NULL_BUFFER:      return "Buffer pointer is NULL";
        case POOL_ERR_BUFFER_TOO_SMALL: return "Buffer too small for even one slot";
        case POOL_ERR_INVALID_SLOT_SIZE:return "Slot size is invalid (zero)";
        case POOL_ERR_INVALID_ALIGNMENT:return "POOL_ALIGN is not a power of two";
        case POOL_ERR_NULL_PTR:         return "Pointer argument is NULL";
        case POOL_ERR_INVALID_PTR:      return "Pointer not owned by pool";
        case POOL_ERR_DOUBLE_FREE:      return "Double free detected";
        case POOL_ERR_COUNT:            break;
    }
    return "Unknown error";
}

POOL_API size_t pool_required_size(size_t slot_size, size_t slot_count) {
    if (slot_size == 0 || slot_count == 0) return 0;

    size_t effective = slot_size;
    if (effective < sizeof(void *)) effective = sizeof(void *);
    effective = pool__align_up(effective, POOL_ALIGN);

#ifdef POOL_DEBUG
    size_t min_debug_size = sizeof(void *) + sizeof(uintptr_t);
    if (effective < min_debug_size) {
        effective = pool__align_up(min_debug_size, POOL_ALIGN);
    }
    size_t bitmap = pool__align_up((slot_count + 7) / 8, POOL_ALIGN);
#else
    size_t bitmap = 0;
#endif

    return slot_count * effective + bitmap + POOL_ALIGN - 1;
}

#ifdef POOL_DEBUG

POOL_API void *pool_alloc_debug(pool_t *pool, const char *file, int line) {
    void *ptr = pool_alloc(pool);
    if (ptr == NULL && pool != NULL && pool->free_count == 0) {
        POOL_DBG_PRINTF("POOL: Allocation failed (pool exhausted) at %s:%d\n", file, line);
    }
    (void)file; (void)line;
    return ptr;
}

POOL_API int pool_free_debug(pool_t *pool, void *ptr, const char *file, int line) {
    int result = pool_free(pool, ptr);
    if (result != POOL_OK) {
        POOL_DBG_PRINTF("POOL: Free failed (%s) at %s:%d, ptr=%p\n",
                        pool_error_string(result), file, line, ptr);
    }
    (void)file; (void)line;
    return result;
}

POOL_API int pool_is_allocated(const pool_t *pool, const void *ptr) {
    if (pool == NULL || ptr == NULL) return 0;
    if (!pool_owns(pool, ptr)) return 0;

    size_t index = pool__slot_index(pool, ptr);
    return pool__bitmap_get(pool, index);
}

#endif // POOL_DEBUG

#endif // POOL_IMPLEMENTATION