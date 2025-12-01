/*
 * slab.h , Single header slab allocator library
 *
 * A multi size class memory allocator with pool like efficiency
 * this provides O(1) allocation and deallocation through embedded free lists.
 *
 * USAGE:
 *   In ONE C file, define SLAB_IMPLEMENTATION before including:
 *     #define SLAB_IMPLEMENTATION
 *     #include "slab.h"
 *
 *   In all other files, just include normally:
 *     #include "slab.h"
 *
 * EXAMPLE:
 *   uint8_t buffer[65536];
 *   size_t sizes[] = {32, 64, 128, 256, 512};
 *   slab_t slab;
 *
 *   if (slab_init(&slab, buffer, sizeof(buffer), sizes, 5) != 0) {
 *       // Handle error
 *   }
 *
 *   void *ptr = slab_alloc(&slab, 50);  // Gets 64-byte slot
 *   slab_free(&slab, ptr);
 *   slab_destroy(&slab);
 *
 * OPTIONS:
 *
 * CONFIGURATION MACROS (define before including):
 *   SLAB_IMPLEMENTATION   - Include implementation (define in ONE file)
 *   SLAB_STATIC           - Make all functions static
 *   SLAB_DEBUG            - Enable debug features (poison, leak detection)
 *   SLAB_ASSERT(x)        - Custom assert (default: assert(x))
 *   SLAB_MAX_CLASSES      - Maximum size classes (default: 16)
 *   SLAB_ALIGNMENT        - Minimum alignment (default: 8)
 *   SLAB_MEMSET           - Custom memset (default: memset)
 *   SLAB_POISON_BYTE      - Byte used to poison freed slots (default: 0xFE)
 *
 * Some things about this lib you may need to know:
 *
 *   Thread Safety:
 *     This library is not thread-safe, If you share a slab across threads,
 *     you must provide your own synchronization (mutex, spinlock, etc.).
 *
 *   Memory Partitioning:
 *     The buffer is divided EQUALLY among all size classes. This means:
 *     - Smaller slot sizes get more slots (good for most use cases)
 *     - If you need 1000 small objects and 5 large ones, size your buffer
 *       based on: max(1000 * small_size, 5 * large_size) * num_classes
 *     - Use slab_buffer_size_needed() to calculate minimum buffer size
 *
 *   Debug Mode (SLAB_DEBUG):
 *     - Freed memory is poisoned with SLAB_POISON_BYTE (helps catch use after free)
 *     - Leak detection runs on slab_destroy()
 *     - Peak usage and lifetime counters are tracked
 *     - Has performance cost (memset on every free) , use only for debugging
 *
 */

/*
 * slab.h - single-header slab allocator library
 * usage:
 *   #define SLAB_IMPLEMENTATION
 *   #include "slab.h"
 */

#ifndef SLAB_H
#define SLAB_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SLAB_MAX_CLASSES
#define SLAB_MAX_CLASSES 16
#endif

#ifndef SLAB_ALIGNMENT
#define SLAB_ALIGNMENT 8
#endif

#ifndef SLAB_POISON_BYTE
#define SLAB_POISON_BYTE 0xFE
#endif

#ifdef SLAB_STATIC
#define SLAB_API static
#else
#define SLAB_API extern
#endif

#include <stddef.h>
#include <stdint.h>

#define SLAB_OK                 0
#define SLAB_ERR_NULL_PARAM    -1
#define SLAB_ERR_ZERO_SIZE     -2
#define SLAB_ERR_TOO_MANY      -3
#define SLAB_ERR_BUFFER_SMALL  -4
#define SLAB_ERR_INVALID_SIZE  -5
#define SLAB_ERR_ALREADY_INIT  -6

typedef struct slab_class_stats {
    size_t slot_size;
    size_t total_slots;
    size_t used_slots;
    size_t free_slots;
    size_t peak_used;
    size_t alloc_count;
    size_t free_count;
} slab_class_stats_t;

typedef struct slab_stats {
    size_t class_count;
    size_t total_capacity;
    size_t usable_capacity;
    size_t total_slots;
    size_t used_slots;
    size_t free_slots;
    size_t peak_used;
    size_t total_alloc_count;
    size_t total_free_count;
} slab_stats_t;

