/*
 * tests for slab.h
 *
 *   # basic tests
 *   gcc -Wall -Wextra -O2 -o tests_slab tests_slab.c && ./tests_slab
 *
 *   # with debug features
 *   gcc -Wall -Wextra -DSLAB_DEBUG -O2 -o tests_slab_debug tests_slab.c && ./tests_slab_debug
 */

#define SLAB_IMPLEMENTATION
#include "../slab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)

#define RUN_TEST(name) do { \
    printf("  Running %-40s ", #name "..."); \
    fflush(stdout); \
    tests_run++; \
    name(); \
    printf("\033[32mPASSED\033[0m\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("\033[31mFAILED\033[0m\n"); \
        printf("    Assertion failed: %s\n", #cond); \
        printf("    Location: %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("\033[31mFAILED\033[0m\n"); \
        printf("    Assertion failed: %s == %s\n", #a, #b); \
        printf("    Got: %zu, Expected: %zu\n", (size_t)(a), (size_t)(b)); \
        printf("    Location: %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NE(a, b) do { \
    if ((a) == (b)) { \
        printf("\033[31mFAILED\033[0m\n"); \
        printf("    Assertion failed: %s != %s\n", #a, #b); \
        printf("    Location: %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("\033[31mFAILED\033[0m\n"); \
        printf("    Assertion failed: %s != NULL\n", #ptr); \
        printf("    Location: %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        printf("\033[31mFAILED\033[0m\n"); \
        printf("    Assertion failed: %s == NULL\n", #ptr); \
        printf("    Location: %s:%d\n", __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } \
} while(0)

// check if pointer is aligned to given alignment
static bool is_aligned(const void *ptr, size_t align) {
    return ((uintptr_t)ptr & (align - 1)) == 0;
}

TEST(test_init_basic) {
    uint8_t buffer[4096];
    size_t sizes[] = {32, 64, 128};
    slab_t slab;
    int result;

    memset(&slab, 0, sizeof(slab));

    result = slab_init(&slab, buffer, sizeof(buffer), sizes, 3);
    ASSERT_EQ(result, SLAB_OK);
    ASSERT_EQ(slab_class_count(&slab), 3);

    slab_destroy(&slab);
}

TEST(test_init_null_params) {
    uint8_t buffer[1024];
    size_t sizes[] = {32};
    slab_t slab;
    int result;

    memset(&slab, 0, sizeof(slab));

    result = slab_init(NULL, buffer, sizeof(buffer), sizes, 1);
    ASSERT_EQ(result, SLAB_ERR_NULL_PARAM);

    result = slab_init(&slab, NULL, sizeof(buffer), sizes, 1);
    ASSERT_EQ(result, SLAB_ERR_NULL_PARAM);

    result = slab_init(&slab, buffer, sizeof(buffer), NULL, 1);
    ASSERT_EQ(result, SLAB_ERR_NULL_PARAM);
}

TEST(test_init_zero_params) {
    uint8_t buffer[1024];
    size_t sizes[] = {32};
    slab_t slab;
    int result;

    memset(&slab, 0, sizeof(slab));

    result = slab_init(&slab, buffer, 0, sizes, 1);
    ASSERT_EQ(result, SLAB_ERR_ZERO_SIZE);

    result = slab_init(&slab, buffer, sizeof(buffer), sizes, 0);
    ASSERT_EQ(result, SLAB_ERR_ZERO_SIZE);
}

TEST(test_init_too_many_classes) {
    uint8_t buffer[65536];
    size_t sizes[SLAB_MAX_CLASSES + 1];
    slab_t slab;
    int result;
    size_t i;

    memset(&slab, 0, sizeof(slab));

    for (i = 0; i <= SLAB_MAX_CLASSES; i++) {
        sizes[i] = 32 * (i + 1);
    }

    result = slab_init(&slab, buffer, sizeof(buffer), sizes, SLAB_MAX_CLASSES + 1);
    ASSERT_EQ(result, SLAB_ERR_TOO_MANY);
}

TEST(test_init_duplicate_sizes) {
    uint8_t buffer[4096];
    size_t sizes[] = {32, 64, 64, 128}; // duplicate 64
    slab_t slab;
    int result;

    memset(&slab, 0, sizeof(slab));

    result = slab_init(&slab, buffer, sizeof(buffer), sizes, 4);
    ASSERT_EQ(result, SLAB_ERR_INVALID_SIZE);
}

TEST(test_init_zero_size_class) {
    uint8_t buffer[4096];
    size_t sizes[] = {32, 0, 128};
    slab_t slab;
    int result;

    memset(&slab, 0, sizeof(slab));

    result = slab_init(&slab, buffer, sizeof(buffer), sizes, 3);
    ASSERT_EQ(result, SLAB_ERR_INVALID_SIZE);
}

TEST(test_init_buffer_too_small) {
    uint8_t buffer[64];
    size_t sizes[] = {32, 64, 128, 256, 512};
    slab_t slab;
    int result;

    memset(&slab, 0, sizeof(slab));

    // 5 classes, each needs at least 1 slot, but buffer is tiny
    result = slab_init(&slab, buffer, sizeof(buffer), sizes, 5);
    ASSERT_EQ(result, SLAB_ERR_BUFFER_SMALL);
}

TEST(test_init_sizes_sorted) {
    uint8_t buffer[4096];
    size_t sizes[] = {128, 32, 256, 64}; // unsorted
    slab_t slab;
    int result;

    memset(&slab, 0, sizeof(slab));

    result = slab_init(&slab, buffer, sizeof(buffer), sizes, 4);
    ASSERT_EQ(result, SLAB_OK);

    // classes should be sorted by size
    ASSERT(slab_class_slot_size(&slab, 0) <= slab_class_slot_size(&slab, 1));
    ASSERT(slab_class_slot_size(&slab, 1) <= slab_class_slot_size(&slab, 2));
    ASSERT(slab_class_slot_size(&slab, 2) <= slab_class_slot_size(&slab, 3));

    slab_destroy(&slab);
}

TEST(test_basic_multisize) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128, 256};
    slab_t slab;
    void *ptr32, *ptr64, *ptr128, *ptr256;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 4), SLAB_OK);

    ptr32 = slab_alloc(&slab, 32);
    ASSERT_NOT_NULL(ptr32);

    ptr64 = slab_alloc(&slab, 64);
    ASSERT_NOT_NULL(ptr64);

    ptr128 = slab_alloc(&slab, 128);
    ASSERT_NOT_NULL(ptr128);

    ptr256 = slab_alloc(&slab, 256);
    ASSERT_NOT_NULL(ptr256);

    // all should be different addresses
    ASSERT_NE(ptr32, ptr64);
    ASSERT_NE(ptr32, ptr128);
    ASSERT_NE(ptr32, ptr256);
    ASSERT_NE(ptr64, ptr128);
    ASSERT_NE(ptr64, ptr256);
    ASSERT_NE(ptr128, ptr256);

    // verify we can write to them
    memset(ptr32, 0xAA, 32);
    memset(ptr64, 0xBB, 64);
    memset(ptr128, 0xCC, 128);
    memset(ptr256, 0xDD, 256);

    slab_free(&slab, ptr256);
    slab_free(&slab, ptr128);
    slab_free(&slab, ptr64);
    slab_free(&slab, ptr32);

    slab_destroy(&slab);
}

TEST(test_size_rounding) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128, 256};
    slab_t slab;
    void *ptr;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 4), SLAB_OK);

    // 50-byte request should get a 64-byte slot (rounds up)
    ptr = slab_alloc(&slab, 50);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(slab_usable_size(&slab, ptr), slab_class_slot_size(&slab, 1));

    // 1-byte request should get smallest class
    slab_free(&slab, ptr);
    ptr = slab_alloc(&slab, 1);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(slab_usable_size(&slab, ptr), slab_class_slot_size(&slab, 0));

    // 65-byte request should get 128-byte slot
    slab_free(&slab, ptr);
    ptr = slab_alloc(&slab, 65);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(slab_usable_size(&slab, ptr), slab_class_slot_size(&slab, 2));

    slab_free(&slab, ptr);
    slab_destroy(&slab);
}

TEST(test_class_isolation) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128};
    slab_t slab;
    void *ptr32a, *ptr32b, *ptr64;
    slab_class_stats_t stats32_before, stats32_after;
    slab_class_stats_t stats64_before, stats64_after;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 3), SLAB_OK);

    ptr32a = slab_alloc(&slab, 32);
    ptr64 = slab_alloc(&slab, 64);
    ptr32b = slab_alloc(&slab, 32);

    stats32_before = slab_class_stats(&slab, 0);
    stats64_before = slab_class_stats(&slab, 1);

    // free in 64-byte class
    slab_free(&slab, ptr64);

    stats32_after = slab_class_stats(&slab, 0);
    stats64_after = slab_class_stats(&slab, 1);

    // 32-byte class should be unchanged
    ASSERT_EQ(stats32_before.used_slots, stats32_after.used_slots);

    // 64-byte class should have one more free
    ASSERT_EQ(stats64_before.free_slots + 1, stats64_after.free_slots);

    slab_free(&slab, ptr32a);
    slab_free(&slab, ptr32b);
    slab_destroy(&slab);
}

