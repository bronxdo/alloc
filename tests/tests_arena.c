/*
 * tests for arena.h
 *
 *   # basic tests
 *   gcc -Wall -Wextra -O2 -o test_arena test_arena.c && ./test_arena
 *
 *   # With debug features
 *   gcc -Wall -Wextra -DARENA_DEBUG -O2 -o test_arena_debug test_arena.c && ./test_arena_debug
 *
 *   # With block chaining
 *   gcc -Wall -Wextra -DARENA_BLOCK_CHAINING -O2 -o test_arena_chain test_arena.c && ./test_arena_chain
 *
 *   # With both debug and chaining
 *   gcc -Wall -Wextra -DARENA_DEBUG -DARENA_BLOCK_CHAINING -O2 -o test_arena_full test_arena.c && ./test_arena_full
 *
 */

#define ARENA_IMPLEMENTATION
#include "../arena.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>


static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_BUFFER_SIZE 4096
#define LARGE_BUFFER_SIZE 65536

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

static bool is_aligned(const void *ptr, size_t align) {
    return ((uintptr_t)ptr % align) == 0;
}

TEST(test_init_valid) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;

    bool result = arena_init(&arena, buffer, sizeof(buffer));

    ASSERT(result == true);
    ASSERT(arena_is_valid(&arena));
    ASSERT_EQ(arena_capacity(&arena), TEST_BUFFER_SIZE);
    ASSERT_EQ(arena_used(&arena), 0);
    ASSERT_EQ(arena_remaining(&arena), TEST_BUFFER_SIZE);

    arena_destroy(&arena);
}

TEST(test_init_null_arena) {
    uint8_t buffer[TEST_BUFFER_SIZE];

    bool result = arena_init(NULL, buffer, sizeof(buffer));

    ASSERT(result == false);
}

TEST(test_init_null_buffer_nonzero_size) {
    arena_t arena;

    bool result = arena_init(&arena, NULL, 100);

    ASSERT(result == false);
}

TEST(test_init_null_buffer_zero_size) {
    arena_t arena;

    // zero size arena with NULL buffer should be valid (quirky edge case this is)
    bool result = arena_init(&arena, NULL, 0);

    ASSERT(result == true);
    ASSERT_EQ(arena_capacity(&arena), 0);
    ASSERT_EQ(arena_remaining(&arena), 0);

    arena_destroy(&arena);
}

TEST(test_basic_alloc) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    void *ptr1 = arena_alloc(&arena, 100);
    ASSERT_NOT_NULL(ptr1);
    ASSERT(arena_used(&arena) >= 100);

    void *ptr2 = arena_alloc(&arena, 200);
    ASSERT_NOT_NULL(ptr2);
    ASSERT(arena_used(&arena) >= 300);

    ASSERT(ptr1 != ptr2);

    ASSERT((uint8_t *)ptr1 >= buffer);
    ASSERT((uint8_t *)ptr1 < buffer + sizeof(buffer));
    ASSERT((uint8_t *)ptr2 >= buffer);
    ASSERT((uint8_t *)ptr2 < buffer + sizeof(buffer));

    arena_destroy(&arena);
}

TEST(test_alloc_advances_pointer) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    size_t initial = arena_used(&arena);
    ASSERT_EQ(initial, 0);

    arena_alloc(&arena, 64);
    ASSERT(arena_used(&arena) >= 64);

    arena_alloc(&arena, 128);
    ASSERT(arena_used(&arena) >= 192);

    arena_destroy(&arena);
}

TEST(test_alloc_can_write_memory) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    char *str = arena_alloc(&arena, 256);
    ASSERT_NOT_NULL(str);
    strcpy(str, "hello, arena");
    ASSERT(strcmp(str, "hello, arena") == 0);

    int *nums = arena_alloc(&arena, 10 * sizeof(int));
    ASSERT_NOT_NULL(nums);
    for (int i = 0; i < 10; i++) {
        nums[i] = i * i;
    }
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(nums[i], i * i);
    }

    arena_destroy(&arena);
}


TEST(test_default_alignment) {
    uint8_t buffer[TEST_BUFFER_SIZE] __attribute__((aligned(64)));
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    for (int i = 0; i < 20; i++) {
        void *ptr = arena_alloc(&arena, 1 + (i * 7)); // various sizes
        ASSERT_NOT_NULL(ptr);
        ASSERT(is_aligned(ptr, sizeof(max_align_t)));
    }

    arena_destroy(&arena);
}

