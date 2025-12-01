#define STACK_IMPLEMENTATION
#include "../stack.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int x, y;
    char name[32];
} Entity;

int main(void) {
    // create a buffer and initialize the stack
    uint8_t buffer[4096];
    stack_t stack;

    stack_init_buffer(&stack, buffer);
    printf("stack initialized with %zu bytes\n\n", stack_remaining(&stack));

    // do basic allocation
    int *numbers = stack_alloc(&stack, 5 * sizeof(int));
    for (int i = 0; i < 5; i++) {
        numbers[i] = i * 10;
    }
    printf("allocated 5 ints: %d %d %d %d %d\n",
           numbers[0], numbers[1], numbers[2], numbers[3], numbers[4]);

    // allocate a struct
    Entity *player = stack_alloc(&stack, sizeof(Entity));
    player->x = 100;
    player->y = 200;
    strcpy(player->name, "Hero");
    printf("allocated entity %s at (%d, %d)\n", player->name, player->x, player->y);

    // use stack_calloc for zeroed memory
    int *scores = stack_calloc(&stack, 3, sizeof(int));
    printf("calloc'd scores (should be 0): %d %d %d\n", scores[0], scores[1], scores[2]);

    // Save a marker before temporary work
    stack_marker_t checkpoint = stack_save(&stack);

    // do some temporary allocations
    char *temp1 = stack_alloc(&stack, 64);
    char *temp2 = stack_alloc(&stack, 128);
    strcpy(temp1, "temporary string 1");
    strcpy(temp2, "temporary string 2");
    printf("temp allocations: '%s', '%s'\n", temp1, temp2);
    printf("remaining after temps: %zu bytes\n", stack_remaining(&stack));

    // rstore to marker , frees all temps at once
    stack_restore(&stack, checkpoint);
    printf("remaining after restore %zu bytes\n", stack_remaining(&stack));

    // previous allocations still valid
    printf("player still valid, %s at (%d, %d)\n", player->name, player->x, player->y);

    // lifo free  must free in reverse order
    stack_free(&stack, scores);   // Free most recent first
    stack_free(&stack, player);
    stack_free(&stack, numbers);  // Free oldest last
    printf("all freed, remaining, %zu bytes\n", stack_remaining(&stack));

    // aligned allocation , this should helpfuly for simd or simd like data
    float *simd_data = stack_alloc_aligned(&stack, 16 * sizeof(float), 16);
    printf("\naligned pointer, %p 16 byte aligned %s\n",
           (void*)simd_data,
           ((uintptr_t)simd_data % 16 == 0) ? "yes" : "no");

    // reset clears everything
    stack_reset(&stack);
    printf("\nafter rese,: %zu bytes available\n", stack_remaining(&stack));

    // Cleanup
    stack_destroy(&stack);

    return 0;
}
