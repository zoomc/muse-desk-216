/* Only the requested additions: Pomodoro and Dalian weather. */
#include "muse_desk.h"
#include "muse_desk_timer.h"
#include "muse_weather.h"
#include "muse_state.h"
#include "muse_wifi.h"
#include "muse_voice.h"
#include "esp_timer.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#define PAGE_COUNT 4
LV_FONT_DECLARE(muse_font_cjk_16);
static lv_font_t s_font;
static lv_obj_t *s_tv, *s_pages[PAGE_COUNT];
static lv_obj_t *s_timer_lbl, *s_timer_hint, *s_round, *s_progress;
static lv_obj_t *s_weather_title, *s_weather_details, *s_weather_age;
static muse_desk_timer_t s_timer;
static bool s_rest, s_sntp_started;
static unsigned s_rounds;
static int s_minutes = 25;
static int64_t s_next_tick;
static _Atomic unsigned s_pending = UINT32_MAX;
static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_width(obj, width); lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_text_color(obj, lv_color_hex(0xe6eafb), 0);
    lv_obj_set_style_text_font(obj, &s_font, 0); lv_label_set_text(obj, text);
    return obj;
}
static void set_phase(bool rest, int minutes, const char *hint)
{
    s_rest = rest; s_minutes = minutes;
    muse_desk_timer_reset(&s_timer, (int64_t)minutes * 60000000);
    lv_label_set_text(s_timer_hint, hint);
    s_next_tick = 0;
}
static void perform_action(unsigned id)
{
    muse_state_poke();
    if (id >= 100 && id < 100 + PAGE_COUNT) {
        lv_tileview_set_tile(s_tv, s_pages[id - 100], LV_ANIM_ON);
    } else if (id == 20) {
        muse_desk_timer_toggle(&s_timer, esp_timer_get_time());
        lv_label_set_text(s_timer_hint, s_timer.running ? (s_rest ? "休息进行中" : "专注进行中") : "已暂停，点击继续");
    } else if (id == 21) {
        s_rounds = 0; set_phase(false, 25, "25分钟专注，点击开始");
    } else if (id == 22) {
        set_phase(false, 25, "25分钟专注，点击开始");
    } else if (id == 23) {
        set_phase(true, 5, "5分钟休息，点击开始");
    } else if (id == 70) {
        muse_weather_refresh(); lv_label_set_text(s_weather_age, "已请求刷新，后台获取中");
    }
#if CONFIG_LV_USE_SNAPSHOT
    else if (id == 90) {
        s_rest = false; muse_desk_timer_reset(&s_timer, 2000000);
        muse_desk_timer_toggle(&s_timer, esp_timer_get_time());
    }
#endif
    s_next_tick = 0;
}
static void action(lv_event_t *event) { perform_action((unsigned)(uintptr_t)lv_event_get_user_data(event)); }
static void button(lv_obj_t *parent, const char *text, int x, int y, int w, unsigned id)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, 46);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x2c3b62), 0);
    lv_obj_set_style_radius(obj, 15, 0);
    lv_obj_add_event_cb(obj, action, LV_EVENT_CLICKED, (void *)(uintptr_t)id);
    lv_obj_t *text_obj = lv_label_create(obj);
    lv_obj_set_style_text_font(text_obj, &s_font, 0);
    lv_label_set_text(text_obj, text); lv_obj_center(text_obj);
}
static lv_obj_t *page(unsigned index, const char *title)
{
    lv_obj_t *obj = lv_tileview_add_tile(s_tv, index, 0, index == PAGE_COUNT - 1 ? LV_DIR_LEFT : LV_DIR_HOR);
    s_pages[index] = obj;
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x090e1c), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    label(obj, title, 25, 22, 320); button(obj, "人偶", 365, 10, 85, 100);
    label(obj, "左右滑动切换 · KEY 按住说话", 89, 451, 330);
    return obj;
}
bool muse_desk_command(const char *line)
{
    if (!strncmp(line, "desk.page=", 10)) {
        char *end; unsigned value = strtoul(line + 10, &end, 10);
        if (end != line + 10 && !*end && value < PAGE_COUNT) atomic_store(&s_pending, 100 + value);
        return true;
    }
#if CONFIG_LV_USE_SNAPSHOT
    if (!strcmp(line, "desk.timer_test")) { atomic_store(&s_pending, 90); return true; }
    if (!strcmp(line, "desk.timer_reset")) { atomic_store(&s_pending, 21); return true; }
#endif
    return false;
}
void muse_desk_create(lv_obj_t *tileview)
{
    s_tv = tileview; s_pages[0] = lv_tileview_get_tile_active(s_tv);
    s_font = lv_font_montserrat_16; s_font.fallback = &muse_font_cjk_16;
    muse_hd_avatar_set_style(0);
    lv_obj_t *timer = page(2, "番茄专注");
    s_timer_lbl = label(timer, "25:00", 135, 97, 230);
    lv_obj_set_style_text_font(s_timer_lbl, &lv_font_montserrat_48, 0);
    s_timer_hint = label(timer, "25分钟专注，点击开始", 70, 175, 370);
    s_progress = lv_bar_create(timer);
    lv_obj_set_pos(s_progress, 40, 226); lv_obj_set_size(s_progress, 400, 10);
    lv_obj_set_style_bg_color(s_progress, lv_color_hex(0x78c7dd), LV_PART_INDICATOR);
    s_round = label(timer, "已完成 0 个番茄", 115, 253, 330);
    button(timer, "开始 / 暂停", 25, 310, 205, 20);
    button(timer, "重置", 249, 310, 205, 21);
    button(timer, "专注 25 分钟", 25, 378, 205, 22);
    button(timer, "休息 5 分钟", 249, 378, 205, 23);
    set_phase(false, 25, "25分钟专注，点击开始");
    lv_obj_t *weather = page(3, "大连天气 / 今日");
    s_weather_title = label(weather, "-- C", 48, 100, 380);
    lv_obj_set_style_text_font(s_weather_title, &lv_font_montserrat_48, 0);
    s_weather_details = label(weather, "连接 Wi-Fi 后自动更新", 48, 185, 384);
    s_weather_age = label(weather, "Open-Meteo · 约每30分钟更新", 35, 372, 402);
    button(weather, "刷新天气", 126, 406, 228, 70);
    muse_weather_start();
    ESP_LOGI("muse_desk", "minimal pages ready: Pomodoro, Dalian weather, HD anime");
}
void muse_desk_tick(void)
{
    if (!s_tv) return;
    unsigned pending = atomic_exchange(&s_pending, UINT32_MAX);
    if (pending != UINT32_MAX) perform_action(pending);
    int64_t now = esp_timer_get_time();
    if (!s_sntp_started && muse_wifi_connected()) {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2, ESP_SNTP_SERVER_LIST("time.cloudflare.com", "pool.ntp.org"));
        s_sntp_started = esp_netif_sntp_init(&cfg) == ESP_OK;
    }
    if (now < s_next_tick) return;
    s_next_tick = now + 1000000;
    int64_t seconds = (muse_desk_timer_remaining(&s_timer, now) + 999999) / 1000000;
    lv_label_set_text_fmt(s_timer_lbl, "%02lld:%02lld", seconds / 60, seconds % 60);
    lv_bar_set_value(s_progress, s_minutes ? 100 - seconds * 100 / (s_minutes * 60) : 0, LV_ANIM_OFF);
    if (muse_desk_timer_take_alarm(&s_timer)) {
        if (!s_rest) {
            ++s_rounds; int rest = s_rounds % 4 == 0 ? 15 : 5;
            set_phase(true, rest, rest == 15 ? "专注完成！长休息15分钟，点击开始" : "专注完成！休息5分钟，点击开始");
        } else set_phase(false, 25, "休息结束！下个番茄，点击开始");
        muse_state_set_asleep(false); lv_tileview_set_tile(s_tv, s_pages[2], LV_ANIM_ON);
        muse_voice_request_chirp(); muse_state_make_happy();
        ESP_LOGI("muse_desk", "Pomodoro phase complete; next=%s", s_rest ? "rest" : "focus");
    }
    lv_label_set_text_fmt(s_round, "已完成 %u 个番茄", s_rounds);
    muse_weather_t weather; muse_weather_get(&weather);
    if (weather.valid) {
        lv_label_set_text_fmt(s_weather_title, "%.1f C", weather.temperature);
        lv_label_set_text_fmt(s_weather_details,
            "%s · 体感 %.1f C\n\n今日 %.1f ~ %.1f C\n湿度 %.0f%% · 风速 %.1f m/s\n\n日出 %.5s · 日落 %.5s\n数据时间：%s",
            muse_weather_condition(weather.code), weather.apparent, weather.low, weather.high, weather.humidity, weather.wind,
            weather.sunrise + 11, weather.sunset + 11, weather.time);
        lv_label_set_text_fmt(s_weather_age, "%s · %lld分钟前更新 · Open-Meteo",
            weather.failed || !muse_wifi_connected() ? "缓存" : "已更新", (now - weather.fetched_us) / 60000000);
    } else {
        lv_label_set_text(s_weather_title, "-- C");
        lv_label_set_text(s_weather_details, weather.failed ? "天气暂时不可用，稍后自动重试。" : "正在后台获取大连天气…");
    }
}