TEST(test_alignment_explicit_small) {
    uint8_t buffer[TEST_BUFFER_SIZE] __attribute__((aligned(64)));
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    void *ptr1 = arena_alloc_aligned(&arena, 10, 1);
    ASSERT_NOT_NULL(ptr1);
    ASSERT(is_aligned(ptr1, 1));

    void *ptr2 = arena_alloc_aligned(&arena, 10, 2);
    ASSERT_NOT_NULL(ptr2);
    ASSERT(is_aligned(ptr2, 2));

    void *ptr4 = arena_alloc_aligned(&arena, 10, 4);
    ASSERT_NOT_NULL(ptr4);
    ASSERT(is_aligned(ptr4, 4));

    void *ptr8 = arena_alloc_aligned(&arena, 10, 8);
    ASSERT_NOT_NULL(ptr8);
    ASSERT(is_aligned(ptr8, 8));

    arena_destroy(&arena);
}

TEST(test_alignment_explicit_large) {
    uint8_t *buffer = aligned_alloc(4096, LARGE_BUFFER_SIZE);
    ASSERT_NOT_NULL(buffer);

    arena_t arena;
    arena_init(&arena, buffer, LARGE_BUFFER_SIZE);

    // Test 64 byte alignment (cache line)
    void *ptr64 = arena_alloc_aligned(&arena, 100, 64);
    ASSERT_NOT_NULL(ptr64);
    ASSERT(is_aligned(ptr64, 64));

    // Test 128 byte alignment
    void *ptr128 = arena_alloc_aligned(&arena, 100, 128);
    ASSERT_NOT_NULL(ptr128);
    ASSERT(is_aligned(ptr128, 128));

    // Test 256 byte alignment
    void *ptr256 = arena_alloc_aligned(&arena, 100, 256);
    ASSERT_NOT_NULL(ptr256);
    ASSERT(is_aligned(ptr256, 256));

    // Test 4096 byte alignment (page)
    void *ptr4096 = arena_alloc_aligned(&arena, 100, 4096);
    ASSERT_NOT_NULL(ptr4096);
    ASSERT(is_aligned(ptr4096, 4096));

    arena_destroy(&arena);
    free(buffer);
}

TEST(test_alignment_preserves_previous) {
    uint8_t buffer[TEST_BUFFER_SIZE] __attribute__((aligned(64)));
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // trying to allocate with various alignments and verify none corrupt previous
    char *str1 = arena_alloc(&arena, 50);
    ASSERT_NOT_NULL(str1);
    strcpy(str1, "First string");

    void *aligned = arena_alloc_aligned(&arena, 100, 64);
    ASSERT_NOT_NULL(aligned);
    ASSERT(is_aligned(aligned, 64));

    char *str2 = arena_alloc(&arena, 50);
    ASSERT_NOT_NULL(str2);
    strcpy(str2, "Second string");

    // trying to verify if the first string is still intact
    ASSERT(strcmp(str1, "First string") == 0);
    ASSERT(strcmp(str2, "Second string") == 0);

    arena_destroy(&arena);
}


TEST(test_reset) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc(&arena, 1000);
    arena_alloc(&arena, 500);
    ASSERT(arena_used(&arena) >= 1500);

    arena_reset(&arena);
    ASSERT_EQ(arena_used(&arena), 0);
    ASSERT_EQ(arena_remaining(&arena), TEST_BUFFER_SIZE);

    void *ptr = arena_alloc(&arena, 100);
    ASSERT_NOT_NULL(ptr);

    arena_destroy(&arena);
}

TEST(test_reset_memory_reusable) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    while (arena_alloc(&arena, 100) != NULL) {
    }

    size_t capacity = arena_capacity(&arena);

    arena_reset(&arena);

    size_t total_allocated = 0;
    void *ptr;
    while ((ptr = arena_alloc(&arena, 100)) != NULL) {
        total_allocated += 100;
    }

    ASSERT(total_allocated > capacity / 2);

    arena_destroy(&arena);
}