typedef struct slab_class {
    size_t   slot_size;
    void    *free_list;
    uint8_t *region_start;
    uint8_t *region_end;
    size_t   slot_count;
    size_t   free_count;
#ifdef SLAB_DEBUG
    size_t   peak_used;
    size_t   alloc_count;
    size_t   free_count_total;
#endif
} slab_class_t;

typedef struct slab {
    uint8_t      *buffer;
    uint8_t      *aligned_start;
    size_t        capacity;
    size_t        usable_capacity;
    slab_class_t  classes[SLAB_MAX_CLASSES];
    size_t        class_count;
    int           initialized;
} slab_t;

// initializes slab. buffer is divided equally among size classes.
SLAB_API int slab_init(slab_t *slab, void *buffer, size_t size, const size_t sizes[], size_t count);

// destroys slab. in debug mode, reports leaked allocations.
SLAB_API void slab_destroy(slab_t *slab);

// allocates memory. returns null if size exceeds largest class or class exhausted.
SLAB_API void *slab_alloc(slab_t *slab, size_t size);

// frees memory. ptr must be owned by this slab.
SLAB_API void slab_free(slab_t *slab, void *ptr);

// resets slab to initial state. invalidates all allocations.
SLAB_API void slab_reset(slab_t *slab);

// gets aggregate statistics for the slab.
SLAB_API slab_stats_t slab_stats(const slab_t *slab);

// gets statistics for a specific size class.
SLAB_API slab_class_stats_t slab_class_stats(const slab_t *slab, size_t class_index);

// gets the number of size classes.
SLAB_API size_t slab_class_count(const slab_t *slab);

// gets the slot size for a specific class.
SLAB_API size_t slab_class_slot_size(const slab_t *slab, size_t class_index);

// checks if a pointer is within slab managed memory.
SLAB_API int slab_owns(const slab_t *slab, const void *ptr);

// gets the actual allocated size for a pointer.
SLAB_API size_t slab_usable_size(const slab_t *slab, const void *ptr);

// allocates zero-initialized memory.
SLAB_API void *slab_calloc(slab_t *slab, size_t size);

// gets the maximum allocatable size (size of largest class).
SLAB_API size_t slab_max_alloc(const slab_t *slab);

// calculates minimum buffer size needed for configuration including alignment.
SLAB_API size_t slab_buffer_size_needed(const size_t sizes[], size_t count, size_t min_slots_each);

#ifdef SLAB_IMPLEMENTATION

#include <string.h>

#ifndef SLAB_ASSERT
#include <assert.h>
#define SLAB_ASSERT(x) assert(x)
#endif

#ifndef SLAB_MEMSET
#define SLAB_MEMSET memset
#endif

#define SLAB_ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))
#define SLAB_MIN(a, b) ((a) < (b) ? (a) : (b))
#define SLAB_MAX(a, b) ((a) > (b) ? (a) : (b))

#define SLAB_MIN_SLOT_SIZE SLAB_MAX(sizeof(void*), SLAB_ALIGNMENT)
#define SLAB_MAGIC 0x534C4142

typedef struct slab_free_node {
    struct slab_free_node *next;
} slab_free_node_t;

static void slab__sort_sizes(size_t *arr, size_t count) {
    size_t i, j, key;
    for (i = 1; i < count; i++) {
        key = arr[i];
        j = i;
        while (j > 0 && arr[j - 1] > key) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = key;
    }
}

static size_t slab__find_class_for_size(const slab_t *slab, size_t size) {
    size_t i;
    for (i = 0; i < slab->class_count; i++) {
        if (slab->classes[i].slot_size >= size) {
            return i;
        }
    }
    return (size_t)-1;
}

static size_t slab__find_class_for_ptr(const slab_t *slab, const void *ptr) {
    size_t i;
    const uint8_t *p = (const uint8_t *)ptr;

    for (i = 0; i < slab->class_count; i++) {
        const slab_class_t *cls = &slab->classes[i];
        if (p >= cls->region_start && p < cls->region_end) {
            return i;
        }
    }
    return (size_t)-1;
}

