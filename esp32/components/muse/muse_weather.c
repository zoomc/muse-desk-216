#include "muse_weather.h"
#include "muse_wifi.h"
#include "muse_state.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static muse_weather_t s_weather;
static TaskHandle_t s_task;
static const char *URL = "https://api.open-meteo.com/v1/forecast?latitude=38.91222&longitude=121.60222&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset&wind_speed_unit=ms&timezone=Asia%2FShanghai&forecast_days=1";
void muse_weather_get(muse_weather_t *out)
{
    portENTER_CRITICAL(&s_lock); *out = s_weather; portEXIT_CRITICAL(&s_lock);
}
void muse_weather_refresh(void) { if (s_task) xTaskNotifyGive(s_task); }
static bool fetch(muse_weather_t *out)
{
    char *body = heap_caps_malloc(8192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!body) return false;
    esp_http_client_config_t config = {.url=URL, .timeout_ms=15000, .buffer_size=1024,
        .crt_bundle_attach=esp_crt_bundle_attach, .disable_auto_redirect=true};
    esp_http_client_handle_t http = esp_http_client_init(&config);
    bool ok = false;
    if (http && esp_http_client_open(http,0) == ESP_OK && esp_http_client_fetch_headers(http) >= 0
        && esp_http_client_get_status_code(http) == 200) {
        size_t size = 0;
        while (size < 8191) {
            int n = esp_http_client_read(http,body+size,8191-size);
            if (n <= 0) break;
            size += n;
        }
        body[size] = 0;
        ok = size > 0 && size < 8191 && esp_http_client_is_complete_data_received(http) && muse_weather_parse(body,out);
    }
    if (http) { esp_http_client_close(http); esp_http_client_cleanup(http); }
    heap_caps_free(body);
    return ok;
}
static void worker(void *arg)
{
    (void)arg;
    int64_t next = esp_timer_get_time() + 15000000;
    int64_t last = 0;
    bool requested = false;
    for (;;) {
        if (ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000))) requested = true;
        int64_t now = esp_timer_get_time();
        if (now < next && !requested) continue;
        if (now - last < 30000000) continue;
        if (!muse_wifi_connected() || muse_state_mode(NULL) != MUSE_MODE_IDLE) continue;
        last = now; requested = false;
        muse_weather_t value = {0};
        bool ok = fetch(&value);
        if (ok) value.fetched_us = esp_timer_get_time();
        portENTER_CRITICAL(&s_lock);
        if (ok) s_weather = value;
        else s_weather.failed = true;
        portEXIT_CRITICAL(&s_lock);
        next = esp_timer_get_time() + (ok ? 1800000000LL : 60000000LL);
        ESP_LOGI("muse_weather","Dalian weather %s",ok ? "updated" : "unavailable; retaining cache");
    }
}
void muse_weather_start(void)
{
    if (!s_task) xTaskCreatePinnedToCoreWithCaps(worker,"muse_weather",8192,NULL,2,&s_task,0,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}
