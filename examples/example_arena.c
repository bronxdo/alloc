/*
 * examples for arena.h
 *
 * compile:
 *   gcc -std=c11 -O2 -o example_arena example_arena.c
 *
 * with debug features:
 *   gcc -std=c11 -O2 -DARENA_DEBUG -o example_arena example_arena.c
 */

#define ARENA_IMPLEMENTATION
#include "../arena.h"

#include <stdio.h>
#include <string.h>


void example_basic(void) {
    // create a buffer on the stack or use malloc for heap
    unsigned char buffer[4096];
    arena_t arena;

    // initialize
    arena_init(&arena, buffer, sizeof(buffer));

    // allocate memory this is like malloc, but from the arena
    int *numbers = arena_alloc(&arena, 10 * sizeof(int));
    char *name = arena_alloc(&arena, 64);

    strcpy(name, "arena test");

    // use the memory the way you like
    for (int i = 0; i < 10; i++) {
        numbers[i] = i * i;
    }

    printf("  numbers[5] = %d\n", numbers[5]);
    printf("  name = \"%s\"\n", name);
    printf("  used %zu / %zu bytes\n", arena_used(&arena), arena_capacity(&arena));

    // reset to reuse all memory no need for individual free
    arena_reset(&arena);
    printf("  after reset: %zu bytes used\n", arena_used(&arena));

    // once the lifetime of objects that uses arena is done, clean up
    arena_destroy(&arena);
    printf("\n");
}

// another example

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    char name[32];
    int health;
    Vec3 position;
} Entity;

void example_typed(void) {

    unsigned char buffer[4096];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // allocate single struct we try to properly align
    Entity *player = arena_new(&arena, Entity);
    strcpy(player->name, "player1");
    player->health = 100;
    player->position = (Vec3){0.0f, 1.0f, 0.0f};

    // allocate array of structs (zeroed)
    Entity *enemies = arena_new_array_zero(&arena, Entity, 5);
    for (int i = 0; i < 5; i++) {
        snprintf(enemies[i].name, sizeof(enemies[i].name), "enemy%d", i);
        enemies[i].health = 50;
    }

    printf("  player: %s, hp=%d\n", player->name, player->health);
    printf("  enemy[2]: %s, hp=%d\n", enemies[2].name, enemies[2].health);

    arena_destroy(&arena);
    printf("\n");
}

// another example
void example_save_restore(void) {

    unsigned char buffer[4096];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    // permanent allocation
    char *config = arena_alloc(&arena, 256);
    strcpy(config, "game_config_data");
    printf("  after config %zu bytes used\n", arena_used(&arena));

    // save position before temporary work
    arena_marker_t marker = arena_save(&arena);

    // do temporary work
    char *temp_buffer = arena_alloc(&arena, 1024);
    strcpy(temp_buffer, "temporary processing data...");
    printf("  after temp work %zu bytes used\n", arena_used(&arena));

    // restore, now temp_buffer is now invalid, but the config is still valid
    arena_reset_to(&arena, marker);
    printf("  after restore %zu bytes used\n", arena_used(&arena));
    printf("  config still valid \"%s\"\n", config);

    arena_destroy(&arena);
    printf("\n");
}

// another example, temp scope very much raii like pattern
char* process_string(arena_t *arena, const char *input) {
    size_t len = strlen(input);

    // allocate result first so it sits below the temp scope
    char *result = arena_alloc(arena, len + 1);

    // start the temporary scope
    arena_temp_t temp = arena_temp_begin(arena);

    // temporary work buffer
    char *work = arena_alloc(arena, len + 1);

    // process just uppercase for this example
    for (size_t i = 0; i < len; i++) {
        work[i] = (input[i] >= 'a' && input[i] <= 'z')
                  ? input[i] - 32
                  : input[i];
    }
    work[len] = '\0';

    // copy to result
    strcpy(result, work);

    // free temporary work buffer, result remains valid
    arena_temp_end(&temp);

    return result;
}

void example_temp_scope(void) {
    unsigned char buffer[4096];
    arena_t arena;
    arena_init(&arena, buffer, sizeof(buffer));

    char *result = process_string(&arena, "hello world");
    printf("  result \"%s\"\n", result);
    printf("  used %zu bytes (work buffer was freed)\n", arena_used(&arena));

    arena_destroy(&arena);
    printf("\n");
}

// another example, this is more or less works like frame allocator pattern
void example_frame_allocator(void) {

    unsigned char buffer[8192];
    arena_t frame_arena;
    arena_init(&frame_arena, buffer, sizeof(buffer));

    // simulate 3 frames very dumb but it will work for this example
    for (int frame = 0; frame < 3; frame++) {
        // reset at start of each frame
        arena_reset(&frame_arena);

        // allocate per frame data
        int num_entities = 10 + frame * 5;
        Vec3 *positions = arena_new_array(&frame_arena, Vec3, num_entities);

        for (int i = 0; i < num_entities; i++) {
            positions[i] = (Vec3){(float)i, 0.0f, (float)frame};
        }

        printf("  frame %d allocated %d entities, used %zu bytes\n",
               frame, num_entities, arena_used(&frame_arena));

        // at end of frame, memory is automatically reused next frame
    }

    arena_destroy(&frame_arena);
    printf("\n");
}


int main(void) {
    example_basic();
    example_typed();
    example_save_restore();
    example_temp_scope();
    example_frame_allocator();
    return 0;
}