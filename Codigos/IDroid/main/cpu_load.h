#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct { uint32_t idle, total; bool seeded; int percent; } cpu_load_t;
/* Unsigned deltas handle the 32-bit ESP timer runtime counter wrap. */
static inline int cpu_load_sample(cpu_load_t *s, uint32_t total, uint32_t idle)
{
    uint32_t dt=total-s->total, di=idle-s->idle;
    if (s->seeded && dt) {
        if (di > dt) di=dt;
        s->percent=100-(int)((uint64_t)di*100/dt);
    } else if (!s->seeded) s->percent=-1;
    s->idle=idle; s->total=total; s->seeded=true;
    return s->percent;
}