static int slab__validate_ptr_in_class(const slab_class_t *cls, const void *ptr) {
    const uint8_t *p = (const uint8_t *)ptr;
    size_t offset;

    if (p < cls->region_start || p >= cls->region_end) {
        return 0;
    }

    offset = (size_t)(p - cls->region_start);
    if (offset % cls->slot_size != 0) {
        return 0;
    }

    return 1;
}

static void slab__build_free_list(slab_class_t *cls) {
    size_t i;
    slab_free_node_t *prev = NULL;
    uint8_t *slot;

    cls->free_list = NULL;
    cls->free_count = cls->slot_count;

    if (cls->slot_count == 0) return;

    for (i = cls->slot_count; i > 0; i--) {
        slot = cls->region_start + (i - 1) * cls->slot_size;
        slab_free_node_t *node = (slab_free_node_t *)slot;
        node->next = prev;
        prev = node;
    }

    cls->free_list = prev;
}

#ifdef SLAB_DEBUG
static void slab__poison(void *ptr, size_t size) {
    size_t skip = sizeof(slab_free_node_t);
    if (size > skip) {
        SLAB_MEMSET((uint8_t *)ptr + skip, SLAB_POISON_BYTE, size - skip);
    }
}

static void slab__check_leaks(const slab_t *slab) {
    size_t i;
    size_t total_leaked = 0;

    for (i = 0; i < slab->class_count; i++) {
        const slab_class_t *cls = &slab->classes[i];
        size_t used = cls->slot_count - cls->free_count;
        if (used > 0) total_leaked += used;
    }

    if (total_leaked > 0) {
        SLAB_ASSERT(0 && "memory leak detected in slab allocator");
    }
}
#endif

SLAB_API int slab_init(slab_t *slab, void *buffer, size_t size, const size_t sizes[], size_t count) {
    size_t i;
    size_t sorted_sizes[SLAB_MAX_CLASSES];
    uint8_t *region_ptr;
    size_t region_size;
    size_t alignment_loss;

    if (slab == NULL || buffer == NULL || sizes == NULL) return SLAB_ERR_NULL_PARAM;
    if (size == 0 || count == 0) return SLAB_ERR_ZERO_SIZE;
    if (count > SLAB_MAX_CLASSES) return SLAB_ERR_TOO_MANY;
    if (slab->initialized == SLAB_MAGIC) return SLAB_ERR_ALREADY_INIT;

    for (i = 0; i < count; i++) {
        if (sizes[i] == 0) return SLAB_ERR_INVALID_SIZE;
        sorted_sizes[i] = sizes[i];
    }

    slab__sort_sizes(sorted_sizes, count);

    for (i = 1; i < count; i++) {
        if (sorted_sizes[i] == sorted_sizes[i - 1]) return SLAB_ERR_INVALID_SIZE;
    }

    slab->buffer = (uint8_t *)buffer;
    slab->capacity = size;

    {
        uintptr_t addr = (uintptr_t)buffer;
        uintptr_t aligned = SLAB_ALIGN_UP(addr, SLAB_ALIGNMENT);
        alignment_loss = (size_t)(aligned - addr);
    }

    if (alignment_loss >= size) return SLAB_ERR_BUFFER_SMALL;

    slab->aligned_start = slab->buffer + alignment_loss;
    slab->usable_capacity = size - alignment_loss;
    slab->class_count = count;

    region_size = slab->usable_capacity / count;
    region_size = (region_size / SLAB_ALIGNMENT) * SLAB_ALIGNMENT;
    region_ptr = slab->aligned_start;

    if (region_size < SLAB_ALIGNMENT) return SLAB_ERR_BUFFER_SMALL;

    for (i = 0; i < count; i++) {
        slab_class_t *cls = &slab->classes[i];
        size_t aligned_slot_size;
        size_t slots;

        aligned_slot_size = SLAB_ALIGN_UP(sorted_sizes[i], SLAB_ALIGNMENT);
        aligned_slot_size = SLAB_MAX(aligned_slot_size, SLAB_MIN_SLOT_SIZE);

        slots = region_size / aligned_slot_size;

        if (slots == 0) {
            SLAB_MEMSET(slab, 0, sizeof(*slab));
            return SLAB_ERR_BUFFER_SMALL;
        }

        cls->slot_size = aligned_slot_size;
        cls->region_start = region_ptr;
        cls->slot_count = slots;
        cls->region_end = region_ptr + (slots * aligned_slot_size);

        slab__build_free_list(cls);

#ifdef SLAB_DEBUG
        cls->peak_used = 0;
        cls->alloc_count = 0;
        cls->free_count_total = 0;

        SLAB_MEMSET(cls->region_start, SLAB_POISON_BYTE, slots * aligned_slot_size);
        slab__build_free_list(cls);
#endif

        region_ptr += region_size;
    }

    for (i = count; i < SLAB_MAX_CLASSES; i++) {
        SLAB_MEMSET(&slab->classes[i], 0, sizeof(slab_class_t));
    }

    slab->initialized = SLAB_MAGIC;

    return SLAB_OK;
}

