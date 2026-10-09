#pragma once
#include "lvgl.h"
void muse_desk_create(lv_obj_t *tileview);
void muse_desk_tick(void);
void muse_hd_avatar_set_style(unsigned style);
unsigned muse_hd_avatar_style(void);
void muse_hd_avatar_action(unsigned action);
bool muse_desk_command(const char *line);
