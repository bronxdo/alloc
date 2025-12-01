/*
 * tests for stack.h
 *
 *   # super basic tests
 *   gcc -Wall -Wextra -O2 -o tests_stack tests_stack.c && ./tests_stack
 *
 *   # With debug features
 *   gcc -Wall -Wextra -DSTACK_DEBUG -O2 -o tests_stack_debug tests_stack.c && ./tests_stack_debug
 */

#define STACK_IMPLEMENTATION
#include "../stack.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define TEST_BUFFER_SIZE 4096

TEST(test_init_destroy) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));
  stack_destroy(&stack);

  ASSERT_EQ(stack_init(&stack, NULL, 100), -1);
  ASSERT_EQ(stack_init(&stack, buffer, 0), -1);
}

TEST(test_basic_alloc_free) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  void *ptr1, *ptr2;
  size_t remaining_after_alloc;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  ptr1 = stack_alloc(&stack, 100);
  ASSERT_NOT_NULL(ptr1);
  ASSERT(stack_owns(&stack, ptr1));

  remaining_after_alloc = stack_remaining(&stack);
  ASSERT(remaining_after_alloc < sizeof(buffer));

  // free should restore space
  stack_free(&stack, ptr1);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  // allocate again should reuse space
  ptr2 = stack_alloc(&stack, 100);
  ASSERT_NOT_NULL(ptr2);
  ASSERT_EQ(ptr1, ptr2);

  stack_free(&stack, ptr2);
  stack_destroy(&stack);
}

TEST(test_lifo_order) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  void *a, *b, *c;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  a = stack_alloc(&stack, 64);
  b = stack_alloc(&stack, 128);
  c = stack_alloc(&stack, 256);

  ASSERT_NOT_NULL(a);
  ASSERT_NOT_NULL(b);
  ASSERT_NOT_NULL(c);

  // free in lifo order
  stack_free(&stack, c);
  stack_free(&stack, b);
  stack_free(&stack, a);

  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_save_restore) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  stack_marker_t mark;
  void *a, *b;
  size_t offset_at_mark;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  a = stack_alloc(&stack, 100);
  ASSERT_NOT_NULL(a);

  mark = stack_save(&stack);
  offset_at_mark = sizeof(buffer) - stack_remaining(&stack);

  b = stack_alloc(&stack, 200);
  ASSERT_NOT_NULL(b);
  ASSERT(stack_remaining(&stack) < sizeof(buffer) - offset_at_mark);

  // restore should free b
  stack_restore(&stack, mark);
  ASSERT_EQ(sizeof(buffer) - stack_remaining(&stack), offset_at_mark);

  stack_free(&stack, a);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_nested_markers) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  stack_marker_t mark1, mark2, mark3;
  void *a, *b, *c;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  mark1 = stack_save(&stack);
  a = stack_alloc(&stack, 50);
  ASSERT_NOT_NULL(a);

  mark2 = stack_save(&stack);
  b = stack_alloc(&stack, 75);
  ASSERT_NOT_NULL(b);

  mark3 = stack_save(&stack);
  c = stack_alloc(&stack, 100);
  ASSERT_NOT_NULL(c);

  // restore innermost first
  stack_restore(&stack, mark3);
  stack_restore(&stack, mark2);
  stack_restore(&stack, mark1);

  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_alignment) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  void *ptr;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  ptr = stack_alloc_aligned(&stack, 32, 16);
  ASSERT_NOT_NULL(ptr);
  ASSERT_EQ((uintptr_t)ptr % 16, 0);
  stack_free(&stack, ptr);

  ptr = stack_alloc_aligned(&stack, 64, 32);
  ASSERT_NOT_NULL(ptr);
  ASSERT_EQ((uintptr_t)ptr % 32, 0);
  stack_free(&stack, ptr);

  ptr = stack_alloc_aligned(&stack, 128, 64);
  ASSERT_NOT_NULL(ptr);
  ASSERT_EQ((uintptr_t)ptr % 64, 0);
  stack_free(&stack, ptr);

  // large alignment
  ptr = stack_alloc_aligned(&stack, 16, 256);
  ASSERT_NOT_NULL(ptr);
  ASSERT_EQ((uintptr_t)ptr % 256, 0);
  stack_free(&stack, ptr);

  // alignment smaller than minimum should use minimum
  ptr = stack_alloc_aligned(&stack, 100, 1);
  ASSERT_NOT_NULL(ptr);
  ASSERT_EQ((uintptr_t)ptr % STACK_MIN_ALIGNMENT, 0);
  stack_free(&stack, ptr);

  stack_destroy(&stack);
}

TEST(test_reset) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  stack_alloc(&stack, 100);
  stack_alloc(&stack, 200);
  stack_alloc(&stack, 300);

  ASSERT(stack_remaining(&stack) < sizeof(buffer));

  stack_reset(&stack);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_overflow) {
  uint8_t buffer[256];
  stack_t stack;
  void *ptr;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  ptr = stack_alloc(&stack, 100);
  ASSERT_NOT_NULL(ptr);

  ptr = stack_alloc(&stack, 200);
  ASSERT_NULL(ptr);

  stack_reset(&stack);

  // exact capacity minus header overhead should work
  ptr = stack_alloc(&stack, sizeof(buffer) - STACK_HEADER_SIZE);
  ASSERT_NOT_NULL(ptr);

  ptr = stack_alloc(&stack, 1);
  ASSERT_NULL(ptr);

  stack_destroy(&stack);
}

