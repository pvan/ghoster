

#define RND_IMPLEMENTATION
#include "../lib/rnd.h"


#include <time.h>  // for seeding



rnd_pcg_t rand_pcg;
bool rand_initialized = false;

void rand_init()
{
    // rnd_pcg_seed( &rand_pcg, 0u );  // static seed, same on every run

    time_t seconds;
    time(&seconds);
    rnd_pcg_seed(&rand_pcg, (RND_U32)seconds);

    rand_initialized = true;
}

int rand_int(int upToAndNotIncluding)
{
    if (!rand_initialized) rand_init();

    return rnd_pcg_range(&rand_pcg, 0, upToAndNotIncluding-1);
    // // todo: oddly this only-seed-once code is throwing something off
    // // todo: replace with proper randomness anyway (lib?)

    // // static bool global_already_seeded_rand = false;
    // // if (!global_already_seeded_rand)
    // // {
    //     // global_already_seeded_rand = true;
    //     srand(GetTickCount());
    // // }
    // return rand() % upToAndNotIncluding;
}


