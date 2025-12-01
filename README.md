# alloc

**alloc** is a collection of lightweight, single header memory allocators for C.

These libraries follow the [stb-style](https://github.com/nothings/stb) single header library design, they are intended for scenarios where you need precise control over memory layout, performance, and fragmentation, such as game development, embedded systems, or high performance systems programming.

## Features

*   **Zero Dependencies:** Only requires the standard C library (and even that is optional via configuration).
*   **Predictable:** No hidden allocations. You usually provide the backing buffer.
*   **Debuggable:** Optional debug macros to track usage, detect leaks, poison freed memory, and validate integrity.
*   **Customizable:** Overridable macros for `assert`, `memset`, and memory backing.

## Integration

Do this in **one** C/C++ file to create the implementation:

```c
#define ARENA_IMPLEMENTATION
#include "arena.h"

#define POOL_IMPLEMENTATION
#include "pool.h"

// etc...
```

In other files, simply include the header:

```c
#include "arena.h"
#include "pool.h"
```

## The Allocators

### 1. Arena (`arena.h`)
A **Linear/Bump Allocator**., Memory is allocated sequentially. You cannot free individual items, you free everything at once by resetting the arena.
*   **Best for:** Per frame data, temporary buffers, string processing, loading configurations.
*   **Complexity:** Allocation O(1), Free O(1).
*   **Capabilities:** Supports fixed buffers (stack) OR infinite growth via block chaining

```c
#include "arena.h"

void example(void) {
    uint8_t buffer[1024];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // allocation
    int *data = arena_new_array(&arena, int, 10);
    char *str = arena_alloc(&arena, 64);

    // free everything 
    arena_reset(&arena); 
    
    arena_destroy(&arena);
}
```

### 2. Pool (`pool.h`)
A **Fixed-Size Slot Allocator**. Breaks memory into equal-sized chunks. Uses a free-list to track available slots.
*   **Best for:** Game entities, particles, network packets, any scenario with many objects of the same type being created and destroyed randomly.
*   **Complexity:** Allocation O(1), Free O(1).

```c
#include "pool.h"

typedef struct { float x, y; } Particle;

void example(void) {
    uint8_t buffer[4096];
    pool_t pool;
    // Initialize pool for Particle-sized slots
    pool_init(&pool, buffer, sizeof(buffer), sizeof(Particle));

    Particle *p1 = pool_alloc(&pool);
    Particle *p2 = pool_alloc(&pool);

    // Can be freed in any order
    pool_free(&pool, p1);
    pool_free(&pool, p2);

    pool_destroy(&pool);
}
```

### 3. Stack (`stack.h`)
A **LIFO (Last In, First Out) Allocator**. Similar to a linear allocator, but allows freeing memory starting from the top of the stack.
*   **Best for:** Recursion, hierarchical processing, nested scopes.
*   **Complexity:** Allocation O(1), Free O(1).
*   **Constraint:** You must free pointers in the reverse order they were allocated.

```c
#include "stack.h"

void example(void) {
    uint8_t buffer[1024];
    stack_t stack;
    stack_init(&stack, buffer, sizeof(buffer));

    void *a = stack_alloc(&stack, 100);
    
    // Create a save point
    stack_marker_t marker = stack_save(&stack);
    
    void *temp1 = stack_alloc(&stack, 50);
    void *temp2 = stack_alloc(&stack, 50);

    // Instantly free temp1 and temp2, 'a' remains valid
    stack_restore(&stack, marker);

    stack_free(&stack, a);
    stack_destroy(&stack);
}
```

### 4. Slab (`slab.h`)
A **Multi Size Class Allocator**. Contains multiple pools of different sizes, Automatically routes an allocation to the smallest pool that fits the requested size.
*   **Best for:** General purpose allocation within a fixed memory budget, reducing fragmentation for mixed-size workloads.
*   **Complexity:** Allocation O(1) (mostly), Free O(1).

```c
#include "slab.h"

void example(void) {
    uint8_t buffer[8192];
    // Define bucket sizes
    size_t sizes[] = { 32, 64, 128, 256 }; 
    
    slab_t slab;
    slab_init(&slab, buffer, sizeof(buffer), sizes, 4);

    void *small = slab_alloc(&slab, 20);  // Goes into 32-byte slot
    void *med   = slab_alloc(&slab, 100); // Goes into 128-byte slot

    slab_free(&slab, small);
    slab_free(&slab, med);
    
    slab_destroy(&slab);
}
```

## Configuration

You can customize the behavior of the allocators by defining macros before including the headers.

*   `*_STATIC`: Define this (e.g., `ARENA_STATIC`) to make all functions `static`, allowing you to use the library privately in a single source file without linkage issues.
*   `*_DEBUG`: Define this (e.g., `POOL_DEBUG`) to enable bounds checking, leak detection, memory poisoning, and usage statistics.
*   `*_ASSERT`: Override the default `assert.h`.

Example:

```c
#define POOL_IMPLEMENTATION
#define POOL_DEBUG          // Enable leak detection
#define POOL_ASSERT(x)      my_custom_assert(x)
#include "pool.h"
```

## Roadmap

*   [ ] **General Purpose Allocator:** A heap style allocator (Free list or Buddy system) for general cases where arenas/pools don't fit.
*   [ ] **Thread Safety:** Optional wrapper macros or atomic primitives for thread-safe access (currently, these are single threaded by design).
*   [ ] **C++ RAII Wrappers:** Optional C++ headers to provide `std::allocator` compatibility or RAII scoping.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