TEST(test_zero_size_alloc) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  void *ptr;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  ptr = stack_alloc(&stack, 0);
  ASSERT_NULL(ptr);

  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_free_null) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  stack_free(&stack, NULL);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_stats) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  stack_stats_t stats;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  stats = stack_stats(&stack);
  ASSERT_EQ(stats.capacity, sizeof(buffer));
  ASSERT_EQ(stats.used, 0);
  ASSERT_EQ(stats.remaining, sizeof(buffer));
#ifdef STACK_DEBUG
  ASSERT_EQ(stats.allocation_count, 0);
  ASSERT_EQ(stats.peak_usage, 0);
#endif

  stack_alloc(&stack, 100);
  stack_alloc(&stack, 200);

  stats = stack_stats(&stack);
  ASSERT_EQ(stats.capacity, sizeof(buffer));
  ASSERT(stats.used > 0);
  ASSERT_EQ(stats.remaining, stats.capacity - stats.used);
#ifdef STACK_DEBUG
  ASSERT_EQ(stats.allocation_count, 2);
  ASSERT(stats.peak_usage >= stats.used);
#endif

  stack_reset(&stack);

  stats = stack_stats(&stack);
  ASSERT_EQ(stats.used, 0);
#ifdef STACK_DEBUG
  ASSERT_EQ(stats.allocation_count, 0);
  ASSERT(stats.peak_usage > 0);
#endif

  stack_destroy(&stack);
}

TEST(test_mixed_free_restore) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  stack_marker_t mark;
  void *a, *b, *c;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  a = stack_alloc(&stack, 50);
  mark = stack_save(&stack);
  b = stack_alloc(&stack, 75);
  c = stack_alloc(&stack, 100);

  ASSERT_NOT_NULL(a);
  ASSERT_NOT_NULL(b);
  ASSERT_NOT_NULL(c);

  // free c manually
  stack_free(&stack, c);

  // then restore to mark which should free b
  stack_restore(&stack, mark);

  // a should still be active
  stack_stats_t stats = stack_stats(&stack);
  (void)stats;
#ifdef STACK_DEBUG
  ASSERT_EQ(stats.allocation_count, 1);
#endif

  stack_free(&stack, a);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_owns) {
  uint8_t buffer1[TEST_BUFFER_SIZE];
  uint8_t buffer2[TEST_BUFFER_SIZE];
  stack_t stack1;
  void *ptr;

  ASSERT_EQ(stack_init(&stack1, buffer1, sizeof(buffer1)), 0);

  ptr = stack_alloc(&stack1, 100);
  ASSERT_NOT_NULL(ptr);

  ASSERT(stack_owns(&stack1, ptr));

  ASSERT(!stack_owns(&stack1, buffer2));
  ASSERT(!stack_owns(&stack1, buffer2 + 50));
  ASSERT(!stack_owns(&stack1, NULL));

  stack_destroy(&stack1);
}

TEST(test_multiple_stacks) {
  uint8_t buffer1[TEST_BUFFER_SIZE];
  uint8_t buffer2[TEST_BUFFER_SIZE];
  stack_t stack1, stack2;
  void *p1, *p2;

  ASSERT_EQ(stack_init(&stack1, buffer1, sizeof(buffer1)), 0);
  ASSERT_EQ(stack_init(&stack2, buffer2, sizeof(buffer2)), 0);

  p1 = stack_alloc(&stack1, 100);
  p2 = stack_alloc(&stack2, 200);

  ASSERT_NOT_NULL(p1);
  ASSERT_NOT_NULL(p2);

  ASSERT(stack_owns(&stack1, p1));
  ASSERT(!stack_owns(&stack1, p2));
  ASSERT(!stack_owns(&stack2, p1));
  ASSERT(stack_owns(&stack2, p2));

  stack_free(&stack1, p1);
  stack_free(&stack2, p2);

  stack_destroy(&stack1);
  stack_destroy(&stack2);
}

TEST(test_write_to_allocation) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  char *str;
  int *arr;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  str = (char *)stack_alloc(&stack, 32);
  ASSERT_NOT_NULL(str);
  strcpy(str, "Hello, Stack!");
  ASSERT_EQ(strcmp(str, "Hello, Stack!"), 0);

  arr = (int *)stack_alloc_aligned(&stack, 10 * sizeof(int), sizeof(int));
  ASSERT_NOT_NULL(arr);
  for (int i = 0; i < 10; i++) {
    arr[i] = i * i;
  }
  for (int i = 0; i < 10; i++) {
    ASSERT_EQ(arr[i], i * i);
  }

  ASSERT_EQ(strcmp(str, "Hello, Stack!"), 0);

  stack_free(&stack, arr);
  stack_free(&stack, str);
  stack_destroy(&stack);
}