TEST(test_save_restore) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc(&arena, 100);

    // save the position
    arena_marker_t marker = arena_save(&arena);
    size_t saved_used = arena_used(&arena);

    arena_alloc(&arena, 500);
    arena_alloc(&arena, 300);
    ASSERT(arena_used(&arena) > saved_used);

    // restore to marker
    arena_reset_to(&arena, marker);
    ASSERT_EQ(arena_used(&arena), saved_used);

    arena_destroy(&arena);
}

TEST(test_save_restore_multiple) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc(&arena, 100);
    arena_marker_t m1 = arena_save(&arena);

    arena_alloc(&arena, 200);
    arena_marker_t m2 = arena_save(&arena);

    arena_alloc(&arena, 300);

    // Save m3 but don't use it , just for testing if multiple saves work
    (void)arena_save(&arena);

    arena_alloc(&arena, 400);

    arena_reset_to(&arena, m2);
    size_t after_m2 = arena_used(&arena);

    arena_reset_to(&arena, m1);
    ASSERT(arena_used(&arena) < after_m2);

    ASSERT(arena_used(&arena) <= m1.offset + sizeof(max_align_t));

    arena_destroy(&arena);
}

TEST(test_save_restore_preserves_data) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    char *str1 = arena_alloc(&arena, 50);
    strcpy(str1, "persistent data");

    arena_marker_t marker = arena_save(&arena);

    char *str2 = arena_alloc(&arena, 50);
    strcpy(str2, "temp data");

    arena_reset_to(&arena, marker);

    ASSERT(strcmp(str1, "persistent data") == 0);

    arena_destroy(&arena);
}


TEST(test_overflow_returns_null) {
    uint8_t buffer[256];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    void *ptr = arena_alloc(&arena, 1000);
    ASSERT_NULL(ptr);

    // arena should still be usable
    void *small = arena_alloc(&arena, 10);
    ASSERT_NOT_NULL(small);

    arena_destroy(&arena);
}

TEST(test_exact_capacity) {
    // using unaligned buffer size to test edge case
    uint8_t buffer[256];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // we allocating in chunks until we can't anymore
    size_t alloc_count = 0;
    void *ptr;
    while ((ptr = arena_alloc(&arena, 16)) != NULL) {
        alloc_count++;
    }

    // should have made at least some allocations accounting for default alignment which can be up to 32 bytes
    ASSERT(alloc_count >= 4);

    // remaining should be less than one allocation's worth
    ASSERT(arena_remaining(&arena) < 16 + ARENA_DEFAULT_ALIGN);

    arena_destroy(&arena);
}

TEST(test_full_capacity) {
    uint8_t buffer[256];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // try to allocate exactly the capacity considering alignment
    void *ptr = arena_alloc_aligned(&arena, 256, 1);
    ASSERT_NOT_NULL(ptr);

    ASSERT_EQ(arena_remaining(&arena), 0);

    // any more  allocations should fail
    ASSERT_NULL(arena_alloc(&arena, 1));

    arena_destroy(&arena);
}

TEST(test_near_overflow_size) {
    uint8_t buffer[256];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // try to allocate SIZE_MAX should fail safely, not overflow
    void *ptr = arena_alloc(&arena, SIZE_MAX);
    ASSERT_NULL(ptr);

    // but arena should still work
    void *small = arena_alloc(&arena, 10);
    ASSERT_NOT_NULL(small);

    arena_destroy(&arena);
}

TEST(test_near_overflow_alignment) {
    uint8_t buffer[256];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc_aligned(&arena, 200, 1);

    // trying allocation that would overflow with alignment padding
    void *ptr = arena_alloc_aligned(&arena, 100, 64);
    ASSERT_NULL(ptr);

    arena_destroy(&arena);
}

TEST(test_zero_size_alloc) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // zero size allocation should return non null
    void *ptr = arena_alloc(&arena, 0);
    ASSERT_NOT_NULL(ptr);

    /* should not advance the arena or maybe advance slightly */
    size_t used_after = arena_used(&arena);
    ASSERT_EQ(used_after, 0);

    arena_destroy(&arena);
}

TEST(test_zero_size_alloc_multiple) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    void *ptr1 = arena_alloc(&arena, 0);
    void *ptr2 = arena_alloc(&arena, 0);
    void *ptr3 = arena_alloc(&arena, 0);

    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(ptr2);
    ASSERT_NOT_NULL(ptr3);

    ASSERT(ptr1 == ptr2);
    ASSERT(ptr2 == ptr3);

    arena_destroy(&arena);
}

