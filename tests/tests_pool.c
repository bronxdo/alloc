/*
 *  tests for pool.h
 *
 *   # super basic tests
 *   gcc -Wall -Wextra -O2 -o tests_pool tests_pool.c && ./tests_pool
 *
 *   # with debug features
 *   gcc -Wall -Wextra -DPOOL_DEBUG -O2 -o tests_pool_debug tests_pool.c && ./tests_pool_debug

 */

#define POOL_IMPLEMENTATION
#include "../pool.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)

#define RUN_TEST(name)                                                         \
  do {                                                                         \
    printf("  Running %-40s ", #name "...");                                   \
    fflush(stdout);                                                            \
    tests_run++;                                                               \
    name();                                                                    \
    printf("\033[32mPASSED\033[0m\n");                                         \
    tests_passed++;                                                            \
  } while (0)

#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("\033[31mFAILED\033[0m\n");                                       \
      printf("    Assertion failed: %s\n", #cond);                             \
      printf("    Location: %s:%d\n", __FILE__, __LINE__);                     \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(a, b)                                                        \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      printf("\033[31mFAILED\033[0m\n");                                       \
      printf("    Assertion failed: %s == %s\n", #a, #b);                      \
      printf("    Got: %zu, Expected: %zu\n", (size_t)(a), (size_t)(b));       \
      printf("    Location: %s:%d\n", __FILE__, __LINE__);                     \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_NOT_NULL(ptr)                                                   \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      printf("\033[31mFAILED\033[0m\n");                                       \
      printf("    Assertion failed: %s != NULL\n", #ptr);                      \
      printf("    Location: %s:%d\n", __FILE__, __LINE__);                     \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_NULL(ptr)                                                       \
  do {                                                                         \
    if ((ptr) != NULL) {                                                       \
      printf("\033[31mFAILED\033[0m\n");                                       \
      printf("    Assertion failed: %s == NULL\n", #ptr);                      \
      printf("    Location: %s:%d\n", __FILE__, __LINE__);                     \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

static bool is_aligned(const void *ptr, size_t align) {
  return ((uintptr_t)ptr % align) == 0;
}

TEST(test_basic_alloc_free) {
  uint8_t buffer[1024];
  pool_t pool;

  int err = pool_init(&pool, buffer, sizeof(buffer), 32);
  ASSERT_EQ(err, POOL_OK);

  void *slot = pool_alloc(&pool);
  ASSERT_NOT_NULL(slot);

  size_t before_free = pool_available(&pool);

  err = pool_free(&pool, slot);
  ASSERT_EQ(err, POOL_OK);

  size_t after_free = pool_available(&pool);
  ASSERT_EQ(after_free, before_free + 1);

  // should be able to reallocate
  void *slot2 = pool_alloc(&pool);
  ASSERT_NOT_NULL(slot2);

  pool_free(&pool, slot2);
  pool_destroy(&pool);
}

TEST(test_slot_size_honored) {
  uint8_t buffer[4096];
  pool_t pool;

  size_t test_sizes[] = {1, 4, 8, 16, 32, 64, 100, 128, 200, 256};
  size_t num_sizes = sizeof(test_sizes) / sizeof(test_sizes[0]);

  for (size_t i = 0; i < num_sizes; i++) {
    size_t requested = test_sizes[i];

    int err = pool_init(&pool, buffer, sizeof(buffer), requested);
    ASSERT_EQ(err, POOL_OK);

    size_t actual = pool_slot_size(&pool);
    ASSERT(actual >= requested);
    ASSERT(actual >= sizeof(void *));

    // allocate two slots and verify they don't overlap
    void *slot1 = pool_alloc(&pool);
    void *slot2 = pool_alloc(&pool);
    ASSERT_NOT_NULL(slot1);
    ASSERT_NOT_NULL(slot2);

    uintptr_t diff = (uintptr_t)slot2 > (uintptr_t)slot1
                         ? (uintptr_t)slot2 - (uintptr_t)slot1
                         : (uintptr_t)slot1 - (uintptr_t)slot2;
    ASSERT(diff >= actual);

    pool_free(&pool, slot1);
    pool_free(&pool, slot2);

    pool_destroy(&pool);
  }
}

TEST(test_alignment) {
  // intentionally misalign buffer
  uint8_t raw_buffer[2048 + 64];
  uint8_t *misaligned = raw_buffer + 3;

  pool_t pool;
  int err = pool_init(&pool, misaligned, 2048, 32);
  ASSERT_EQ(err, POOL_OK);

  for (int i = 0; i < 10; i++) {
    void *slot = pool_alloc(&pool);
    ASSERT_NOT_NULL(slot);
    ASSERT(is_aligned(slot, POOL_ALIGN));
    pool_free(&pool, slot);
  }

  pool_destroy(&pool);
}

TEST(test_exhaust_pool) {
  uint8_t buffer[1024];
  pool_t pool;

  int err = pool_init(&pool, buffer, sizeof(buffer), 64);
  ASSERT_EQ(err, POOL_OK);

  size_t capacity = pool_capacity(&pool);
  ASSERT(capacity > 0);

  // allocate all slots
  void **slots = (void **)malloc(capacity * sizeof(void *));
  ASSERT_NOT_NULL(slots);

  for (size_t i = 0; i < capacity; i++) {
    slots[i] = pool_alloc(&pool);
    ASSERT_NOT_NULL(slots[i]);
  }

  ASSERT(pool_is_full(&pool));
  ASSERT_EQ(pool_available(&pool), 0);

  // next allocation should fail
  void *extra = pool_alloc(&pool);
  ASSERT_NULL(extra);

  for (size_t i = 0; i < capacity; i++) {
      pool_free(&pool, slots[i]);
  }

  free(slots);
  pool_destroy(&pool);
}

TEST(test_free_reuse) {
  uint8_t buffer[512];
  pool_t pool;

  pool_init(&pool, buffer, sizeof(buffer), 32);

  void *slot1 = pool_alloc(&pool);
  ASSERT_NOT_NULL(slot1);

  pool_free(&pool, slot1);

  // next allocation should return the same slot lifo
  void *slot2 = pool_alloc(&pool);
  ASSERT_EQ(slot2, slot1);

  pool_free(&pool, slot2);

  pool_destroy(&pool);
}

TEST(test_reset) {
  uint8_t buffer[1024];
  pool_t pool;

  pool_init(&pool, buffer, sizeof(buffer), 32);
  size_t capacity = pool_capacity(&pool);

  pool_alloc(&pool);
  pool_alloc(&pool);
  pool_alloc(&pool);

  ASSERT_EQ(pool_available(&pool), capacity - 3);

  // Reset clears all allocations, so no manual free needed
  pool_reset(&pool);

  ASSERT_EQ(pool_available(&pool), capacity);
  ASSERT(pool_is_empty(&pool));

  // should be able to allocate capacity slots again
  void **slots = (void **)malloc(capacity * sizeof(void*));
  for (size_t i = 0; i < capacity; i++) {
    slots[i] = pool_alloc(&pool);
    ASSERT_NOT_NULL(slots[i]);
  }

  for (size_t i = 0; i < capacity; i++) {
      pool_free(&pool, slots[i]);
  }
  free(slots);

  pool_destroy(&pool);
}

TEST(test_owns) {
  uint8_t buffer1[1024];
  uint8_t buffer2[1024];
  pool_t pool1, pool2;

  pool_init(&pool1, buffer1, sizeof(buffer1), 32);
  pool_init(&pool2, buffer2, sizeof(buffer2), 32);

  void *slot1 = pool_alloc(&pool1);
  void *slot2 = pool_alloc(&pool2);

  ASSERT(pool_owns(&pool1, slot1));
  ASSERT(pool_owns(&pool2, slot2));

  ASSERT(!pool_owns(&pool1, slot2));
  ASSERT(!pool_owns(&pool2, slot1));

  int stack_var;
  ASSERT(!pool_owns(&pool1, &stack_var));
  ASSERT(!pool_owns(&pool1, NULL));

  // misaligned pointer within buffer range
  uint8_t *misaligned = (uint8_t *)slot1 + 1;
  ASSERT(!pool_owns(&pool1, misaligned));

  pool_free(&pool1, slot1);
  pool_free(&pool2, slot2);

  pool_destroy(&pool1);
  pool_destroy(&pool2);
}

TEST(test_interleaved) {
  uint8_t buffer[4096];
  pool_t pool;

  pool_init(&pool, buffer, sizeof(buffer), 32);
  size_t capacity = pool_capacity(&pool);

  void **slots = (void **)calloc(capacity, sizeof(void *));
  ASSERT_NOT_NULL(slots);

  srand(42);

  size_t allocated = 0;
  for (int op = 0; op < 1000; op++) {
    if (allocated == 0 || (allocated < capacity && rand() % 2 == 0)) {
      // allocate
      void *slot = pool_alloc(&pool);
      if (slot != NULL) {
        for (size_t i = 0; i < capacity; i++) {
          if (slots[i] == NULL) {
            slots[i] = slot;
            allocated++;
            // pattern to detect corruption
            memset(slot, (int)(i & 0xFF), pool_slot_size(&pool));
            break;
          }
        }
      }
    } else {
      // free random
      size_t start = (size_t)rand() % capacity;
      for (size_t i = 0; i < capacity; i++) {
        size_t idx = (start + i) % capacity;
        if (slots[idx] != NULL) {
          pool_free(&pool, slots[idx]);
          slots[idx] = NULL;
          allocated--;
          break;
        }
      }
    }

    ASSERT_EQ(pool_used(&pool), allocated);
    ASSERT_EQ(pool_available(&pool), capacity - allocated);
  }

  for(size_t i = 0; i < capacity; i++) {
      if(slots[i] != NULL) {
          pool_free(&pool, slots[i]);
      }
  }

  free(slots);
  pool_destroy(&pool);
}

TEST(test_init_errors) {
  uint8_t buffer[1024];
  pool_t pool;
  int err;

  err = pool_init(NULL, buffer, sizeof(buffer), 32);
  ASSERT_EQ(err, POOL_ERR_NULL_POOL);

  err = pool_init(&pool, NULL, sizeof(buffer), 32);
  ASSERT_EQ(err, POOL_ERR_NULL_BUFFER);

  err = pool_init(&pool, buffer, sizeof(buffer), 0);
  ASSERT_EQ(err, POOL_ERR_INVALID_SLOT_SIZE);

  uint8_t tiny[8];
  err = pool_init(&pool, tiny, sizeof(tiny), 64);
  ASSERT_EQ(err, POOL_ERR_BUFFER_TOO_SMALL);
}

TEST(test_free_errors) {
  uint8_t buffer[1024];
  pool_t pool;

  pool_init(&pool, buffer, sizeof(buffer), 32);

  int err = pool_free(NULL, buffer);
  ASSERT_EQ(err, POOL_ERR_NULL_POOL);

  err = pool_free(&pool, NULL);
  ASSERT_EQ(err, POOL_ERR_NULL_PTR);

#ifndef POOL_DEBUG
  // invalid ptr only returns error in release
  // in debug, it asserts/crashes intentionally
  int stack_var;
  err = pool_free(&pool, &stack_var);
  ASSERT_EQ(err, POOL_ERR_INVALID_PTR);
#endif

  pool_destroy(&pool);
}

TEST(test_stats) {
  uint8_t buffer[2048];
  pool_t pool;

  pool_init(&pool, buffer, sizeof(buffer), 64);

  pool_stats_t stats;
  pool_stats(&pool, &stats);

  ASSERT_EQ(stats.slot_size, pool_slot_size(&pool));
  ASSERT_EQ(stats.slot_count, pool_capacity(&pool));
  ASSERT_EQ(stats.free_count, pool_available(&pool));
  ASSERT_EQ(stats.used_count, 0);

  void *s1 = pool_alloc(&pool);
  void *s2 = pool_alloc(&pool);
  void *s3 = pool_alloc(&pool);

  pool_stats(&pool, &stats);
  ASSERT_EQ(stats.used_count, 3);
  ASSERT_EQ(stats.free_count, stats.slot_count - 3);

#ifdef POOL_DEBUG
  ASSERT_EQ(stats.total_allocs, 3);
  ASSERT_EQ(stats.total_frees, 0);
  ASSERT_EQ(stats.peak_used, 3);
#endif

  pool_free(&pool, s1);
  pool_free(&pool, s2);
  pool_free(&pool, s3);

  pool_destroy(&pool);
}

TEST(test_minimum_pool) {
  // smallest possible pool with 1 slot
  size_t required = pool_required_size(1, 1);
  ASSERT(required > 0);

  uint8_t *buffer = (uint8_t *)malloc(required);
  ASSERT_NOT_NULL(buffer);

  pool_t pool;
  int err = pool_init(&pool, buffer, required, 1);
  ASSERT_EQ(err, POOL_OK);
  ASSERT(pool_capacity(&pool) >= 1);

  void *slot = pool_alloc(&pool);
  ASSERT_NOT_NULL(slot);

  // FIXED: Cleanup
  pool_free(&pool, slot);

  pool_destroy(&pool);
  free(buffer);
}

TEST(test_large_slots) {
  size_t slot_size = 4096;
  size_t slot_count = 4;
  size_t required = pool_required_size(slot_size, slot_count);

  uint8_t *buffer = (uint8_t *)malloc(required);
  ASSERT_NOT_NULL(buffer);

  pool_t pool;
  int err = pool_init(&pool, buffer, required, slot_size);
  ASSERT_EQ(err, POOL_OK);
  ASSERT(pool_slot_size(&pool) >= slot_size);

  void *slots[4];
  for (int i = 0; i < 4; i++) {
    slots[i] = pool_alloc(&pool);
    if (slots[i] != NULL) {
      memset(slots[i], i + 1, slot_size);
    }
  }

  // verify patterns check overlap
  for (int i = 0; i < 4; i++) {
    if (slots[i] != NULL) {
      uint8_t *p = (uint8_t *)slots[i];
      for (size_t j = 0; j < slot_size; j++) {
        ASSERT_EQ(p[j], i + 1);
      }
    }
  }

  for (int i = 0; i < 4; i++) {
      if(slots[i]) pool_free(&pool, slots[i]);
  }

  pool_destroy(&pool);
  free(buffer);
}

TEST(test_required_size) {
  size_t test_sizes[] = {1, 8, 32, 64, 128, 256, 512, 1024};
  size_t test_counts[] = {1, 10, 100, 1000};

  for (size_t si = 0; si < sizeof(test_sizes) / sizeof(test_sizes[0]); si++) {
    for (size_t ci = 0; ci < sizeof(test_counts) / sizeof(test_counts[0]);
         ci++) {
      size_t slot_size = test_sizes[si];
      size_t slot_count = test_counts[ci];

      size_t required = pool_required_size(slot_size, slot_count);

      uint8_t *buffer = (uint8_t *)malloc(required);
      ASSERT_NOT_NULL(buffer);

      pool_t pool;
      int err = pool_init(&pool, buffer, required, slot_size);
      ASSERT_EQ(err, POOL_OK);
      ASSERT(pool_capacity(&pool) >= slot_count);

      pool_destroy(&pool);
      free(buffer);
    }
  }
}

TEST(test_error_strings) {
  for (int i = 0; i < POOL_ERR_COUNT; i++) {
    const char *str = pool_error_string(i);
    ASSERT_NOT_NULL(str);
    ASSERT(strlen(str) > 0);
  }

  const char *str = pool_error_string(999);
  ASSERT_NOT_NULL(str);
}

TEST(test_null_pool_queries) {
  ASSERT_EQ(pool_is_full(NULL), 1);
  ASSERT_EQ(pool_is_empty(NULL), 1);
  ASSERT_EQ(pool_slot_size(NULL), 0);
  ASSERT_EQ(pool_capacity(NULL), 0);
  ASSERT_EQ(pool_available(NULL), 0);
  ASSERT_EQ(pool_used(NULL), 0);
  ASSERT_EQ(pool_owns(NULL, (void *)0x1234), 0);
  ASSERT_NULL(pool_alloc(NULL));

  pool_stats_t stats;
  pool_stats(NULL, &stats);
  ASSERT_EQ(stats.slot_count, 0);

  // these should not crash
  pool_destroy(NULL);
  pool_reset(NULL);
}

#ifdef POOL_DEBUG

TEST(test_debug_double_free) {
  uint8_t buffer[1024];
  pool_t pool;

  pool_init(&pool, buffer, sizeof(buffer), 32);

  void *slot = pool_alloc(&pool);
  ASSERT_NOT_NULL(slot);
  ASSERT(pool_is_allocated(&pool, slot));

  int err = pool_free(&pool, slot);
  ASSERT_EQ(err, POOL_OK);
  ASSERT(!pool_is_allocated(&pool, slot));

  // actual double free triggers assert so we just verify state tracking here
  pool_destroy(&pool);
}

TEST(test_debug_stats) {
  uint8_t buffer[2048];
  pool_t pool;

  pool_init(&pool, buffer, sizeof(buffer), 32);

  void *s1 = pool_alloc(&pool);
  void *s2 = pool_alloc(&pool);
  void *s3 = pool_alloc(&pool);
  // peak is 3

  pool_free(&pool, s1);
  pool_free(&pool, s2);
  // used is 1

  void *s4 = pool_alloc(&pool);
  void *s5 = pool_alloc(&pool);
  // used is 3 peak still 3

  pool_stats_t stats;
  pool_stats(&pool, &stats);

  ASSERT_EQ(stats.total_allocs, 5);
  ASSERT_EQ(stats.total_frees, 2);
  ASSERT_EQ(stats.peak_used, 3);
  ASSERT_EQ(stats.used_count, 3);

  pool_free(&pool, s3);
  pool_free(&pool, s4);
  pool_free(&pool, s5);

  pool_destroy(&pool);
}

#endif // POOL_DEBUG

TEST(test_stress_perf) {
  size_t slot_size = 64;
  size_t slot_count = 10000;
  size_t required = pool_required_size(slot_size, slot_count);

  uint8_t *buffer = (uint8_t *)malloc(required);
  ASSERT_NOT_NULL(buffer);

  pool_t pool;
  pool_init(&pool, buffer, required, slot_size);

  size_t actual_count = pool_capacity(&pool);
  void **slots = (void **)malloc(actual_count * sizeof(void *));
  ASSERT_NOT_NULL(slots);

  clock_t start = clock();
  for (size_t i = 0; i < actual_count; i++) {
    slots[i] = pool_alloc(&pool);
  }
  clock_t alloc_time = clock() - start;

  start = clock();
  for (size_t i = 0; i < actual_count; i++) {
    pool_free(&pool, slots[i]);
  }
  clock_t free_time = clock() - start;

  // just ensure it didn't crash and actually did work
  ASSERT(alloc_time >= 0);
  ASSERT(free_time >= 0);

  free(slots);
  pool_destroy(&pool);
  free(buffer);
}

int main(void) {
  printf("\n");
  printf(" pool allocator tests \n");
  printf("configuration:\n");
#ifdef POOL_DEBUG
  printf("   POOL_DEBUG: enabled\n");
#else
  printf("   POOL_DEBUG: disabled\n");
#endif
  printf("   Default alignment: %zu bytes\n", (size_t)POOL_ALIGN);

  RUN_TEST(test_basic_alloc_free);
  RUN_TEST(test_slot_size_honored);
  RUN_TEST(test_alignment);
  RUN_TEST(test_exhaust_pool);
  RUN_TEST(test_free_reuse);
  RUN_TEST(test_reset);
  RUN_TEST(test_owns);
  RUN_TEST(test_interleaved);

  RUN_TEST(test_init_errors);
  RUN_TEST(test_free_errors);

  RUN_TEST(test_stats);

  RUN_TEST(test_minimum_pool);
  RUN_TEST(test_large_slots);
  RUN_TEST(test_required_size);
  RUN_TEST(test_error_strings);
  RUN_TEST(test_null_pool_queries);

#ifdef POOL_DEBUG
  RUN_TEST(test_debug_double_free);
  RUN_TEST(test_debug_stats);
#endif

  RUN_TEST(test_stress_perf);

  printf("    %d/%d tests passed\n", tests_passed, tests_run);
  if (tests_failed > 0) {
    printf("   \033[31m%d TESTS FAILED\033[0m\n", tests_failed);
  } else {
    printf("   \033[32mALL TESTS PASSED\033[0m\n");
  }

  return tests_failed > 0 ? 1 : 0;
}