SLAB_API void slab_destroy(slab_t *slab) {
    if (slab == NULL || slab->initialized != SLAB_MAGIC) return;

#ifdef SLAB_DEBUG
    slab__check_leaks(slab);
#endif

    SLAB_MEMSET(slab, 0, sizeof(*slab));
}

SLAB_API void *slab_alloc(slab_t *slab, size_t size) {
    size_t class_idx;
    slab_class_t *cls;
    slab_free_node_t *node;
    void *ptr;

    if (slab == NULL || slab->initialized != SLAB_MAGIC || size == 0) return NULL;

    class_idx = slab__find_class_for_size(slab, size);
    if (class_idx == (size_t)-1) return NULL;

    cls = &slab->classes[class_idx];

    if (cls->free_list == NULL) return NULL;

    node = (slab_free_node_t *)cls->free_list;
    cls->free_list = node->next;
    cls->free_count--;

    ptr = (void *)node;

#ifdef SLAB_DEBUG
    {
        size_t used = cls->slot_count - cls->free_count;
        if (used > cls->peak_used) cls->peak_used = used;
        cls->alloc_count++;
    }
#endif

    return ptr;
}

SLAB_API void slab_free(slab_t *slab, void *ptr) {
    size_t class_idx;
    slab_class_t *cls;
    slab_free_node_t *node;

    if (ptr == NULL) return;

    if (slab == NULL || slab->initialized != SLAB_MAGIC) {
        SLAB_ASSERT(0 && "slab_free called with invalid slab");
        return;
    }

    class_idx = slab__find_class_for_ptr(slab, ptr);
    if (class_idx == (size_t)-1) {
        SLAB_ASSERT(0 && "slab_free called with pointer not from this slab");
        return;
    }

    cls = &slab->classes[class_idx];

    if (!slab__validate_ptr_in_class(cls, ptr)) {
        SLAB_ASSERT(0 && "slab_free called with misaligned pointer");
        return;
    }

#ifdef SLAB_DEBUG
    slab__poison(ptr, cls->slot_size);
    cls->free_count_total++;
#endif

    node = (slab_free_node_t *)ptr;
    node->next = (slab_free_node_t *)cls->free_list;
    cls->free_list = node;
    cls->free_count++;
}

SLAB_API void slab_reset(slab_t *slab) {
    size_t i;
    if (slab == NULL || slab->initialized != SLAB_MAGIC) return;

    for (i = 0; i < slab->class_count; i++) {
        slab_class_t *cls = &slab->classes[i];

#ifdef SLAB_DEBUG
        SLAB_MEMSET(cls->region_start, SLAB_POISON_BYTE, cls->slot_count * cls->slot_size);
#endif

        slab__build_free_list(cls);

#ifdef SLAB_DEBUG
        cls->peak_used = 0;
#endif
    }
}

SLAB_API slab_stats_t slab_stats(const slab_t *slab) {
    slab_stats_t stats;
    size_t i;

    SLAB_MEMSET(&stats, 0, sizeof(stats));

    if (slab == NULL || slab->initialized != SLAB_MAGIC) return stats;

    stats.class_count = slab->class_count;
    stats.total_capacity = slab->capacity;
    stats.usable_capacity = slab->usable_capacity;

    for (i = 0; i < slab->class_count; i++) {
        const slab_class_t *cls = &slab->classes[i];

        stats.total_slots += cls->slot_count;
        stats.free_slots += cls->free_count;

#ifdef SLAB_DEBUG
        stats.peak_used += cls->peak_used;
        stats.total_alloc_count += cls->alloc_count;
        stats.total_free_count += cls->free_count_total;
#endif
    }

    stats.used_slots = stats.total_slots - stats.free_slots;

#ifndef SLAB_DEBUG
    stats.peak_used = stats.used_slots;
#endif

    return stats;
}