TEST(test_zero_size_interleaved) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    void *ptr1 = arena_alloc(&arena, 100);
    void *zero = arena_alloc(&arena, 0);
    void *ptr2 = arena_alloc(&arena, 100);

    ASSERT_NOT_NULL(ptr1);
    ASSERT_NOT_NULL(zero);
    ASSERT_NOT_NULL(ptr2);
    ASSERT((uintptr_t)zero > (uintptr_t)ptr1);
    ASSERT((uintptr_t)ptr2 >= (uintptr_t)zero);

    arena_destroy(&arena);
}


TEST(test_alloc_zero_is_zeroed) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // fill buffer with stupid garbage first
    memset(buffer, 0xFF, sizeof(buffer));

    arena_reset(&arena);

    uint8_t *ptr = arena_alloc_zero(&arena, 100);
    ASSERT_NOT_NULL(ptr);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(ptr[i], 0);
    }

    arena_destroy(&arena);
}

TEST(test_alloc_zero_aligned) {
    uint8_t buffer[TEST_BUFFER_SIZE] __attribute__((aligned(64)));
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    uint8_t *ptr = arena_alloc_zero_aligned(&arena, 100, 64);
    ASSERT_NOT_NULL(ptr);
    ASSERT(is_aligned(ptr, 64));

    // verify zeroed
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(ptr[i], 0);
    }

    arena_destroy(&arena);
}


typedef struct {
    int x;
    double y;
    char name[32];
} TestStruct;

TEST(test_arena_new) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    TestStruct *s = arena_new(&arena, TestStruct);
    ASSERT_NOT_NULL(s);
    ASSERT(is_aligned(s, _Alignof(TestStruct)));

    // this should be still be usable
    s->x = 42;
    s->y = 3.14159;
    strcpy(s->name, "Test");

    ASSERT_EQ(s->x, 42);

    arena_destroy(&arena);
}

TEST(test_arena_new_zero) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    memset(buffer, 0xFF, sizeof(buffer));
    arena_reset(&arena);

    TestStruct *s = arena_new_zero(&arena, TestStruct);
    ASSERT_NOT_NULL(s);

    ASSERT_EQ(s->x, 0);
    ASSERT(s->y == 0.0);
    ASSERT(s->name[0] == '\0');

    arena_destroy(&arena);
}

TEST(test_arena_new_array) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    int *arr = arena_new_array(&arena, int, 100);
    ASSERT_NOT_NULL(arr);
    ASSERT(is_aligned(arr, _Alignof(int)));

    for (int i = 0; i < 100; i++) {
        arr[i] = i;
    }
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(arr[i], i);
    }

    arena_destroy(&arena);
}

TEST(test_arena_new_array_zero) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    memset(buffer, 0xFF, sizeof(buffer));
    arena_reset(&arena);

    int *arr = arena_new_array_zero(&arena, int, 100);
    ASSERT_NOT_NULL(arr);

    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(arr[i], 0);
    }

    arena_destroy(&arena);
}


TEST(test_temp_scope_basic) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc(&arena, 100);
    size_t before = arena_used(&arena);

    {
        arena_temp_t temp = arena_temp_begin(&arena);

        arena_alloc(&arena, 500);
        arena_alloc(&arena, 300);
        ASSERT(arena_used(&arena) > before);

        arena_temp_end(&temp);
    }

    ASSERT_EQ(arena_used(&arena), before);

    arena_destroy(&arena);
}

TEST(test_temp_scope_nested) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc(&arena, 100);
    size_t level0 = arena_used(&arena);

    arena_temp_t temp1 = arena_temp_begin(&arena);
    arena_alloc(&arena, 200);
    size_t level1 = arena_used(&arena);

    arena_temp_t temp2 = arena_temp_begin(&arena);
    arena_alloc(&arena, 300);

    arena_temp_end(&temp2);
    ASSERT_EQ(arena_used(&arena), level1);

    arena_temp_end(&temp1);
    ASSERT_EQ(arena_used(&arena), level0);

    arena_destroy(&arena);
}


TEST(test_stats_basic) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_stats_t stats = arena_stats(&arena);
    ASSERT_EQ(stats.capacity, TEST_BUFFER_SIZE);
    ASSERT_EQ(stats.used, 0);
    ASSERT_EQ(stats.remaining, TEST_BUFFER_SIZE);

    arena_alloc(&arena, 100);
    arena_alloc(&arena, 200);

    stats = arena_stats(&arena);
    ASSERT(stats.used >= 300);
    ASSERT(stats.remaining == stats.capacity - stats.used);

    arena_destroy(&arena);
}