TEST(test_exhaust_one_class) {
    uint8_t buffer[4096];
    size_t sizes[] = {64, 256};
    slab_t slab;
    slab_class_stats_t stats64, stats256;
    void **ptrs64;
    void *ptr256;
    size_t i, count;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 2), SLAB_OK);

    stats64 = slab_class_stats(&slab, 0);
    count = stats64.total_slots;
    ptrs64 = (void **)malloc(count * sizeof(void *));
    ASSERT_NOT_NULL(ptrs64);

    // exhaust 64-byte class
    for (i = 0; i < count; i++) {
        ptrs64[i] = slab_alloc(&slab, 64);
        ASSERT_NOT_NULL(ptrs64[i]);
    }

    // next 64-byte alloc should fail
    ASSERT_NULL(slab_alloc(&slab, 64));

    // but 256-byte class should still work
    stats256 = slab_class_stats(&slab, 1);
    ASSERT(stats256.free_slots > 0);

    ptr256 = slab_alloc(&slab, 256);
    ASSERT_NOT_NULL(ptr256);

    slab_free(&slab, ptr256);
    for (i = 0; i < count; i++) {
        slab_free(&slab, ptrs64[i]);
    }
    free(ptrs64);

    slab_destroy(&slab);
}

TEST(test_too_large) {
    uint8_t buffer[4096];
    size_t sizes[] = {32, 64, 128};
    slab_t slab;
    void *ptr;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 3), SLAB_OK);

    // request larger than max class should return NULL
    ptr = slab_alloc(&slab, 129);
    ASSERT_NULL(ptr);

    ptr = slab_alloc(&slab, 1024);
    ASSERT_NULL(ptr);

    ptr = slab_alloc(&slab, SIZE_MAX);
    ASSERT_NULL(ptr);

    slab_destroy(&slab);
}

