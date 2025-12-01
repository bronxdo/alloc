/*
 * arena.h , a Single header arena (linear/bump) allocator for C
 * *
 * CONFIGURATION MACROS (define before including)
 *
 *   ARENA_IMPLEMENTATION   - Include implementation (define in ONE file only)
 *   ARENA_STATIC           - Make all functions static (private to compilation unit)
 *   ARENA_DEBUG            - Enable debug features (tracking, poisoning, canaries)
 *   ARENA_BLOCK_CHAINING   - Enable auto-growth via linked blocks
 *   ARENA_ASSERT(x)        - Custom assert (default: assert(x))
 *   ARENA_MEMSET           - Custom memset (default: memset)
 *   ARENA_MEMCPY           - Custom memcpy (default: memcpy)
 *   ARENA_MALLOC           - Custom malloc for block chaining (default: malloc)
 *   ARENA_FREE             - Custom free for block chaining (default: free)
 *   ARENA_DEFAULT_ALIGN    - Default alignment (default: alignof(max_align_t))
 *   ARENA_BLOCK_MIN_SIZE   - Minimum block size for chaining (default: 4096)
 *
 * EXAMPLE USAGE
 *
 *   // Stack-based arena (no malloc)
 *   uint8_t buffer[4096];
 *   arena_t arena;
 *   arena_init(&arena, buffer, sizeof(buffer));
 *
 *   int *numbers = arena_alloc(&arena, 100 * sizeof(int));
 *   char *name = arena_alloc(&arena, 256);
 *
 *   arena_reset(&arena);  // Reuse all memory
 *   arena_destroy(&arena);
 *
 *
 * This code is not thread safe, use one arena per thread, or add external synchronization
 *
 */

/*
 * arena.h - single-header arena (linear/bump) allocator
 * usage:
 *   #define ARENA_IMPLEMENTATION
 *   #include "arena.h"
 */

#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef ARENA_ASSERT
    #include <assert.h>
    #define ARENA_ASSERT(x) assert(x)
#endif

#ifndef ARENA_MEMSET
    #include <string.h>
    #define ARENA_MEMSET memset
#endif

#ifndef ARENA_MEMCPY
    #include <string.h>
    #define ARENA_MEMCPY memcpy
#endif

#ifdef ARENA_BLOCK_CHAINING
    #ifndef ARENA_BLOCK_MIN_SIZE
        #define ARENA_BLOCK_MIN_SIZE 4096
    #endif
#endif

#if defined(ARENA_BLOCK_CHAINING) || defined(ARENA_DEBUG)
    #ifndef ARENA_MALLOC
        #include <stdlib.h>
        #define ARENA_MALLOC malloc
    #endif
    #ifndef ARENA_FREE
        #include <stdlib.h>
        #define ARENA_FREE free
    #endif
#endif

#ifndef ARENA_DEFAULT_ALIGN
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #define ARENA_DEFAULT_ALIGN (sizeof(max_align_t))
    #else
        #define ARENA_DEFAULT_ALIGN (sizeof(long double) > sizeof(void*) ? sizeof(long double) : sizeof(void*))
    #endif
#endif

#ifndef ARENA_ALIGNOF
    #ifdef __cplusplus
        #define ARENA_ALIGNOF(T) alignof(T)
    #elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
        #define ARENA_ALIGNOF(T) _Alignof(T)
    #elif defined(__GNUC__) || defined(__clang__)
        #define ARENA_ALIGNOF(T) __alignof__(T)
    #elif defined(_MSC_VER)
        #define ARENA_ALIGNOF(T) __alignof(T)
    #else
        #define ARENA_ALIGNOF(T) (sizeof(T) < sizeof(void*) ? sizeof(T) : sizeof(void*))
    #endif
#endif

#ifdef ARENA_STATIC
    #define ARENA_API static
#else
    #define ARENA_API extern
#endif