#ifdef ARENA_BLOCK_CHAINING

TEST(test_dynamic_init) {
    arena_t arena;

    bool result = arena_init_dynamic(&arena, 1024);
    ASSERT(result == true);
    ASSERT(arena_is_valid(&arena));
    ASSERT(arena_capacity(&arena) >= 1024);

    arena_destroy(&arena);
}

TEST(test_block_chaining_grows) {
    arena_t arena;
    arena_init_dynamic(&arena, 256);

    size_t initial_capacity = arena_capacity(&arena);

    size_t total_alloc = 0;
    size_t target = initial_capacity + 1000;  // definitely excedds the first block

    while (total_alloc < target) {
        void *ptr = arena_alloc(&arena, 500);
        ASSERT_NOT_NULL(ptr);
        total_alloc += 500;
    }

    // capacity should have grown
    size_t final_capacity = arena_capacity(&arena);
    ASSERT(final_capacity > initial_capacity);

    arena_stats_t stats = arena_stats(&arena);
    ASSERT(stats.block_count > 1);

    arena_destroy(&arena);
}

TEST(test_block_chaining_large_alloc) {
    arena_t arena;
    arena_init_dynamic(&arena, 256);

    void *ptr = arena_alloc(&arena, ARENA_BLOCK_MIN_SIZE * 2);
    ASSERT_NOT_NULL(ptr);

    arena_destroy(&arena);
}

TEST(test_block_chaining_reset) {
    arena_t arena;
    arena_init_dynamic(&arena, 256);

    // we get the initial capacity , it will be at least ARENA_BLOCK_MIN_SIZE
    size_t initial_capacity = arena_capacity(&arena);

    size_t total_alloc = 0;
    size_t target = initial_capacity + 1000;

    while (total_alloc < target) {
        arena_alloc(&arena, 500);
        total_alloc += 500;
    }

    arena_stats_t before = arena_stats(&arena);
    ASSERT(before.block_count > 1);
    size_t block_count_before = before.block_count;

    // reset keeps blocks alive avoids malloc/free churn for loops
    arena_reset(&arena);

    arena_stats_t after = arena_stats(&arena);
    ASSERT_EQ(after.used, 0);
    // blocks are preserved on reset for perf
    ASSERT_EQ(after.block_count, block_count_before);

    // should be able to allocate again without new mallocs
    void *ptr = arena_alloc(&arena, 100);
    ASSERT_NOT_NULL(ptr);

    arena_destroy(&arena);
}

TEST(test_block_chaining_save_restore) {
    arena_t arena;
    arena_init_dynamic(&arena, 256);

    arena_alloc(&arena, 100);
    arena_marker_t marker = arena_save(&arena);

    for (int i = 0; i < 10; i++) {
        arena_alloc(&arena, 100);
    }

    arena_reset_to(&arena, marker);

    ASSERT(arena_used(&arena) <= marker.offset + sizeof(max_align_t));

    arena_destroy(&arena);
}

TEST(test_block_chaining_user_buffer_no_grow) {
    // When using user provided buffer, this should not grow
    uint8_t buffer[256];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    while (arena_alloc(&arena, 32)) {

    }

    // any further allocation should fail , there should be no growth for user buffer
    ASSERT_NULL(arena_alloc(&arena, 32));

    arena_destroy(&arena);
}

#endif /* ARENA_BLOCK_CHAINING */

#ifdef ARENA_DEBUG

TEST(test_debug_stats_tracking) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_stats_t stats = arena_stats(&arena);
    ASSERT_EQ(stats.alloc_count, 0);
    ASSERT_EQ(stats.total_requested, 0);

    arena_alloc(&arena, 100);
    arena_alloc(&arena, 200);
    arena_alloc(&arena, 50);

    stats = arena_stats(&arena);
    ASSERT_EQ(stats.alloc_count, 3);
    ASSERT_EQ(stats.total_requested, 350);

    arena_destroy(&arena);
}