TEST(test_free_returns_to_class) {
    uint8_t buffer[2048];
    size_t sizes[] = {64};
    slab_t slab;
    void *ptr1, *ptr2;
    slab_class_stats_t stats_before, stats_after;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    ptr1 = slab_alloc(&slab, 64);
    ASSERT_NOT_NULL(ptr1);

    stats_before = slab_class_stats(&slab, 0);
    slab_free(&slab, ptr1);
    stats_after = slab_class_stats(&slab, 0);

    // one more free slot after freeing
    ASSERT_EQ(stats_before.free_slots + 1, stats_after.free_slots);

    // allocating again should give us the same slot (LIFO)
    ptr2 = slab_alloc(&slab, 64);
    ASSERT_EQ(ptr1, ptr2);

    slab_free(&slab, ptr2);
    slab_destroy(&slab);
}

TEST(test_alloc_zero_size) {
    uint8_t buffer[1024];
    size_t sizes[] = {32};
    slab_t slab;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    ASSERT_NULL(slab_alloc(&slab, 0));

    slab_destroy(&slab);
}

TEST(test_free_null) {
    uint8_t buffer[1024];
    size_t sizes[] = {32};
    slab_t slab;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    // should not crash
    slab_free(&slab, NULL);

    slab_destroy(&slab);
}

