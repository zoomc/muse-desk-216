/* Original procedural desk characters. Render at screen resolution, no pixel enlargement. */
#include "muse_pixel.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#endif
#define MAX_SIZE 480
static uint16_t *s_frame;
static int s_size = 320;
static unsigned s_style;
static const uint16_t *s_anime_palette;
static const uint8_t *s_anime_pixels;
static unsigned s_anime_count;
static int s_anime_size;
static float s_time, s_action_until, s_happy, s_transition_at;
static unsigned s_action = 9, s_frame_id, s_previous_id;
static int16_t s_xmap[MAX_SIZE];

static void load_art(void)
{
    const uint8_t *data = NULL;
    size_t length = 0;
#ifdef ESP_PLATFORM
    static esp_partition_mmap_handle_t handle;
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,0x42,"muse_art");
    const void *mapping = NULL;
    if (part && esp_partition_mmap(part,0,part->size,ESP_PARTITION_MMAP_DATA,&mapping,&handle) == ESP_OK) {
        data = mapping; length = part->size;
    }
#else
    const char *path = getenv("MUSE_ANIME_BIN");
    FILE *file = path ? fopen(path,"rb") : NULL;
    if (file) {
        fseek(file,0,SEEK_END); long n = ftell(file); rewind(file);
        if (n > 0 && n <= 4 * 1024 * 1024) {
            uint8_t *bytes = malloc(n);
            if (bytes && fread(bytes,1,n,file) == (size_t)n) { data = bytes; length = n; }
            else free(bytes);
        }
        fclose(file);
    }
#endif
    if (!data || length < 540 || memcmp(data,"MART",4)) return;
    uint32_t h[6]; memcpy(h,data+4,sizeof(h));
    if (h[0] != 1 || h[1] != h[2] || h[1] < 64 || h[1] > MAX_SIZE || !h[3] || h[3] > 64) return;
    size_t expected = 540 + (size_t)h[1] * h[2] * h[3];
    if (h[4] != expected || expected > length) return;
#ifdef ESP_PLATFORM
    if (esp_rom_crc32_le(0,data+28,expected-28) != h[5]) {
        ESP_LOGW("muse_avatar","art checksum invalid; using procedural fallback"); return;
    }
    ESP_LOGI("muse_avatar","anime art ready: %lux%lu, %lu poses, %u bytes",(unsigned long)h[1],(unsigned long)h[2],(unsigned long)h[3],(unsigned)expected);
#endif
    s_anime_palette = (const uint16_t *)(data+28);
    s_anime_pixels = data+540; s_anime_size = h[1]; s_anime_count = h[3];
}

