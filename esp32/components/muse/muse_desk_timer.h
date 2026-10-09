#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    int64_t remaining_us;
    int64_t deadline_us;
    bool running;
    bool alarm;
} muse_desk_timer_t;
void muse_desk_timer_reset(muse_desk_timer_t *timer, int64_t duration_us);
void muse_desk_timer_toggle(muse_desk_timer_t *timer, int64_t now_us);
int64_t muse_desk_timer_remaining(muse_desk_timer_t *timer, int64_t now_us);
bool muse_desk_timer_take_alarm(muse_desk_timer_t *timer);