TEST(test_reset) {
    uint8_t buffer[4096];
    size_t sizes[] = {32, 64, 128};
    slab_t slab;
    slab_stats_t stats_init, stats_used, stats_reset;
    void *ptr1, *ptr2, *ptr3;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 3), SLAB_OK);

    stats_init = slab_stats(&slab);

    ptr1 = slab_alloc(&slab, 32);
    ptr2 = slab_alloc(&slab, 64);
    ptr3 = slab_alloc(&slab, 128);
    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_NOT_NULL(ptr3);

    stats_used = slab_stats(&slab);
    ASSERT_EQ(stats_used.used_slots, 3);

    slab_reset(&slab);

    stats_reset = slab_stats(&slab);

    // should be back to initial state
    ASSERT_EQ(stats_reset.total_slots, stats_init.total_slots);
    ASSERT_EQ(stats_reset.free_slots, stats_init.free_slots);
    ASSERT_EQ(stats_reset.used_slots, 0);

    // old pointers are invalid; new allocations should work
    ptr1 = slab_alloc(&slab, 32);
    ASSERT_NOT_NULL(ptr1);

    slab_free(&slab, ptr1);
    slab_destroy(&slab);
}

TEST(test_stats_per_class) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128, 256};
    slab_t slab;
    slab_class_stats_t cs;
    void *ptr;
    size_t i;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 4), SLAB_OK);

    for (i = 0; i < 4; i++) {
        cs = slab_class_stats(&slab, i);
        ASSERT(cs.slot_size >= sizes[i]); // aligned size >= requested
        ASSERT_EQ(cs.used_slots, 0);
        ASSERT_EQ(cs.free_slots, cs.total_slots);
    }

    // allocate from class 1
    ptr = slab_alloc(&slab, 64);
    ASSERT_NOT_NULL(ptr);

    cs = slab_class_stats(&slab, 1);
    ASSERT_EQ(cs.used_slots, 1);
    ASSERT_EQ(cs.free_slots, cs.total_slots - 1);

    // class 0 should be unaffected
    cs = slab_class_stats(&slab, 0);
    ASSERT_EQ(cs.used_slots, 0);

    slab_free(&slab, ptr);
    slab_destroy(&slab);
}

TEST(test_aggregate_stats) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128};
    slab_t slab;
    slab_stats_t stats;
    void *ptrs[10];
    size_t i;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 3), SLAB_OK);

    stats = slab_stats(&slab);
    ASSERT_EQ(stats.class_count, 3);
    ASSERT_EQ(stats.used_slots, 0);
    ASSERT(stats.total_slots > 0);
    ASSERT_EQ(stats.free_slots, stats.total_slots);

    // allocate various sizes
    for (i = 0; i < 10; i++) {
        ptrs[i] = slab_alloc(&slab, 32 + (i % 3) * 32);
        ASSERT_NOT_NULL(ptrs[i]);
    }

    stats = slab_stats(&slab);
    ASSERT_EQ(stats.used_slots, 10);
    ASSERT_EQ(stats.free_slots, stats.total_slots - 10);

    for (i = 0; i < 10; i++) {
        slab_free(&slab, ptrs[i]);
    }

    slab_destroy(&slab);
}

TEST(test_ptr_to_class_mapping) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128, 256};
    slab_t slab;
    void *ptr32, *ptr64, *ptr128, *ptr256;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 4), SLAB_OK);

    ptr32 = slab_alloc(&slab, 32);
    ptr64 = slab_alloc(&slab, 64);
    ptr128 = slab_alloc(&slab, 128);
    ptr256 = slab_alloc(&slab, 256);

    // each pointer should map to correct class via usable_size
    ASSERT_EQ(slab_usable_size(&slab, ptr32), slab_class_slot_size(&slab, 0));
    ASSERT_EQ(slab_usable_size(&slab, ptr64), slab_class_slot_size(&slab, 1));
    ASSERT_EQ(slab_usable_size(&slab, ptr128), slab_class_slot_size(&slab, 2));
    ASSERT_EQ(slab_usable_size(&slab, ptr256), slab_class_slot_size(&slab, 3));

    slab_free(&slab, ptr32);
    slab_free(&slab, ptr64);
    slab_free(&slab, ptr128);
    slab_free(&slab, ptr256);

    slab_destroy(&slab);
}

