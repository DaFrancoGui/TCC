#include "finger_detect.h"
#include <string.h>
/* Tune with raw data if LED current or the optical module changes (~7.2 mA here). */
#define FINGER_UP 4500u
#define FINGER_DOWN 3000u
void finger_init(finger_state_t *f) { memset(f,0,sizeof(*f)); }
bool finger_update(finger_state_t *f, uint32_t ir)
{
    if (ir > f->baseline+FINGER_UP) { if (f->up < 3) f->up++; f->down=0; }
    else if (ir < f->baseline+FINGER_DOWN) { if (f->down < 3) f->down++; f->up=0; }
    if (!f->present && f->up >= 3) f->present=true;
    if (f->present && f->down >= 3) f->present=false;
    /* Do not learn a possible finger as the initial empty baseline. */
    if (!f->present && !f->up && ir < f->baseline+2000) f->baseline=(f->baseline*7+ir)/8;
    return f->present;
}