SLAB_API slab_class_stats_t slab_class_stats(const slab_t *slab, size_t class_index) {
    slab_class_stats_t stats;
    const slab_class_t *cls;

    SLAB_MEMSET(&stats, 0, sizeof(stats));

    if (slab == NULL || slab->initialized != SLAB_MAGIC) return stats;
    if (class_index >= slab->class_count) return stats;

    cls = &slab->classes[class_index];

    stats.slot_size = cls->slot_size;
    stats.total_slots = cls->slot_count;
    stats.free_slots = cls->free_count;
    stats.used_slots = cls->slot_count - cls->free_count;

#ifdef SLAB_DEBUG
    stats.peak_used = cls->peak_used;
    stats.alloc_count = cls->alloc_count;
    stats.free_count = cls->free_count_total;
#else
    stats.peak_used = stats.used_slots;
    stats.alloc_count = 0;
    stats.free_count = 0;
#endif

    return stats;
}

SLAB_API size_t slab_class_count(const slab_t *slab) {
    if (slab == NULL || slab->initialized != SLAB_MAGIC) return 0;
    return slab->class_count;
}

SLAB_API size_t slab_class_slot_size(const slab_t *slab, size_t class_index) {
    if (slab == NULL || slab->initialized != SLAB_MAGIC) return 0;
    if (class_index >= slab->class_count) return 0;
    return slab->classes[class_index].slot_size;
}

SLAB_API int slab_owns(const slab_t *slab, const void *ptr) {
    if (slab == NULL || slab->initialized != SLAB_MAGIC || ptr == NULL) return 0;
    return slab__find_class_for_ptr(slab, ptr) != (size_t)-1;
}

SLAB_API size_t slab_usable_size(const slab_t *slab, const void *ptr) {
    size_t class_idx;
    if (slab == NULL || slab->initialized != SLAB_MAGIC || ptr == NULL) return 0;

    class_idx = slab__find_class_for_ptr(slab, ptr);
    if (class_idx == (size_t)-1) return 0;

    return slab->classes[class_idx].slot_size;
}

SLAB_API void *slab_calloc(slab_t *slab, size_t size) {
    void *ptr = slab_alloc(slab, size);
    if (ptr != NULL) {
        size_t slot_size = slab_usable_size(slab, ptr);
        SLAB_MEMSET(ptr, 0, slot_size);
    }
    return ptr;
}

SLAB_API size_t slab_max_alloc(const slab_t *slab) {
    if (slab == NULL || slab->initialized != SLAB_MAGIC || slab->class_count == 0) return 0;
    return slab->classes[slab->class_count - 1].slot_size;
}

SLAB_API size_t slab_buffer_size_needed(const size_t sizes[], size_t count, size_t min_slots_each) {
    size_t i;
    size_t max_aligned_size = 0;
    size_t slots_needed;
    size_t region_size_needed;
    size_t total;

    if (sizes == NULL || count == 0 || count > SLAB_MAX_CLASSES) return 0;

    slots_needed = (min_slots_each == 0) ? 1 : min_slots_each;

    for (i = 0; i < count; i++) {
        size_t aligned_size;
        if (sizes[i] == 0) return 0;

        aligned_size = SLAB_ALIGN_UP(sizes[i], SLAB_ALIGNMENT);
        aligned_size = SLAB_MAX(aligned_size, SLAB_MIN_SLOT_SIZE);

        if (aligned_size > max_aligned_size) max_aligned_size = aligned_size;
    }

    region_size_needed = max_aligned_size * slots_needed;
    region_size_needed = SLAB_ALIGN_UP(region_size_needed, SLAB_ALIGNMENT);

    if (region_size_needed > SIZE_MAX / count) return 0;

    total = region_size_needed * count;
    total += SLAB_ALIGNMENT - 1;

    return total;
}

#endif // SLAB_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // SLAB_H