TEST(test_slab_owns) {
    uint8_t buffer[4096];
    uint8_t external_buffer[64];
    size_t sizes[] = {32, 64, 128};
    slab_t slab;
    void *ptr;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 3), SLAB_OK);

    ptr = slab_alloc(&slab, 32);
    ASSERT_NOT_NULL(ptr);

    // should own its own allocations
    ASSERT(slab_owns(&slab, ptr) == 1);

    // should not own external pointers
    ASSERT(slab_owns(&slab, external_buffer) == 0);
    ASSERT(slab_owns(&slab, &slab) == 0);
    ASSERT(slab_owns(&slab, NULL) == 0);

    slab_free(&slab, ptr);
    slab_destroy(&slab);
}

TEST(test_alignment_all_classes) {
    uint8_t buffer[16384];
    size_t sizes[] = {17, 33, 65, 129, 257}; // non-aligned sizes
    slab_t slab;
    void *ptrs[5];
    size_t i;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 5), SLAB_OK);

    for (i = 0; i < 5; i++) {
        ptrs[i] = slab_alloc(&slab, sizes[i]);
        ASSERT_NOT_NULL(ptrs[i]);
        ASSERT(is_aligned(ptrs[i], SLAB_ALIGNMENT));
    }

    for (i = 0; i < 5; i++) {
        slab_free(&slab, ptrs[i]);
    }

    slab_destroy(&slab);
}

TEST(test_unaligned_buffer) {
    uint8_t raw_buffer[4096 + SLAB_ALIGNMENT];
    uint8_t *unaligned_buffer;
    size_t sizes[] = {32, 64};
    slab_t slab;
    void *ptr;

    // create intentionally unaligned buffer
    unaligned_buffer = raw_buffer;
    if (is_aligned(unaligned_buffer, SLAB_ALIGNMENT)) {
        unaligned_buffer++;
    }

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, unaligned_buffer, 4096, sizes, 2), SLAB_OK);

    // allocations should still be aligned
    ptr = slab_alloc(&slab, 32);
    ASSERT_NOT_NULL(ptr);
    ASSERT(is_aligned(ptr, SLAB_ALIGNMENT));

    slab_free(&slab, ptr);
    slab_destroy(&slab);
}

TEST(test_alloc_free_cycle) {
    uint8_t buffer[16384];
    size_t sizes[] = {32, 64, 128, 256};
    slab_t slab;
    void *ptrs[100];
    size_t i, j;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 4), SLAB_OK);

    for (j = 0; j < 10; j++) {
        // allocate batch
        for (i = 0; i < 100; i++) {
            size_t size = 32 << (i % 4);
            ptrs[i] = slab_alloc(&slab, size);
            // some may fail if class is exhausted
            if (ptrs[i]) {
                memset(ptrs[i], (int)i, 32);
            }
        }

        // free batch
        for (i = 0; i < 100; i++) {
            slab_free(&slab, ptrs[i]);
            ptrs[i] = NULL;
        }
    }

    slab_destroy(&slab);
}

TEST(test_exhaust_and_recover) {
    uint8_t buffer[2048];
    size_t sizes[] = {64};
    slab_t slab;
    slab_class_stats_t stats;
    void **ptrs;
    size_t i, total;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    stats = slab_class_stats(&slab, 0);
    total = stats.total_slots;
    ptrs = (void **)malloc(total * sizeof(void *));
    ASSERT_NOT_NULL(ptrs);

    // exhaust
    for (i = 0; i < total; i++) {
        ptrs[i] = slab_alloc(&slab, 64);
        ASSERT_NOT_NULL(ptrs[i]);
    }

    ASSERT_NULL(slab_alloc(&slab, 64));

    // free half
    for (i = 0; i < total / 2; i++) {
        slab_free(&slab, ptrs[i]);
        ptrs[i] = NULL;
    }

    // allocate again
    for (i = 0; i < total / 2; i++) {
        ptrs[i] = slab_alloc(&slab, 64);
        ASSERT_NOT_NULL(ptrs[i]);
    }

    // clean up
    for (i = 0; i < total; i++) {
        slab_free(&slab, ptrs[i]);
    }
    free(ptrs);

    slab_destroy(&slab);
}