#ifdef ARENA_DEBUG
    #define ARENA_POISON_FREED      0xFE
    #define ARENA_POISON_UNINIT     0xCD

    #ifndef ARENA_MAX_TRACKED_ALLOCS
        #define ARENA_MAX_TRACKED_ALLOCS 1024
    #endif

    #ifndef ARENA_PRINTF
        #include <stdio.h>
        #define ARENA_PRINTF(...) fprintf(stderr, __VA_ARGS__)
    #endif
#endif

typedef struct arena_t arena_t;
typedef struct arena_marker_t arena_marker_t;
typedef struct arena_stats_t arena_stats_t;

#ifdef ARENA_BLOCK_CHAINING
typedef struct arena_block_t arena_block_t;
#endif

#ifdef ARENA_DEBUG
typedef struct arena_alloc_record_t arena_alloc_record_t;
#endif

#ifdef ARENA_BLOCK_CHAINING
struct arena_block_t {
    uint8_t         *buffer;
    size_t           capacity;
    size_t           offset;
    arena_block_t   *next;
    arena_block_t   *prev;
    bool             owned;
};
#endif

#ifdef ARENA_DEBUG
struct arena_alloc_record_t {
    void        *ptr;
    size_t       size;
    size_t       actual_size;
    const char  *file;
    int          line;
    uint32_t     sequence;
};
#endif

struct arena_stats_t {
    size_t   capacity;
    size_t   used;
    size_t   remaining;
    size_t   alloc_count;
    size_t   total_requested;
    size_t   peak_usage;
    size_t   wasted_alignment;
#ifdef ARENA_BLOCK_CHAINING
    size_t   block_count;
    size_t   total_capacity;
#endif
};

struct arena_marker_t {
    size_t   offset;
#ifdef ARENA_BLOCK_CHAINING
    arena_block_t *block;
#endif
#ifdef ARENA_DEBUG
    size_t   alloc_count;
    size_t   total_requested;
#endif
};

struct arena_t {
    uint8_t     *buffer;
    size_t       capacity;
    size_t       offset;
    bool         initialized;

#ifdef ARENA_BLOCK_CHAINING
    arena_block_t *first_block;
    arena_block_t *current_block;
    bool           owns_first;
#endif

#ifdef ARENA_DEBUG
    const char  *name;
    size_t       alloc_count;
    size_t       total_requested;
    size_t       peak_usage;
    size_t       wasted_alignment;
    uint32_t     sequence;
    arena_alloc_record_t *records;
    size_t       records_capacity;
    size_t       records_count;
#endif
};

// initializes arena using user buffer.
ARENA_API bool arena_init(arena_t *arena, void *buffer, size_t size);

#ifdef ARENA_BLOCK_CHAINING
// initializes arena with internal dynamic allocation.
ARENA_API bool arena_init_dynamic(arena_t *arena, size_t initial_size);
#endif

// destroys arena and frees owned resources.
ARENA_API void arena_destroy(arena_t *arena);

#ifdef ARENA_DEBUG
    #define arena_alloc(arena, size) \
        arena__alloc((arena), (size), ARENA_DEFAULT_ALIGN, __FILE__, __LINE__)

    #define arena_alloc_aligned(arena, size, align) \
        arena__alloc((arena), (size), (align), __FILE__, __LINE__)

    #define arena_alloc_zero(arena, size) \
        arena__alloc_zero((arena), (size), ARENA_DEFAULT_ALIGN, __FILE__, __LINE__)

    #define arena_alloc_zero_aligned(arena, size, align) \
        arena__alloc_zero((arena), (size), (align), __FILE__, __LINE__)
#else
    #define arena_alloc(arena, size) \
        arena__alloc((arena), (size), ARENA_DEFAULT_ALIGN)

    #define arena_alloc_aligned(arena, size, align) \
        arena__alloc((arena), (size), (align))

    #define arena_alloc_zero(arena, size) \
        arena__alloc_zero((arena), (size), ARENA_DEFAULT_ALIGN)

    #define arena_alloc_zero_aligned(arena, size, align) \
        arena__alloc_zero((arena), (size), (align))
#endif

