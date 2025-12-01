
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POOL_IMPLEMENTATION
#include "../pool.h"


// example

typedef struct {
    int x, y;
    char name[24];
} Entity;

void example_basic(void) {

    // allocate memory for the pool
    uint8_t buffer[4096];
    pool_t pool;

    // initialize the pool
    int err = pool_init(&pool, buffer, sizeof(buffer), sizeof(Entity));
    if (err != POOL_OK) {
        printf("init failed %s\n", pool_error_string(err));
        return;
    }

    printf("pool created %zu slots of %zu bytes each\n",
           pool_capacity(&pool), pool_slot_size(&pool));

    // allocate objects
    Entity *player = pool_alloc(&pool);
    Entity *enemy = pool_alloc(&pool);

    if (player && enemy) {
        player->x = 100;
        player->y = 200;
        strcpy(player->name, "hero");

        enemy->x = 50;
        enemy->y = 75;
        strcpy(enemy->name, "goblin");

        printf("player: %s at %d, %d\n", player->name, player->x, player->y);
        printf("enemy: %s at %d, %d\n", enemy->name, enemy->x, enemy->y);
    }

    // free objects when done
    pool_free(&pool, player);
    pool_free(&pool, enemy);

    // once fully done , clean up
    pool_destroy(&pool);

    printf("\n");
}

// another example , now using pool_required_size() for exact allocation

void example_exact_size(void) {

    // we calculate exactly how much memory we need for 100 entities
    size_t needed = pool_required_size(sizeof(Entity), 100);
    printf("need %zu bytes for 100 entities\n", needed);

    // allocate exact amount
    uint8_t *buffer = malloc(needed);
    if (!buffer) {
        printf("malloc failed\n");
        return;
    }

    pool_t pool;
    pool_init(&pool, buffer, needed, sizeof(Entity));

    printf("got exactly %zu slots\n", pool_capacity(&pool));

    pool_destroy(&pool);
    free(buffer);

    printf("\n");
}

// another example , using pool state

void example_pool_state(void) {

    uint8_t buffer[1024];
    pool_t pool;
    pool_init(&pool, buffer, sizeof(buffer), 64);

    printf("initial: %zu available, %zu used\n",
           pool_available(&pool), pool_used(&pool));

    // allocate 3 slots
    void *a = pool_alloc(&pool);
    void *b = pool_alloc(&pool);
    void *c = pool_alloc(&pool);

    printf("after 3 allocs: %zu available, %zu used\n",
           pool_available(&pool), pool_used(&pool));

    printf("pool is full: %s\n", pool_is_full(&pool) ? "yes" : "no");
    printf("pool is empty: %s\n", pool_is_empty(&pool) ? "yes" : "no");

    // free one
    pool_free(&pool, b);
    printf("after 1 free: %zu available, %zu used\n",
           pool_available(&pool), pool_used(&pool));

    pool_free(&pool, a);
    pool_free(&pool, c);
    pool_destroy(&pool);

    printf("\n");
}

// another example, now using pool_owns() for validation

void example_ownership(void) {
    uint8_t buffer1[512];
    uint8_t buffer2[512];
    pool_t pool1, pool2;

    pool_init(&pool1, buffer1, sizeof(buffer1), 32);
    pool_init(&pool2, buffer2, sizeof(buffer2), 32);

    void *from_pool1 = pool_alloc(&pool1);
    void *from_pool2 = pool_alloc(&pool2);
    int stack_var = 42;

    printf("pool1 owns from_pool1: %s\n", pool_owns(&pool1, from_pool1) ? "yes" : "no");
    printf("pool1 owns from_pool2: %s\n", pool_owns(&pool1, from_pool2) ? "yes" : "no");
    printf("pool1 owns stack_var: %s\n", pool_owns(&pool1, &stack_var) ? "yes" : "no");

    pool_free(&pool1, from_pool1);
    pool_free(&pool2, from_pool2);
    pool_destroy(&pool1);
    pool_destroy(&pool2);

    printf("\n");
}

// another example , now using pool_reset() for frame allocators

void example_reset(void) {

    uint8_t buffer[2048];
    pool_t pool;
    pool_init(&pool, buffer, sizeof(buffer), 64);

    for (int frame = 1; frame <= 3; frame++) {
        printf("frame %d: ", frame);

        int count = 0;
        while (pool_alloc(&pool) != NULL) {
            count++;
        }
        printf("allocated %d objects, ", count);

        pool_reset(&pool);
        printf("reset done, %zu available\n", pool_available(&pool));
    }

    pool_destroy(&pool);

    printf("\n");
}

// another example on how to get the stats

void example_stats(void) {

    uint8_t buffer[2048];
    pool_t pool;
    pool_init(&pool, buffer, sizeof(buffer), 32);

    void *ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = pool_alloc(&pool);
    }
    for (int i = 0; i < 5; i++) {
        pool_free(&pool, ptrs[i]);
    }

    // get the stats
    pool_stats_t stats;
    pool_stats(&pool, &stats);

    printf("slot size: %zu bytes\n", stats.slot_size);
    printf("total slots: %zu\n", stats.slot_count);
    printf("used: %zu\n", stats.used_count);
    printf("free: %zu\n", stats.free_count);

#ifdef POOL_DEBUG
    printf("total allocs: %zu\n", stats.total_allocs);
    printf("total frees: %zu\n", stats.total_frees);
    printf("peak usage: %zu\n", stats.peak_used);
#endif

    // clean up remaining
    for (int i = 5; i < 10; i++) {
        pool_free(&pool, ptrs[i]);
    }
    pool_destroy(&pool);

    printf("\n");
}


int main(void) {

    example_basic();
    example_exact_size();
    example_pool_state();
    example_ownership();
    example_reset();
    example_stats();

    return 0;
}