TEST(test_buffer_size_needed) {
    size_t sizes[] = {32, 64, 128};
    size_t needed;
    uint8_t *buffer;
    slab_t slab;

    // calculate needed size for at least 1 slot each
    needed = slab_buffer_size_needed(sizes, 3, 1);
    ASSERT(needed > 0);

    // allocate that much and init should succeed
    buffer = (uint8_t *)malloc(needed);
    ASSERT_NOT_NULL(buffer);

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, needed, sizes, 3), SLAB_OK);

    // each class should have at least 1 slot
    ASSERT(slab_class_stats(&slab, 0).total_slots >= 1);
    ASSERT(slab_class_stats(&slab, 1).total_slots >= 1);
    ASSERT(slab_class_stats(&slab, 2).total_slots >= 1);

    slab_destroy(&slab);
    free(buffer);
}

TEST(test_buffer_size_needed_invalid) {
    size_t sizes[] = {32, 64};

    ASSERT_EQ(slab_buffer_size_needed(NULL, 2, 1), 0);
    ASSERT_EQ(slab_buffer_size_needed(sizes, 0, 1), 0);
    ASSERT_EQ(slab_buffer_size_needed(sizes, SLAB_MAX_CLASSES + 1, 1), 0);
}

TEST(test_class_slot_size) {
    uint8_t buffer[4096];
    size_t sizes[] = {32, 64, 128};
    slab_t slab;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 3), SLAB_OK);

    // slot sizes should be at least the requested sizes
    ASSERT(slab_class_slot_size(&slab, 0) >= 32);
    ASSERT(slab_class_slot_size(&slab, 1) >= 64);
    ASSERT(slab_class_slot_size(&slab, 2) >= 128);

    // invalid indices should return 0
    ASSERT_EQ(slab_class_slot_size(&slab, 3), 0);
    ASSERT_EQ(slab_class_slot_size(&slab, 100), 0);

    slab_destroy(&slab);
}

TEST(test_single_class) {
    uint8_t buffer[1024];
    size_t sizes[] = {64};
    slab_t slab;
    void *ptr1, *ptr2;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);
    ASSERT_EQ(slab_class_count(&slab), 1);

    ptr1 = slab_alloc(&slab, 1); // small request
    ptr2 = slab_alloc(&slab, 64); // exact request
    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(ptr2);

    // too large request fails
    ASSERT_NULL(slab_alloc(&slab, 65));

    slab_free(&slab, ptr1);
    slab_free(&slab, ptr2);
    slab_destroy(&slab);
}

TEST(test_max_classes) {
    uint8_t buffer[65536];
    size_t sizes[SLAB_MAX_CLASSES];
    slab_t slab;
    size_t i;

    for (i = 0; i < SLAB_MAX_CLASSES; i++) {
        sizes[i] = 32 * (i + 1);
    }

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, SLAB_MAX_CLASSES), SLAB_OK);
    ASSERT_EQ(slab_class_count(&slab), SLAB_MAX_CLASSES);

    slab_destroy(&slab);
}

TEST(test_operations_on_destroyed_slab) {
    uint8_t buffer[1024];
    size_t sizes[] = {32};
    slab_t slab;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    slab_destroy(&slab);

    // operations on destroyed slab should be safe
    ASSERT_NULL(slab_alloc(&slab, 32));
    ASSERT_EQ(slab_class_count(&slab), 0);
    ASSERT_EQ(slab_owns(&slab, buffer), 0);

    // re-destroy should be safe
    slab_destroy(&slab);
}

TEST(test_operations_on_null_slab) {
    slab_stats_t stats;
    slab_class_stats_t cstats;

    ASSERT_NULL(slab_alloc(NULL, 32));
    slab_free(NULL, NULL);
    slab_reset(NULL);

    stats = slab_stats(NULL);
    ASSERT_EQ(stats.class_count, 0);

    cstats = slab_class_stats(NULL, 0);
    ASSERT_EQ(cstats.slot_size, 0);

    ASSERT_EQ(slab_class_count(NULL), 0);
    ASSERT_EQ(slab_class_slot_size(NULL, 0), 0);
    ASSERT_EQ(slab_owns(NULL, NULL), 0);
    ASSERT_EQ(slab_usable_size(NULL, NULL), 0);
}