#define arena_new(arena, Type) \
    ((Type *)arena_alloc_aligned((arena), sizeof(Type), ARENA_ALIGNOF(Type)))

#define arena_new_zero(arena, Type) \
    ((Type *)arena_alloc_zero_aligned((arena), sizeof(Type), ARENA_ALIGNOF(Type)))

#define arena_new_array(arena, Type, count) \
    ((Type *)arena_alloc_aligned((arena), sizeof(Type) * (count), ARENA_ALIGNOF(Type)))

#define arena_new_array_zero(arena, Type, count) \
    ((Type *)arena_alloc_zero_aligned((arena), sizeof(Type) * (count), ARENA_ALIGNOF(Type)))

// internal allocation functions, use macros above.
#ifdef ARENA_DEBUG
ARENA_API void *arena__alloc(arena_t *arena, size_t size, size_t align, const char *file, int line);
ARENA_API void *arena__alloc_zero(arena_t *arena, size_t size, size_t align, const char *file, int line);
#else
ARENA_API void *arena__alloc(arena_t *arena, size_t size, size_t align);
ARENA_API void *arena__alloc_zero(arena_t *arena, size_t size, size_t align);
#endif

// resets arena to empty state, reusing memory.
ARENA_API void arena_reset(arena_t *arena);

// resets arena to a saved marker, invalidating subsequent allocations.
ARENA_API void arena_reset_to(arena_t *arena, arena_marker_t marker);

// captures current state for later restore.
ARENA_API arena_marker_t arena_save(arena_t *arena);

// gets bytes available in current block.
ARENA_API size_t arena_remaining(const arena_t *arena);

// gets total capacity.
ARENA_API size_t arena_capacity(const arena_t *arena);

// gets used bytes.
ARENA_API size_t arena_used(const arena_t *arena);

// fills stats structure.
ARENA_API arena_stats_t arena_stats(const arena_t *arena);

// checks if arena is initialized and valid.
ARENA_API bool arena_is_valid(const arena_t *arena);

#ifdef ARENA_DEBUG
// sets name for debug printing.
ARENA_API void arena_set_name(arena_t *arena, const char *name);

// enables internal allocation tracking.
ARENA_API bool arena_enable_tracking(arena_t *arena, size_t max_records);

// prints stats and records to stderr.
ARENA_API void arena_print_stats(const arena_t *arena);

// verifies integrity of structure and blocks.
ARENA_API bool arena_check_integrity(const arena_t *arena);
#endif

typedef struct arena_temp_t {
    arena_t        *arena;
    arena_marker_t  marker;
} arena_temp_t;

// begins temporary scope (raii-like).
ARENA_API arena_temp_t arena_temp_begin(arena_t *arena);

// ends temporary scope, rolling back allocations.
ARENA_API void arena_temp_end(arena_temp_t *temp);

#ifdef ARENA_IMPLEMENTATION

static inline bool arena__is_power_of_two(size_t x) {
    return x != 0 && (x & (x - 1)) == 0;
}

static inline size_t arena__align_up(size_t value, size_t align) {
    return (value + align - 1) & ~(align - 1);
}

static inline size_t arena__safe_add(size_t a, size_t b) {
    if (a > SIZE_MAX - b) return SIZE_MAX;
    return a + b;
}

static inline bool arena__calc_aligned_offset(
    size_t current_offset,
    size_t align,
    size_t size,
    size_t capacity,
    size_t *out_aligned_offset,
    size_t *out_padding
) {
    size_t aligned = arena__align_up(current_offset, align);

    if (aligned < current_offset) return false;

    size_t padding = aligned - current_offset;

    if (aligned > capacity) return false;
    if (size > capacity - aligned) return false;

    *out_aligned_offset = aligned;
    *out_padding = padding;
    return true;
}

#ifdef ARENA_BLOCK_CHAINING

