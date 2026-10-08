#include "muse_tts.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include <strings.h>
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/stream_buffer.h"
#include "freertos/task.h"

static const char *TAG = "muse_tts";
static constexpr size_t QUEUE_BYTES = 8192;
static std::atomic<bool> s_busy{false};

struct muse_tts_request {
    std::atomic<unsigned> refs{2}; // chat task and HTTP worker
    std::atomic<bool> cancelled{false};
    std::atomic<int> result{0};     // running / complete / failed
    StaticStreamBuffer_t control{};
    StreamBufferHandle_t bytes = nullptr;
    uint8_t *storage = nullptr;
    char *text = nullptr;
    bool mp3_type = false;
};

static void release(muse_tts_request_t *r)
{
    if (r->refs.fetch_sub(1) == 1) {
        if (r->bytes) vStreamBufferDelete(r->bytes);
        heap_caps_free(r->storage);
        heap_caps_free(r->text);
        delete r;
    }
}

static esp_err_t on_http(esp_http_client_event_t *ev)
{
    auto *r = static_cast<muse_tts_request_t *>(ev->user_data);
    if (ev->event_id == HTTP_EVENT_ON_HEADER && ev->header_key && ev->header_value &&
        !strcasecmp(ev->header_key, "Content-Type")) {
        r->mp3_type = !strncasecmp(ev->header_value, "audio/mpeg", 10);
    }
    return ESP_OK;
}

static void worker(void *arg)
{
    auto *r = static_cast<muse_tts_request_t *>(arg);
    esp_http_client_config_t cfg{};
    cfg.url = CONFIG_MUSE_LOCAL_TTS_URL;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 2000;
    cfg.buffer_size = 2048;
    cfg.event_handler = on_http;
    cfg.user_data = r;
    cfg.disable_auto_redirect = true;
    auto http = esp_http_client_init(&cfg);
    bool ok = false;
    size_t total = 0;
    if (http) {
        esp_http_client_set_header(http, "Content-Type", "text/plain; charset=utf-8");
        esp_http_client_set_header(http, "Accept", "audio/mpeg");
        // The speech server requires a Bearer token on /tts (see tts-server/).
        // Empty token sends no header; the server then answers 401 and the
        // reply falls back to captions instead of speech.
        char auth[160] = {0};
        if (CONFIG_MUSE_LOCAL_TTS_TOKEN[0]) {
            snprintf(auth, sizeof(auth), "Bearer %s", CONFIG_MUSE_LOCAL_TTS_TOKEN);
            esp_http_client_set_header(http, "Authorization", auth);
        }
        size_t length = strlen(r->text), sent = 0;
        if (!r->cancelled && esp_http_client_open(http, length) == ESP_OK) {
            while (sent < length && !r->cancelled) {
                int n = esp_http_client_write(http, r->text + sent, length - sent);
                if (n <= 0) break;
                sent += n;
            }
            if (sent == length && !r->cancelled && esp_http_client_fetch_headers(http) >= 0 &&
                esp_http_client_get_status_code(http) == 200 && r->mp3_type) {
                esp_http_client_set_timeout_ms(http, 500);
                int64_t last_data = esp_timer_get_time();
                uint8_t chunk[2048];
                while (!r->cancelled) {
                    int n = esp_http_client_read(http, reinterpret_cast<char *>(chunk), sizeof(chunk));
                    if (n > 0) {
                        size_t off = 0;
                        while (off < static_cast<size_t>(n) && !r->cancelled) {
                            off += xStreamBufferSend(r->bytes, chunk + off, n - off, pdMS_TO_TICKS(100));
                        }
                        total += off;
                        last_data = esp_timer_get_time(); // backpressure is not a network timeout
                    } else if (esp_http_client_is_complete_data_received(http)) {
                        ok = total > 0;
                        break;
                    } else if (n != -ESP_ERR_HTTP_EAGAIN) {
                        break; // truncated HTTP body or connection error
                    } else if (esp_timer_get_time() - last_data > 20000000LL) {
                        break;
                    }
                }
            }
        }
        esp_http_client_close(http);
        esp_http_client_cleanup(http);
    }
    if (!r->cancelled) {
        ESP_LOGI(TAG, "speech stream %s (%u bytes)", ok ? "complete" : "failed", (unsigned)total);
    }
    r->result.store(ok ? 1 : -1);
    s_busy.store(false);
    release(r);
    vTaskDeleteWithCaps(nullptr);
}

extern "C" bool muse_tts_busy(void) { return s_busy.load(); }

extern "C" muse_tts_request_t *muse_tts_begin(const char *text)
{
    if (!text || !text[0] || strncmp(CONFIG_MUSE_LOCAL_TTS_URL, "http://", 7)) return nullptr;
    bool expected = false;
    if (!s_busy.compare_exchange_strong(expected, true)) return nullptr;
    auto *r = new (std::nothrow) muse_tts_request_t;
    if (!r) { s_busy.store(false); return nullptr; }
    // PSRAM only: Wi-Fi connect momentarily leaves ~6KB of internal RAM, so
    // even an 8KB+4KB TTS allocation must avoid internal RAM or TLS/audio starve.
    r->storage = static_cast<uint8_t *>(heap_caps_malloc(QUEUE_BYTES + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    r->text = static_cast<char *>(heap_caps_malloc(MUSE_TTS_TEXT_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (r->storage && r->text) {
        size_t n = strnlen(text, MUSE_TTS_TEXT_BYTES - 1);
        memcpy(r->text, text, n);
        // A caption buffer may end inside a UTF-8 character. Do not send that suffix.
        if (n) {
            size_t lead = n - 1;
            while (lead && (static_cast<uint8_t>(r->text[lead]) & 0xc0) == 0x80) --lead;
            uint8_t c = r->text[lead];
            size_t width = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
            if (n - lead < width) n = lead;
        }
        r->text[n] = '\0';
        r->bytes = xStreamBufferCreateStatic(QUEUE_BYTES + 1, 1, r->storage, &r->control);
    }
    if (!r->bytes || !r->text[0] ||
        xTaskCreatePinnedToCoreWithCaps(worker, "muse_tts", 8192, r, 4, nullptr, 0,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        r->refs.store(1);
        release(r);
        s_busy.store(false);
        return nullptr;
    }
    return r;
}

extern "C" size_t muse_tts_read(muse_tts_request_t *r, uint8_t *out, size_t cap)
{
    return r && out && cap ? xStreamBufferReceive(r->bytes, out, cap, 0) : 0;
}

extern "C" bool muse_tts_finished(muse_tts_request_t *r, bool *ok)
{
    int result = r ? r->result.load() : -1;
    if (!result || (r && xStreamBufferBytesAvailable(r->bytes))) return false;
    if (ok) *ok = result > 0;
    return true;
}

extern "C" void muse_tts_close(muse_tts_request_t **request)
{
    if (!request || !*request) return;
    auto *r = *request;
    *request = nullptr;
    r->cancelled.store(true);
    release(r);
}