TEST(test_memory_write_integrity) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128, 256};
    slab_t slab;
    void *ptrs[4];
    uint8_t *p;
    size_t i, j, size;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 4), SLAB_OK);

    // allocate and fill with patterns
    for (i = 0; i < 4; i++) {
        size = sizes[i];
        ptrs[i] = slab_alloc(&slab, size);
        ASSERT_NOT_NULL(ptrs[i]);
        memset(ptrs[i], (int)(i + 1) * 0x11, size);
    }

    // verify patterns are intact (no overlap)
    for (i = 0; i < 4; i++) {
        size = sizes[i];
        p = (uint8_t *)ptrs[i];
        for (j = 0; j < size; j++) {
            ASSERT_EQ(p[j], (i + 1) * 0x11);
        }
    }

    for (i = 0; i < 4; i++) {
        slab_free(&slab, ptrs[i]);
    }

    slab_destroy(&slab);
}

TEST(test_calloc_zeroed) {
    uint8_t buffer[4096];
    size_t sizes[] = {64, 128};
    slab_t slab;
    void *ptr;
    uint8_t *p;
    size_t i, slot_size;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 2), SLAB_OK);

    ptr = slab_calloc(&slab, 64);
    ASSERT_NOT_NULL(ptr);

    // entire slot should be zeroed
    slot_size = slab_usable_size(&slab, ptr);
    p = (uint8_t *)ptr;
    for (i = 0; i < slot_size; i++) {
        ASSERT_EQ(p[i], 0);
    }

    // write garbage, free, calloc again - should be zeroed
    memset(ptr, 0xAB, slot_size);
    slab_free(&slab, ptr);

    ptr = slab_calloc(&slab, 64);
    ASSERT_NOT_NULL(ptr);
    p = (uint8_t *)ptr;
    for (i = 0; i < slot_size; i++) {
        ASSERT_EQ(p[i], 0);
    }

    slab_free(&slab, ptr);
    slab_destroy(&slab);
}

TEST(test_calloc_null_on_failure) {
    uint8_t buffer[1024];
    size_t sizes[] = {64};
    slab_t slab;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    // request too large
    ASSERT_NULL(slab_calloc(&slab, 128));

    // zero size
    ASSERT_NULL(slab_calloc(&slab, 0));

    slab_destroy(&slab);
}

TEST(test_max_alloc) {
    uint8_t buffer[8192];
    size_t sizes[] = {32, 64, 128, 256};
    slab_t slab;
    size_t max_size;
    void *ptr;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 4), SLAB_OK);

    max_size = slab_max_alloc(&slab);

    // max should be at least 256
    ASSERT(max_size >= 256);

    // should be able to allocate max_size
    ptr = slab_alloc(&slab, max_size);
    ASSERT_NOT_NULL(ptr);
    slab_free(&slab, ptr);

    // should fail to allocate max_size + 1
    ASSERT_NULL(slab_alloc(&slab, max_size + 1));

    slab_destroy(&slab);
}

TEST(test_max_alloc_invalid) {
    ASSERT_EQ(slab_max_alloc(NULL), 0);
}

#ifdef SLAB_DEBUG

TEST(test_debug_peak_tracking) {
    uint8_t buffer[4096];
    size_t sizes[] = {64};
    slab_t slab;
    slab_class_stats_t stats;
    void *ptrs[10];
    size_t i;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    // allocate 10
    for (i = 0; i < 10; i++) {
        ptrs[i] = slab_alloc(&slab, 64);
        ASSERT_NOT_NULL(ptrs[i]);
    }

    stats = slab_class_stats(&slab, 0);
    ASSERT_EQ(stats.peak_used, 10);

    // free 5
    for (i = 0; i < 5; i++) {
        slab_free(&slab, ptrs[i]);
    }

    // peak should still be 10
    stats = slab_class_stats(&slab, 0);
    ASSERT_EQ(stats.peak_used, 10);
    ASSERT_EQ(stats.used_slots, 5);

    for (i = 5; i < 10; i++) {
        slab_free(&slab, ptrs[i]);
    }

    slab_destroy(&slab);
}