static arena_block_t *arena__create_block(size_t min_size) {
    size_t size = min_size < ARENA_BLOCK_MIN_SIZE ? ARENA_BLOCK_MIN_SIZE : min_size;

    size_t header_size = arena__align_up(sizeof(arena_block_t), ARENA_DEFAULT_ALIGN);
    size_t total_size = arena__safe_add(header_size, size);
    if (total_size == SIZE_MAX) return NULL;

    uint8_t *memory = (uint8_t *)ARENA_MALLOC(total_size);
    if (!memory) return NULL;

    arena_block_t *block = (arena_block_t *)memory;
    block->buffer = memory + header_size;
    block->capacity = size;
    block->offset = 0;
    block->next = NULL;
    block->prev = NULL;
    block->owned = true;

    return block;
}

static void arena__free_block(arena_block_t *block) {
    if (block && block->owned) {
        ARENA_FREE(block);
    }
}

static void arena__free_block_chain(arena_block_t *first, bool free_first) {
    arena_block_t *block = first;
    while (block) {
        arena_block_t *next = block->next;
        if (block != first || free_first) {
            arena__free_block(block);
        }
        block = next;
    }
}

#endif

ARENA_API bool arena_init(arena_t *arena, void *buffer, size_t size) {
    if (!arena) return false;
    if (!buffer && size > 0) return false;

    ARENA_MEMSET(arena, 0, sizeof(*arena));

    arena->buffer = (uint8_t *)buffer;
    arena->capacity = size;
    arena->offset = 0;
    arena->initialized = true;

#ifdef ARENA_BLOCK_CHAINING
    arena->first_block = NULL;
    arena->current_block = NULL;
    arena->owns_first = false;
#endif

#ifdef ARENA_DEBUG
    arena->name = "unnamed";
    arena->alloc_count = 0;
    arena->total_requested = 0;
    arena->peak_usage = 0;
    arena->wasted_alignment = 0;
    arena->sequence = 0;
    arena->records = NULL;
    arena->records_capacity = 0;
    arena->records_count = 0;
#endif

    return true;
}

#ifdef ARENA_BLOCK_CHAINING
ARENA_API bool arena_init_dynamic(arena_t *arena, size_t initial_size) {
    if (!arena) return false;

    ARENA_MEMSET(arena, 0, sizeof(*arena));

    arena_block_t *block = arena__create_block(initial_size);
    if (!block) return false;

    arena->buffer = block->buffer;
    arena->capacity = block->capacity;
    arena->offset = 0;
    arena->initialized = true;
    arena->first_block = block;
    arena->current_block = block;
    arena->owns_first = true;

#ifdef ARENA_DEBUG
    arena->name = "unnamed_dynamic";
    arena->alloc_count = 0;
    arena->total_requested = 0;
    arena->peak_usage = 0;
    arena->wasted_alignment = 0;
    arena->sequence = 0;
    arena->records = NULL;
    arena->records_capacity = 0;
    arena->records_count = 0;
#endif

    return true;
}
#endif

ARENA_API void arena_destroy(arena_t *arena) {
    if (!arena || !arena->initialized) return;

#ifdef ARENA_DEBUG
    if (arena->records) {
        ARENA_FREE(arena->records);
    }
    if (arena->buffer && arena->capacity > 0) {
        ARENA_MEMSET(arena->buffer, ARENA_POISON_FREED, arena->capacity);
    }
#endif

#ifdef ARENA_BLOCK_CHAINING
    if (arena->first_block) {
        arena__free_block_chain(arena->first_block, arena->owns_first);
    }
#endif

    ARENA_MEMSET(arena, 0, sizeof(*arena));
}