void muse_hd_avatar_set_style(unsigned style) { s_style = style % 4; }
unsigned muse_hd_avatar_style(void) { return s_style; }
void muse_hd_avatar_action(unsigned action)
{
    s_style = 0; s_action = 9 + action % 9; s_action_until = s_time + 4;
}
static uint16_t rgb(unsigned c)
{
    return ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
}
static void pixel(int x, int y, uint16_t c, float opacity)
{
    if (!s_frame || x < 0 || y < 0 || x >= s_size || y >= s_size) return;
    if (opacity >= 1) { s_frame[y * s_size + x] = c; return; }
    if (opacity <= 0) return;
    unsigned a = (unsigned)(opacity * 256), b = 256 - a;
    uint16_t old = s_frame[y * s_size + x];
    s_frame[y * s_size + x] = ((((c >> 11) * a + (old >> 11) * b) >> 8) << 11)
        | (((((c >> 5) & 63) * a + ((old >> 5) & 63) * b) >> 8) << 5)
        | (((c & 31) * a + (old & 31) * b) >> 8);
}
static void span(int y, float left, float right, uint16_t color)
{
    int lo = (int)floorf(left), hi = (int)ceilf(right);
    if (lo < -1) lo = -1;
    if (hi > s_size) hi = s_size;
    for (int x = lo; x <= hi; ++x) {
        float cover = fminf(x + 1, right) - fmaxf(x, left);
        pixel(x, y, color, cover);
    }
}
// Coordinates use a 1000-unit design space; edges receive subpixel coverage.
static void ellipse(float x, float y, float rx, float ry, unsigned color)
{
    float scale = s_size / 1000.f;
    x *= scale; y *= scale; rx *= scale; ry *= scale;
    if (rx <= 0 || ry <= 0) return;
    int top = (int)floorf(y - ry), bottom = (int)ceilf(y + ry);
    uint16_t c = rgb(color);
    for (int row = top; row <= bottom; ++row) {
        if (row < 0 || row >= s_size) continue;
        float d = (row + .5f - y) / ry;
        if (fabsf(d) >= 1) continue;
        float w = rx * sqrtf(1 - d * d);
        span(row, x - w, x + w, c);
    }
}
static void round_rect(float x, float y, float w, float h, float radius, unsigned color)
{
    float scale = s_size / 1000.f;
    x *= scale; y *= scale; w *= scale; h *= scale; radius *= scale;
    int top = (int)floorf(y), bottom = (int)ceilf(y + h);
    uint16_t c = rgb(color);
    for (int row = top; row < bottom; ++row) {
        if (row < 0 || row >= s_size) continue;
        float d = row + .5f - y;
        float edge = d < radius ? radius - d : d > h - radius ? d - (h - radius) : 0;
        float inset = edge > 0 ? radius - sqrtf(fmaxf(0, radius * radius - edge * edge)) : 0;
        span(row, x + inset, x + w - inset, c);
    }
}
static void heart(float x, float y, float size)
{
    ellipse(x - size * .26f, y, size * .34f, size * .34f, 0xff7dac);
    ellipse(x + size * .26f, y, size * .34f, size * .34f, 0xff7dac);
    ellipse(x, y + size * .28f, size * .42f, size * .43f, 0xff7dac);
}
uint32_t muse_pixel_accent(muse_mode_t mode)
{
    static const unsigned colors[] = {0xa8a1ff,0x79ddff,0x72edbe,0xffce80,0xbcabff,0xff819e,0x8c93af};
    return colors[(unsigned)mode < MUSE_MODE_COUNT ? mode : MUSE_MODE_IDLE];
}
void muse_pixel_set_size(int px)
{
    s_size = px < 1 ? 1 : px > MAX_SIZE ? MAX_SIZE : px;
    if (!s_frame) {
#ifdef ESP_PLATFORM
        s_frame = heap_caps_calloc(MAX_SIZE * MAX_SIZE, sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
        s_frame = calloc(MAX_SIZE * MAX_SIZE, sizeof(uint16_t));
#endif
        load_art();
    }
}
static uint16_t mix565(uint16_t a, uint16_t b, unsigned blend)
{
    unsigned inverse = 256 - blend;
    return ((((a >> 11) * inverse + (b >> 11) * blend) >> 8) << 11)
        | (((((a >> 5) & 63) * inverse + ((b >> 5) & 63) * blend) >> 8) << 5)
        | (((a & 31) * inverse + (b & 31) * blend) >> 8);
}
static void anime(const muse_pose_t *p)
{
    unsigned frame = 0;
    s_time = p->t;
    if (p->happy > s_happy + .08f && s_anime_count >= 18) {
        s_action = 9 + ((unsigned)(p->t * 100) % 9); s_action_until = p->t + 3;
    }
    s_happy = p->happy;
    if (p->mode == MUSE_MODE_BOOT) frame = 9;
    else if (p->mode == MUSE_MODE_OFF) frame = 7;
    else if (p->mode == MUSE_MODE_LISTENING) frame = 2;
    else if (p->mode == MUSE_MODE_THINKING) frame = p->mode_t > 3 ? 12 : 3;
    else if (p->mode == MUSE_MODE_ERROR) frame = 14;
    else if (p->mode == MUSE_MODE_SPEAKING) frame = p->level < .07f ? 0 : p->level > .45f ? 5 : 4;
    else if (p->t < s_action_until) frame = s_action;
    else if (p->happy > .15f) frame = 6;
    else if (fmodf(p->t,5.1f) > 4.93f) frame = 1;
    else if (p->mode_t > 10 && fmodf(p->t,24) < 2.4f) {
        static const unsigned poses[] = {9,10,11,13,16,17};
        frame = poses[((unsigned)(p->t / 24)) % 6];
    }
    frame %= s_anime_count;
    if (frame != s_frame_id) {
        s_previous_id = s_frame_id; s_frame_id = frame; s_transition_at = p->t;
    }
    unsigned blend = (unsigned)fmaxf(0,fminf(256,(p->t - s_transition_at) * 256 / .12f));
    float scale = s_size * .94f / s_anime_size;
    float shift = s_size * .025f + sinf(p->t * 1.8f) * s_size * .006f;
    for (int x = 0; x < s_size; ++x) s_xmap[x] = (int)((x - s_size * .03f) / scale);
    for (int y = 0; y < s_size; ++y) {
        int sy = (int)((y - shift) / scale);
        if (y < shift || sy < 0 || sy >= s_anime_size) continue;
        const uint8_t *current = s_anime_pixels + ((size_t)s_frame_id * s_anime_size + sy) * s_anime_size;
        const uint8_t *old = s_anime_pixels + ((size_t)s_previous_id * s_anime_size + sy) * s_anime_size;
        uint16_t *out = s_frame + y * s_size;
        for (int x = 0; x < s_size; ++x) {
            int sx = s_xmap[x];
            if (x < s_size * .03f || sx < 0 || sx >= s_anime_size) continue;
            uint16_t c = s_anime_palette[current[sx]];
            out[x] = blend < 256 ? mix565(s_anime_palette[old[sx]],c,blend) : c;
        }
    }
}
void muse_pixel_render(const muse_pose_t *p)
{
    if (!s_frame || !p) return;
    memset(s_frame, 0, s_size * s_size * sizeof(uint16_t));
    if (!s_style && s_anime_pixels) { anime(p); return; }
    unsigned style = s_style ? s_style - 1 : 0;
    unsigned accent = muse_pixel_accent(p->mode);
    float bob = sinf(p->t * 2.1f) * 8;
    float happy = p->happy;
    if (happy > 1) happy = 1;
    if (happy < 0) happy = 0;
    bob += happy * sinf(p->mode_t * 14 + p->t * 9) * 15;
    float breath = sinf(p->t * 1.8f) * 4;
    float look = sinf(p->t * .63f) * 7;
    bool blink = fmodf(p->t, 4.9f) > 4.73f || p->mode == MUSE_MODE_OFF;
    float level = fmaxf(0, fminf(1, p->level));
    // A dark halo and contact shadow ground the figure on OLED black.
    ellipse(500, 505, 360, 370, 0x080d19);
    ellipse(500, 866, 220, 32, 0x121b2d);
    if (style == 2) {
        ellipse(500, 500 + bob, 278 + breath, 278 + breath, 0x202944);
        ellipse(500, 482 + bob, 251 + breath, 251 + breath, 0x8ad6f0);
        ellipse(454, 409 + bob, 172, 160, 0xbdeaf4);
    } else {
        // Floating hands wave when petted; the cat has ears and a curled tail.
        float wave = happy * sinf(p->t * 12) * 62;
        ellipse(256, 670 + bob - wave, 47, 94, 0x283852);
        ellipse(744, 670 + bob + wave, 47, 94, 0x283852);
        ellipse(255, 659 + bob - wave, 38, 83, style ? 0xffd0b7 : 0xd9eaf4);
        ellipse(745, 659 + bob + wave, 38, 83, style ? 0xffd0b7 : 0xd9eaf4);
        ellipse(410, 828 + bob, 72, 33, 0x283852);
        ellipse(590, 828 + bob, 72, 33, 0x283852);
        round_rect(320, 573 + bob, 360, 253 + breath, 99, 0x283852);
        round_rect(330, 573 + bob, 340, 238 + breath, 92, style ? 0xffd6bf : 0xdcebf4);
        ellipse(500, 674 + bob, 75, 53, style ? 0xffa898 : accent);
        ellipse(485, 659 + bob, 17, 10, 0xffffff);
        if (style) {
            ellipse(330, 314 + bob, 87, 134, 0x283852);
            ellipse(670, 314 + bob, 87, 134, 0x283852);
            ellipse(330, 304 + bob, 73, 119, 0xffd6bf);
            ellipse(670, 304 + bob, 73, 119, 0xffd6bf);
            ellipse(330, 304 + bob, 43, 78, 0xffaeb2);
            ellipse(670, 304 + bob, 43, 78, 0xffaeb2);
        } else {
            round_rect(485, 180 + bob, 30, 82, 15, 0x657a9c);
            ellipse(500, 183 + bob, 28, 28, accent);
            ellipse(491, 174 + bob, 9, 9, 0xffffff);
        }
        round_rect(230, 269 + bob, 540, 340, 116, 0x283852);
        round_rect(240, 269 + bob, 520, 325, 108, style ? 0xffdfc8 : 0xe6f0f6);
        round_rect(270, 321 + bob, 460, 219, 85, 0x101d35);
        round_rect(283, 330 + bob, 434, 197, 77, 0x182841);
        // Fine highlights remain smooth at the actual display resolution.
        round_rect(305, 293 + bob, 168, 10, 5, 0xffffff);
    }
    float ey = 420 + bob;
    unsigned eye = style == 2 ? 0x142b45 : accent;
    if (blink || happy > .25f) {
        round_rect(355 + look, ey, 82, 13, 6, eye);
        round_rect(563 + look, ey, 82, 13, 6, eye);
    } else if (p->mode == MUSE_MODE_ERROR) {
        round_rect(355, ey - 19, 85, 12, 5, 0xff819e);
        round_rect(563, ey + 7, 85, 12, 5, 0xff819e);
    } else {
        float wide = p->mode == MUSE_MODE_LISTENING ? 30 : 23;
        ellipse(397 + look, ey, wide, 36 + level * 13, eye);
        ellipse(603 + look, ey, wide, 36 + level * 13, eye);
        ellipse(389 + look, ey - 13, 7, 10, 0xffffff);
        ellipse(595 + look, ey - 13, 7, 10, 0xffffff);
    }
    if (p->mode == MUSE_MODE_SPEAKING) {
        ellipse(500, 479 + bob, 32 + level * 8, 9 + level * 27, eye);
        if (level > .3f) ellipse(500, 490 + bob, 17, 6, 0xff9cae);
    } else if (p->mode == MUSE_MODE_THINKING) {
        ellipse(504, 480 + bob, 13, 10, eye);
        for (int i = 0; i < 3; ++i) {
            float a = p->t * 2.5f + i * 2.0944f;
            ellipse(500 + cosf(a) * 309, 453 + sinf(a) * 228, 12, 12, accent);
        }
    } else {
        round_rect(473, 471 + bob, 54, happy > .25f ? 19 : 10, 5, eye);
    }
    if (p->mode == MUSE_MODE_LISTENING) {
        for (int i = 0; i < 3; ++i) {
            float h = 20 + 60 * level + 12 * sinf(p->t * 8 + i);
            round_rect(125 + i * 28, 450 - h / 2, 14, h, 7, accent);
            round_rect(805 + i * 28, 450 - h / 2, 14, h, 7, accent);
        }
    }
    if (happy > .1f) {
        float rise = fmodf(p->t * 85, 105);
        heart(225, 250 - rise, 36);
        heart(770, 302 - rise, 27);
        ellipse(336, 484 + bob, 28, 9, 0xea94a9);
        ellipse(664, 484 + bob, 28, 9, 0xea94a9);
    }
    if (p->mode == MUSE_MODE_OFF) {
        for (int i = 0; i < 3; ++i) round_rect(755 + i * 27, 225 - i * 25, 18, 8, 3, 0x8c93af);
    }
}
void muse_pixel_scale(uint16_t *dst, int stride, int x0, int x1, int y0, int y1)
{
    for (int y = y0; y <= y1; ++y, dst += stride) {
        for (int x = x0; x <= x1; ++x) {
            dst[x - x0] = s_frame && x >= 0 && y >= 0 && x < s_size && y < s_size ? s_frame[y * s_size + x] : 0;
        }
    }
}