TEST(test_debug_peak_tracking) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc(&arena, 500);
    arena_alloc(&arena, 300);

    arena_marker_t marker = arena_save(&arena);
    size_t peak = arena_used(&arena);

    arena_reset_to(&arena, marker);
    arena_reset(&arena);

    arena_stats_t stats = arena_stats(&arena);
    ASSERT(stats.peak_usage >= peak - sizeof(max_align_t));

    arena_destroy(&arena);
}

TEST(test_debug_poison_on_reset) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    uint8_t *ptr = arena_alloc(&arena, 100);
    ASSERT_NOT_NULL(ptr);

    memset(ptr, 0x42, 100);

    size_t offset = (size_t)(ptr - buffer);

    arena_reset(&arena);

    // memory at the allocation location should be poisoned
    int poisoned_count = 0;
    for (int i = 0; i < 100; i++) {
        if (buffer[offset + i] == ARENA_POISON_FREED) {
            poisoned_count++;
        }
    }

    // should have poisoned most of it atleast
    ASSERT(poisoned_count >= 90);

    arena_destroy(&arena);
}

TEST(test_debug_name) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_set_name(&arena, "TestArena");
    // just making suring it doesn't crash

    arena_destroy(&arena);
}

TEST(test_debug_integrity_check) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    ASSERT(arena_check_integrity(&arena));

    arena_alloc(&arena, 100);
    arena_alloc(&arena, 200);

    ASSERT(arena_check_integrity(&arena));

    arena_reset(&arena);

    ASSERT(arena_check_integrity(&arena));

    arena_destroy(&arena);
}

TEST(test_debug_allocation_tracking) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    bool result = arena_enable_tracking(&arena, 100);
    ASSERT(result == true);

    arena_alloc(&arena, 100);
    arena_alloc(&arena, 200);
    arena_alloc(&arena, 50);

    // tracking should have recorded these , we can't easily verify the records directly but other stats should have shown them

    arena_destroy(&arena);
}

#endif /* ARENA_DEBUG */



TEST(test_stress_many_small_allocs) {
    uint8_t *buffer = malloc(LARGE_BUFFER_SIZE);
    ASSERT_NOT_NULL(buffer);

    arena_t arena;
    arena_init(&arena, buffer, LARGE_BUFFER_SIZE);

    int count = 0;
    while (arena_alloc(&arena, 16) != NULL) {
        count++;
    }

    ASSERT(count > 1000);

    arena_destroy(&arena);
    free(buffer);
}

TEST(test_stress_various_sizes) {
    uint8_t *buffer = malloc(LARGE_BUFFER_SIZE);
    ASSERT_NOT_NULL(buffer);

    arena_t arena;
    arena_init(&arena, buffer, LARGE_BUFFER_SIZE);

    for (int i = 0; i < 100; i++) {
        size_t size = 1 + (i * 37 % 512);  // pseudo random sizes
        void *ptr = arena_alloc(&arena, size);
        if (ptr == NULL) break;

        memset(ptr, (uint8_t)i, size);
    }

    arena_destroy(&arena);
    free(buffer);
}

TEST(test_stress_save_restore_loop) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    for (int i = 0; i < 100; i++) {
        arena_marker_t marker = arena_save(&arena);

        arena_alloc(&arena, 50);
        arena_alloc(&arena, 30);

        arena_reset_to(&arena, marker);
    }

    ASSERT(arena_remaining(&arena) > TEST_BUFFER_SIZE - 100);

    arena_destroy(&arena);
}

TEST(test_stress_temp_scopes) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_alloc(&arena, 100);
    size_t baseline = arena_used(&arena);

    for (int i = 0; i < 100; i++) {
        arena_temp_t temp = arena_temp_begin(&arena);

        arena_alloc(&arena, 100);
        arena_alloc(&arena, 50);

        arena_temp_end(&temp);
    }

    ASSERT_EQ(arena_used(&arena), baseline);

    arena_destroy(&arena);
}



TEST(test_destroy_uninitialized) {
    arena_t arena = {0};  // zero initialized, not properly inited this should not crash

    arena_destroy(&arena);
}

TEST(test_destroy_twice) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_destroy(&arena);
    arena_destroy(&arena);
}

TEST(test_operations_on_destroyed) {
    uint8_t buffer[TEST_BUFFER_SIZE];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    arena_destroy(&arena);

    // These should handle gracefully by returning safe defaults
    ASSERT(!arena_is_valid(&arena));
    ASSERT_EQ(arena_remaining(&arena), 0);
    ASSERT_EQ(arena_capacity(&arena), 0);
    ASSERT_EQ(arena_used(&arena), 0);
}

