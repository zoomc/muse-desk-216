#include "muse_desk_timer.h"
void muse_desk_timer_reset(muse_desk_timer_t *t, int64_t duration)
{
    *t = (muse_desk_timer_t){.remaining_us = duration > 0 ? duration : 0};
}
int64_t muse_desk_timer_remaining(muse_desk_timer_t *t, int64_t now)
{
    if (t->running) {
        t->remaining_us = t->deadline_us > now ? t->deadline_us - now : 0;
        if (!t->remaining_us) { t->running = false; t->alarm = true; }
    }
    return t->remaining_us;
}
void muse_desk_timer_toggle(muse_desk_timer_t *t, int64_t now)
{
    muse_desk_timer_remaining(t, now);
    if (t->running) t->running = false;
    else if (t->remaining_us > 0) {
        t->deadline_us = now + t->remaining_us;
        t->running = true;
        t->alarm = false;
    }
}
bool muse_desk_timer_take_alarm(muse_desk_timer_t *t)
{
    bool alarm = t->alarm;
    t->alarm = false;
    return alarm;
}