#ifdef ARENA_DEBUG
ARENA_API void *arena__alloc(arena_t *arena, size_t size, size_t align, const char *file, int line)
#else
ARENA_API void *arena__alloc(arena_t *arena, size_t size, size_t align)
#endif
{
    ARENA_ASSERT(arena != NULL && "arena_alloc: arena is NULL");
    ARENA_ASSERT(arena->initialized && "arena_alloc: arena not initialized");
    ARENA_ASSERT(arena__is_power_of_two(align) && "arena_alloc: alignment must be power of 2");

    if (!arena || !arena->initialized) return NULL;
    if (!arena__is_power_of_two(align)) return NULL;

    if (size == 0) {
        return arena->buffer + arena->offset;
    }

    size_t aligned_offset, padding;

#ifdef ARENA_BLOCK_CHAINING
    if (arena__calc_aligned_offset(arena->offset, align, size, arena->capacity, &aligned_offset, &padding)) {
        goto do_alloc;
    }

    // try allocate new block
    if (arena->first_block) {
        size_t needed = arena__safe_add(size, align - 1);
        if (needed == SIZE_MAX) return NULL;

        arena_block_t *new_block = arena__create_block(needed);
        if (!new_block) return NULL;

        if (arena->current_block) {
            arena->current_block->next = new_block;
            new_block->prev = arena->current_block;
        }
        arena->current_block = new_block;

        arena->buffer = new_block->buffer;
        arena->capacity = new_block->capacity;
        arena->offset = 0;

        if (!arena__calc_aligned_offset(0, align, size, arena->capacity, &aligned_offset, &padding)) {
            ARENA_ASSERT(0 && "Internal error: new block too small");
            return NULL;
        }
    } else {
        return NULL;
    }

do_alloc:
    ;
#else
    if (!arena__calc_aligned_offset(arena->offset, align, size, arena->capacity, &aligned_offset, &padding)) {
        return NULL;
    }
#endif

    void *ptr = arena->buffer + aligned_offset;
    arena->offset = aligned_offset + size;

#ifdef ARENA_BLOCK_CHAINING
    if (arena->current_block) {
        arena->current_block->offset = arena->offset;
    }
#endif

#ifdef ARENA_DEBUG
    ARENA_MEMSET(ptr, ARENA_POISON_UNINIT, size);

    arena->alloc_count++;
    arena->total_requested += size;
    arena->wasted_alignment += padding;

    size_t current_usage = arena_used(arena);
    if (current_usage > arena->peak_usage) {
        arena->peak_usage = current_usage;
    }

    if (arena->records && arena->records_count < arena->records_capacity) {
        arena_alloc_record_t *rec = &arena->records[arena->records_count++];
        rec->ptr = ptr;
        rec->size = size;
        rec->actual_size = size + padding;
        rec->file = file;
        rec->line = line;
        rec->sequence = arena->sequence++;
    }
    (void)file; (void)line;
#endif

    return ptr;
}

#ifdef ARENA_DEBUG
ARENA_API void *arena__alloc_zero(arena_t *arena, size_t size, size_t align, const char *file, int line)
#else
ARENA_API void *arena__alloc_zero(arena_t *arena, size_t size, size_t align)
#endif
{
#ifdef ARENA_DEBUG
    void *ptr = arena__alloc(arena, size, align, file, line);
#else
    void *ptr = arena__alloc(arena, size, align);
#endif

    if (ptr && size > 0) {
        ARENA_MEMSET(ptr, 0, size);
    }

    return ptr;
}

ARENA_API void arena_reset(arena_t *arena) {
    if (!arena || !arena->initialized) return;

#ifdef ARENA_BLOCK_CHAINING
    if (arena->first_block) {
        for (arena_block_t *b = arena->first_block; b; b = b->next) {
#ifdef ARENA_DEBUG
            if (b->offset > 0) {
                ARENA_MEMSET(b->buffer, ARENA_POISON_FREED, b->offset);
            }
#endif
            b->offset = 0;
        }

        arena->current_block = arena->first_block;
        arena->buffer = arena->first_block->buffer;
        arena->capacity = arena->first_block->capacity;
    } else {
#ifdef ARENA_DEBUG
        if (arena->buffer && arena->offset > 0) {
            ARENA_MEMSET(arena->buffer, ARENA_POISON_FREED, arena->offset);
        }
#endif
    }
#else
#ifdef ARENA_DEBUG
    if (arena->buffer && arena->offset > 0) {
        ARENA_MEMSET(arena->buffer, ARENA_POISON_FREED, arena->offset);
    }
#endif
#endif

    arena->offset = 0;

#ifdef ARENA_DEBUG
    arena->records_count = 0;
#endif
}

