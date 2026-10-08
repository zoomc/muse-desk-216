/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/*
 * Bring up the display and build the UI: the avatar on the first tile,
 * settings one swipe to the left. Also owns screen sleep and brightness.
 */
esp_err_t muse_ui_start(void);

/* From any task: the screen has gone dark for sleep (and not yet woken). */
bool muse_ui_dark(void);

/* The functions below run in the LVGL task (or with the display lock held). */

/* Slide back to the face (e.g. when a talk starts). */
void muse_ui_show_face(void);
/* Settings sub-pages turn off the tile swipe so they can use horizontal gestures. */
void muse_ui_set_swipe_enabled(bool enabled);
/* Temporarily applies a brightness while a slider is dragged. */
void muse_ui_preview_brightness(int pct);

/*
 * Presentation state machine (P1): the UI state frame_tick resolved from
 * muse_state's mode plus the sleep flag. BOOT reads as idle, OFF as sleepy.
 */
typedef enum {
    MUSE_UI_IDLE,
    MUSE_UI_LISTENING,
    MUSE_UI_THINKING,
    MUSE_UI_SPEAKING,
    MUSE_UI_SLEEPY,
    MUSE_UI_ERROR,
    MUSE_UI_COUNT,
} muse_ui_state_t;

/* The state of the last frame (LVGL task; unknown until the UI is up). */
muse_ui_state_t muse_ui_state(void);

/*
 * display.draw_url, from any task. An image covers the face until a tap, a
 * talk, the menu or muse_ui_image_hide(). Pixels are RGB565, high byte first.
 * The size is false without PSRAM for the image or before the UI is up.
 */
bool muse_ui_image_size(int *w, int *h);
bool muse_ui_image_draw(int x, int y, int w, int h, const uint16_t *pixels);
void muse_ui_image_hide(void);
/* Watcher camera mode: shows an on-screen shutter hint over the live image. */
void muse_ui_camera_hint(bool visible);

/* Bench testing, from any task: streams the screen over USB serial. */
void muse_ui_request_snapshot(void);