TEST(test_debug_alloc_count) {
    uint8_t buffer[4096];
    size_t sizes[] = {64};
    slab_t slab;
    slab_class_stats_t stats;
    void *ptr;
    size_t i;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    for (i = 0; i < 20; i++) {
        ptr = slab_alloc(&slab, 64);
        ASSERT_NOT_NULL(ptr);
        slab_free(&slab, ptr);
    }

    stats = slab_class_stats(&slab, 0);
    ASSERT_EQ(stats.alloc_count, 20);
    ASSERT_EQ(stats.free_count, 20);

    slab_destroy(&slab);
}

TEST(test_debug_poison_check) {
    uint8_t buffer[4096];
    size_t sizes[] = {64};
    slab_t slab;
    void *ptr;
    uint8_t *p;
    size_t i;
    int poison_found = 0;

    memset(&slab, 0, sizeof(slab));
    ASSERT_EQ(slab_init(&slab, buffer, sizeof(buffer), sizes, 1), SLAB_OK);

    ptr = slab_alloc(&slab, 64);
    ASSERT_NOT_NULL(ptr);

    // fill with known pattern
    memset(ptr, 0xAA, 64);

    slab_free(&slab, ptr);

    // after free, memory should be poisoned (except free list pointer)
    p = (uint8_t *)ptr;
    for (i = sizeof(void *); i < 64; i++) {
        if (p[i] == SLAB_POISON_BYTE) {
            poison_found = 1;
            break;
        }
    }
    ASSERT(poison_found);

    slab_destroy(&slab);
}

#endif /* SLAB_DEBUG */

int main(void) {
    printf("\n");
    printf(" slab allocator tests \n");
    printf("configuration:\n");
#ifdef SLAB_DEBUG
    printf("   SLAB_DEBUG: enabled\n");
#else
    printf("   SLAB_DEBUG: disabled\n");
#endif
    printf("   Alignment: %zu bytes\n", (size_t)SLAB_ALIGNMENT);

    RUN_TEST(test_init_basic);
    RUN_TEST(test_init_null_params);
    RUN_TEST(test_init_zero_params);
    RUN_TEST(test_init_too_many_classes);
    RUN_TEST(test_init_duplicate_sizes);
    RUN_TEST(test_init_zero_size_class);
    RUN_TEST(test_init_buffer_too_small);
    RUN_TEST(test_init_sizes_sorted);

    RUN_TEST(test_basic_multisize);
    RUN_TEST(test_size_rounding);
    RUN_TEST(test_class_isolation);
    RUN_TEST(test_exhaust_one_class);
    RUN_TEST(test_too_large);
    RUN_TEST(test_free_returns_to_class);
    RUN_TEST(test_alloc_zero_size);
    RUN_TEST(test_free_null);

    RUN_TEST(test_reset);

    RUN_TEST(test_stats_per_class);
    RUN_TEST(test_aggregate_stats);

    RUN_TEST(test_ptr_to_class_mapping);
    RUN_TEST(test_slab_owns);

    RUN_TEST(test_alignment_all_classes);
    RUN_TEST(test_unaligned_buffer);

    RUN_TEST(test_alloc_free_cycle);
    RUN_TEST(test_exhaust_and_recover);

    RUN_TEST(test_buffer_size_needed);
    RUN_TEST(test_buffer_size_needed_invalid);
    RUN_TEST(test_class_slot_size);

    RUN_TEST(test_single_class);
    RUN_TEST(test_max_classes);
    RUN_TEST(test_operations_on_destroyed_slab);
    RUN_TEST(test_operations_on_null_slab);
    RUN_TEST(test_memory_write_integrity);
    RUN_TEST(test_calloc_zeroed);
    RUN_TEST(test_calloc_null_on_failure);
    RUN_TEST(test_max_alloc);
    RUN_TEST(test_max_alloc_invalid);

#ifdef SLAB_DEBUG
    RUN_TEST(test_debug_peak_tracking);
    RUN_TEST(test_debug_alloc_count);
    RUN_TEST(test_debug_poison_check);
#endif

    printf("    %d/%d tests passed\n", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf("   \033[31m%d TESTS FAILED\033[0m\n", tests_failed);
    } else {
        printf("   \033[32mALL TESTS PASSED\033[0m\n");
    }

    return tests_failed > 0 ? 1 : 0;
}