TEST(test_tiny_arena) {
    uint8_t buffer[1];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    void *ptr = arena_alloc_aligned(&arena, 1, 1);
    ASSERT_NOT_NULL(ptr);

    ASSERT_NULL(arena_alloc_aligned(&arena, 1, 1));

    arena_destroy(&arena);
}


int main(void) {
    printf("\n");
    printf(" arena allocator tests \n");
    printf("configuration:\n");
#ifdef ARENA_DEBUG
    printf("   ARENA_DEBUG: enabled\n");
#else
    printf("   ARENA_DEBUG: disabled\n");
#endif
#ifdef ARENA_BLOCK_CHAINING
    printf("   ARENA_BLOCK_CHAINING: enabled\n");
#else
    printf("   ARENA_BLOCK_CHAINING: disabled\n");
#endif
    printf("   Default alignment: %zu bytes\n", (size_t)ARENA_DEFAULT_ALIGN);

    RUN_TEST(test_init_valid);
    RUN_TEST(test_init_null_arena);
    RUN_TEST(test_init_null_buffer_nonzero_size);
    RUN_TEST(test_init_null_buffer_zero_size);

    RUN_TEST(test_basic_alloc);
    RUN_TEST(test_alloc_advances_pointer);
    RUN_TEST(test_alloc_can_write_memory);

    RUN_TEST(test_default_alignment);
    RUN_TEST(test_alignment_explicit_small);
    RUN_TEST(test_alignment_explicit_large);
    RUN_TEST(test_alignment_preserves_previous);

    RUN_TEST(test_reset);
    RUN_TEST(test_reset_memory_reusable);
    RUN_TEST(test_save_restore);
    RUN_TEST(test_save_restore_multiple);
    RUN_TEST(test_save_restore_preserves_data);

    RUN_TEST(test_overflow_returns_null);
    RUN_TEST(test_exact_capacity);
    RUN_TEST(test_full_capacity);
    RUN_TEST(test_near_overflow_size);
    RUN_TEST(test_near_overflow_alignment);

    RUN_TEST(test_zero_size_alloc);
    RUN_TEST(test_zero_size_alloc_multiple);
    RUN_TEST(test_zero_size_interleaved);

    RUN_TEST(test_alloc_zero_is_zeroed);
    RUN_TEST(test_alloc_zero_aligned);

    RUN_TEST(test_arena_new);
    RUN_TEST(test_arena_new_zero);
    RUN_TEST(test_arena_new_array);
    RUN_TEST(test_arena_new_array_zero);

    RUN_TEST(test_temp_scope_basic);
    RUN_TEST(test_temp_scope_nested);

    RUN_TEST(test_stats_basic);

#ifdef ARENA_BLOCK_CHAINING
    RUN_TEST(test_dynamic_init);
    RUN_TEST(test_block_chaining_grows);
    RUN_TEST(test_block_chaining_large_alloc);
    RUN_TEST(test_block_chaining_reset);
    RUN_TEST(test_block_chaining_save_restore);
    RUN_TEST(test_block_chaining_user_buffer_no_grow);
#endif

#ifdef ARENA_DEBUG
    RUN_TEST(test_debug_stats_tracking);
    RUN_TEST(test_debug_peak_tracking);
    RUN_TEST(test_debug_poison_on_reset);
    RUN_TEST(test_debug_name);
    RUN_TEST(test_debug_integrity_check);
    RUN_TEST(test_debug_allocation_tracking);
#endif

    RUN_TEST(test_stress_many_small_allocs);
    RUN_TEST(test_stress_various_sizes);
    RUN_TEST(test_stress_save_restore_loop);
    RUN_TEST(test_stress_temp_scopes);

    RUN_TEST(test_destroy_uninitialized);
    RUN_TEST(test_destroy_twice);
    RUN_TEST(test_operations_on_destroyed);
    RUN_TEST(test_tiny_arena);

    printf("    %d/%d tests passed\n", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf("   \033[31m%d TESTS FAILED\033[0m\n", tests_failed);
    } else {
        printf("   \033[32mALL TESTS PASSED\033[0m\n");
    }

    return tests_failed > 0 ? 1 : 0;
}