ARENA_API void arena_reset_to(arena_t *arena, arena_marker_t marker) {
    if (!arena || !arena->initialized) return;

#ifdef ARENA_BLOCK_CHAINING
    if (arena->first_block && marker.block) {
        if (marker.block->next) {
            arena__free_block_chain(marker.block->next, true);
            marker.block->next = NULL;
        }

        arena->current_block = marker.block;
        arena->buffer = marker.block->buffer;
        arena->capacity = marker.block->capacity;
        marker.block->offset = marker.offset;

#ifdef ARENA_DEBUG
        if (marker.offset < arena->capacity) {
            ARENA_MEMSET(arena->buffer + marker.offset, ARENA_POISON_FREED,
                        arena->capacity - marker.offset);
        }
#endif
    }
#else
#ifdef ARENA_DEBUG
    if (marker.offset < arena->offset) {
        ARENA_MEMSET(arena->buffer + marker.offset, ARENA_POISON_FREED,
                    arena->offset - marker.offset);
    }
#endif
#endif

    arena->offset = marker.offset;

#ifdef ARENA_DEBUG
    arena->alloc_count = marker.alloc_count;
    arena->total_requested = marker.total_requested;
    if (arena->records) {
        while (arena->records_count > 0 &&
               arena->records[arena->records_count - 1].sequence >= marker.alloc_count) {
            arena->records_count--;
        }
    }
#endif
}

ARENA_API arena_marker_t arena_save(arena_t *arena) {
    arena_marker_t marker = {0};

    if (!arena || !arena->initialized) return marker;

    marker.offset = arena->offset;

#ifdef ARENA_BLOCK_CHAINING
    marker.block = arena->current_block;
#endif

#ifdef ARENA_DEBUG
    marker.alloc_count = arena->alloc_count;
    marker.total_requested = arena->total_requested;
#endif

    return marker;
}

ARENA_API size_t arena_remaining(const arena_t *arena) {
    if (!arena || !arena->initialized) return 0;
    if (arena->offset > arena->capacity) return 0;
    return arena->capacity - arena->offset;
}

ARENA_API size_t arena_capacity(const arena_t *arena) {
    if (!arena || !arena->initialized) return 0;

#ifdef ARENA_BLOCK_CHAINING
    if (arena->first_block) {
        size_t total = 0;
        for (arena_block_t *b = arena->first_block; b; b = b->next) {
            total = arena__safe_add(total, b->capacity);
            if (total == SIZE_MAX) return SIZE_MAX;
        }
        return total;
    }
#endif

    return arena->capacity;
}

ARENA_API size_t arena_used(const arena_t *arena) {
    if (!arena || !arena->initialized) return 0;

#ifdef ARENA_BLOCK_CHAINING
    if (arena->first_block) {
        size_t total = 0;
        for (arena_block_t *b = arena->first_block; b; b = b->next) {
            total = arena__safe_add(total, b->offset);
            if (total == SIZE_MAX) return SIZE_MAX;
        }
        return total;
    }
#endif

    return arena->offset;
}

ARENA_API arena_stats_t arena_stats(const arena_t *arena) {
    arena_stats_t stats = {0};

    if (!arena || !arena->initialized) return stats;

    stats.capacity = arena->capacity;
    stats.used = arena->offset;
    stats.remaining = arena_remaining(arena);

#ifdef ARENA_BLOCK_CHAINING
    if (arena->first_block) {
        stats.block_count = 0;
        stats.total_capacity = 0;
        size_t total_used = 0;

        for (arena_block_t *b = arena->first_block; b; b = b->next) {
            stats.block_count++;
            stats.total_capacity = arena__safe_add(stats.total_capacity, b->capacity);
            total_used = arena__safe_add(total_used, b->offset);
        }

        stats.used = total_used;
        stats.capacity = stats.total_capacity;
    }
#endif

#ifdef ARENA_DEBUG
    stats.alloc_count = arena->alloc_count;
    stats.total_requested = arena->total_requested;
    stats.peak_usage = arena->peak_usage;
    stats.wasted_alignment = arena->wasted_alignment;
#endif

    return stats;
}