TEST(test_many_allocations) {
  uint8_t buffer[16384];
  stack_t stack;
  void *ptrs[100];
  int i;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  for (i = 0; i < 100; i++) {
    ptrs[i] = stack_alloc(&stack, 32);
    ASSERT_NOT_NULL(ptrs[i]);
  }

  stack_stats_t stats = stack_stats(&stack);
  (void)stats;
#ifdef STACK_DEBUG
  ASSERT_EQ(stats.allocation_count, 100);
#endif

  for (i = 99; i >= 0; i--) {
    stack_free(&stack, ptrs[i]);
  }

  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));
  stack_destroy(&stack);
}

TEST(test_interleaved_markers_and_frees) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  stack_marker_t m1, m2;
  void *a, *b, *c, *d;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  a = stack_alloc(&stack, 32);
  m1 = stack_save(&stack);
  (void)m1;

  b = stack_alloc(&stack, 64);
  c = stack_alloc(&stack, 96);
  m2 = stack_save(&stack);

  d = stack_alloc(&stack, 128);

  stack_free(&stack, d);

  // restore to m2 noop since we already freed d
  stack_restore(&stack, m2);

  stack_free(&stack, c);
  stack_free(&stack, b);

  stack_stats_t stats = stack_stats(&stack);
  (void)stats;
#ifdef STACK_DEBUG
  ASSERT_EQ(stats.allocation_count, 1);
#endif

  stack_free(&stack, a);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  stack_destroy(&stack);
}

TEST(test_small_buffer) {
  uint8_t buffer[64];
  stack_t stack;
  void *ptr;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  ptr = stack_alloc(&stack, 16);
  ASSERT_NOT_NULL(ptr);
  stack_free(&stack, ptr);

  ptr = stack_alloc(&stack, 100);
  ASSERT_NULL(ptr);

  stack_destroy(&stack);
}

TEST(test_calloc) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  int *arr;
  int i;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);

  arr = (int *)stack_calloc(&stack, 10, sizeof(int));
  ASSERT_NOT_NULL(arr);

  for (i = 0; i < 10; i++) {
    ASSERT_EQ(arr[i], 0);
  }

  stack_free(&stack, arr);

  arr = (int *)stack_calloc(&stack, 0, sizeof(int));
  ASSERT_NULL(arr);

  arr = (int *)stack_calloc(&stack, 10, 0);
  ASSERT_NULL(arr);

  // overflow check
  arr = (int *)stack_calloc(&stack, (size_t)-1, (size_t)-1);
  ASSERT_NULL(arr);

  stack_destroy(&stack);
}

TEST(test_init_buffer_macro) {
  uint8_t buffer[1024];
  stack_t stack;
  void *ptr;

  ASSERT_EQ(stack_init_buffer(&stack, buffer), 0);
  ASSERT_EQ(stack_remaining(&stack), sizeof(buffer));

  ptr = stack_alloc(&stack, 100);
  ASSERT_NOT_NULL(ptr);

  stack_destroy(&stack);
}

TEST(test_const_correctness) {
  uint8_t buffer[TEST_BUFFER_SIZE];
  stack_t stack;
  const stack_t *const_stack;
  void *ptr;

  ASSERT_EQ(stack_init(&stack, buffer, sizeof(buffer)), 0);
  ptr = stack_alloc(&stack, 64);
  ASSERT_NOT_NULL(ptr);

  const_stack = &stack;
  (void)stack_remaining(const_stack);
  (void)stack_stats(const_stack);
  (void)stack_owns(const_stack, ptr);

  stack_destroy(&stack);
}

int main(void) {
  printf("\n");
  printf(" stack allocator tests \n");
  printf("configuration:\n");
#ifdef STACK_DEBUG
  printf("   STACK_DEBUG: enabled\n");
#else
  printf("   STACK_DEBUG: disabled\n");
#endif

  RUN_TEST(test_init_destroy);
  RUN_TEST(test_basic_alloc_free);
  RUN_TEST(test_lifo_order);
  RUN_TEST(test_save_restore);
  RUN_TEST(test_nested_markers);
  RUN_TEST(test_alignment);
  RUN_TEST(test_reset);
  RUN_TEST(test_overflow);
  RUN_TEST(test_zero_size_alloc);
  RUN_TEST(test_free_null);
  RUN_TEST(test_stats);
  RUN_TEST(test_mixed_free_restore);
  RUN_TEST(test_owns);
  RUN_TEST(test_multiple_stacks);
  RUN_TEST(test_write_to_allocation);
  RUN_TEST(test_many_allocations);
  RUN_TEST(test_interleaved_markers_and_frees);
  RUN_TEST(test_small_buffer);
  RUN_TEST(test_calloc);
  RUN_TEST(test_init_buffer_macro);
  RUN_TEST(test_const_correctness);

  printf("    %d/%d tests passed\n", tests_passed, tests_run);
  if (tests_failed > 0) {
    printf("   \033[31m%d TESTS FAILED\033[0m\n", tests_failed);
  } else {
    printf("   \033[32mALL TESTS PASSED\033[0m\n");
  }

  return tests_failed > 0 ? 1 : 0;
}