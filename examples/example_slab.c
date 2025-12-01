#include <stdio.h>
#include <string.h>

#define SLAB_IMPLEMENTATION
#include "../slab.h"

// super simple slab example

typedef struct {
    float x, y;
    float vx, vy;
} Bullet;  // this struct is 16 bytes

typedef struct {
    float x, y;
    int health;
    char name[20];
} Enemy;  // 32 bytes

typedef struct {
    float x, y;
    int health;
    int score;
    char name[32];
    float inventory[16];
} Player;  // this should be around 112 bytes

int main(void)
{
    // first we choose the size classes based on the objects
    size_t sizes[] = {
        sizeof(Bullet),  // ~16 bytes  will be 16 or 24 after alignment
        sizeof(Enemy),   // ~32 bytes  will be 32 or 40
        sizeof(Player)   // ~112 bytes will be 112 or 120
    };

    // then Allocate a buffer which can be stack, heap, or static
    static uint8_t buffer[32768];

    // then initialize the slab
    slab_t slab;
    memset(&slab, 0, sizeof(slab));

    int result = slab_init(&slab, buffer, sizeof(buffer), sizes, 3);
    if (result != SLAB_OK) {
        printf("failed to init slab: %d\n", result);
        return 1;
    }

    // print some stats
    printf("Slab initialized!\n");
    printf("  Classes: %zu\n", slab_class_count(&slab));
    printf("  Max allocatable: %zu bytes\n", slab_max_alloc(&slab));
    printf("\n");

    // now we can Allocate objects
    Bullet *bullet = slab_alloc(&slab, sizeof(Bullet));
    Enemy *enemy = slab_alloc(&slab, sizeof(Enemy));
    Player *player = slab_calloc(&slab, sizeof(Player));  // zero initialized

    if (!bullet || !enemy || !player) {
        printf("allocation failed!\n");
        return 1;
    }

    // use the objects
    bullet->x = 100.0f;
    bullet->y = 200.0f;
    bullet->vx = 10.0f;
    bullet->vy = 0.0f;

    enemy->x = 300.0f;
    enemy->y = 200.0f;
    enemy->health = 100;
    strcpy(enemy->name, "Goblin");

    // player was calloc'd, so health starts at 0
    player->health = 100;
    player->score = 0;
    strcpy(player->name, "hero");

    printf("  bullet at %.1f, %.1f\n", bullet->x, bullet->y);
    printf("  enemy '%s' at %.1f, %.1f with %d HP\n",
           enemy->name, enemy->x, enemy->y, enemy->health);
    printf("  player '%s' with %d HP\n", player->name, player->health);
    printf("\n");

    // check stats again
    slab_stats_t stats = slab_stats(&slab);
    printf("Memory stats:\n");
    printf("  Total slots: %zu\n", stats.total_slots);
    printf("  Used slots: %zu\n", stats.used_slots);
    printf("  Free slots: %zu\n", stats.free_slots);
    printf("\n");

    // allocate more bullets
    Bullet *bullets[50];
    for (int i = 0; i < 50; i++) {
        bullets[i] = slab_alloc(&slab, sizeof(Bullet));
        if (bullets[i]) {
            bullets[i]->x = (float)i * 10.0f;
            bullets[i]->y = 100.0f;
        }
    }

    stats = slab_stats(&slab);
    printf("after spawning %zu used, %zu free\n",
           stats.used_slots, stats.free_slots);

    // free some bullets
    for (int i = 0; i < 25; i++) {
        slab_free(&slab, bullets[i]);
        bullets[i] = NULL;
    }

    stats = slab_stats(&slab);
    printf("after freeing %zu used, %zu free\n",
           stats.used_slots, stats.free_slots);
    printf("\n");

    // reset everything when we need like starting a new level
    slab_reset(&slab);

    stats = slab_stats(&slab);
    printf("after reset %zu used, %zu free\n",
           stats.used_slots, stats.free_slots);
    printf("\n");

    // once everything is done , just clean up
    slab_destroy(&slab);

    return 0;
}