ARENA_API bool arena_is_valid(const arena_t *arena) {
    if (!arena) return false;
    if (!arena->initialized) return false;
    if (arena->offset > arena->capacity) return false;
    return true;
}

ARENA_API arena_temp_t arena_temp_begin(arena_t *arena) {
    arena_temp_t temp = {0};
    if (arena && arena->initialized) {
        temp.arena = arena;
        temp.marker = arena_save(arena);
    }
    return temp;
}

ARENA_API void arena_temp_end(arena_temp_t *temp) {
    if (temp && temp->arena) {
        arena_reset_to(temp->arena, temp->marker);
        temp->arena = NULL;
    }
}

#ifdef ARENA_DEBUG

ARENA_API void arena_set_name(arena_t *arena, const char *name) {
    if (arena && arena->initialized) {
        arena->name = name ? name : "unnamed";
    }
}

ARENA_API bool arena_enable_tracking(arena_t *arena, size_t max_records) {
    if (!arena || !arena->initialized) return false;

    if (arena->records) {
        ARENA_FREE(arena->records);
        arena->records = NULL;
    }

    if (max_records == 0) {
        arena->records_capacity = 0;
        arena->records_count = 0;
        return true;
    }

    arena->records = (arena_alloc_record_t *)ARENA_MALLOC(
        max_records * sizeof(arena_alloc_record_t));

    if (!arena->records) return false;

    arena->records_capacity = max_records;
    arena->records_count = 0;

    return true;
}

ARENA_API void arena_print_stats(const arena_t *arena) {
    if (!arena || !arena->initialized) {
        ARENA_PRINTF("Arena: <invalid>\n");
        return;
    }

    arena_stats_t stats = arena_stats(arena);

    ARENA_PRINTF("Arena Statistics: %s \n", arena->name);
    ARENA_PRINTF("  Capacity:          %zu bytes\n", stats.capacity);
    ARENA_PRINTF("  Used:              %zu bytes (%.1f%%)\n",
            stats.used,
            stats.capacity > 0 ? (100.0 * stats.used / stats.capacity) : 0.0);
    ARENA_PRINTF("  Remaining:         %zu bytes\n", stats.remaining);
    ARENA_PRINTF("  Allocations:       %zu\n", stats.alloc_count);
    ARENA_PRINTF("  Total Requested:   %zu bytes\n", stats.total_requested);
    ARENA_PRINTF("  Peak Usage:        %zu bytes\n", stats.peak_usage);
    ARENA_PRINTF("  Wasted (align):    %zu bytes\n", stats.wasted_alignment);

#ifdef ARENA_BLOCK_CHAINING
    ARENA_PRINTF("  Blocks:            %zu\n", stats.block_count);
#endif

    if (arena->records && arena->records_count > 0) {
        ARENA_PRINTF("\n  Recent allocations:\n");
        size_t start = arena->records_count > 10 ? arena->records_count - 10 : 0;
        for (size_t i = start; i < arena->records_count; i++) {
            arena_alloc_record_t *rec = &arena->records[i];
            ARENA_PRINTF("    [%u] %zu bytes at %p (%s:%d)\n",
                    rec->sequence, rec->size, rec->ptr, rec->file, rec->line);
        }
    }
}

ARENA_API bool arena_check_integrity(const arena_t *arena) {
    if (!arena) return false;
    if (!arena->initialized) return false;
    if (arena->offset > arena->capacity) return false;

#ifdef ARENA_BLOCK_CHAINING
    if (arena->first_block) {
        size_t count = 0;
        arena_block_t *prev = NULL;
        for (arena_block_t *b = arena->first_block; b; b = b->next) {
            if (b->prev != prev) return false;
            if (b->offset > b->capacity) return false;

            prev = b;
            count++;

            if (count > 10000) return false;
        }
    }
#endif

    return true;
}

#endif // ARENA_DEBUG

#endif // ARENA_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // ARENA_ALLOCATOR_H