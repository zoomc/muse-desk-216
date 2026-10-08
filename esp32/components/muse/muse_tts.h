/* Local streaming speech for Muse replies. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MUSE_TTS_TEXT_BYTES 4096
typedef struct muse_tts_request muse_tts_request_t;

/* One HTTP worker at a time; a cancelled worker finishes without blocking chat. */
bool muse_tts_busy(void);
muse_tts_request_t *muse_tts_begin(const char *text);
size_t muse_tts_read(muse_tts_request_t *request, uint8_t *out, size_t capacity);
/* True only after the worker has ended and all queued bytes have been read. */
bool muse_tts_finished(muse_tts_request_t *request, bool *ok);
/* Releases the consumer's reference immediately; worker cleanup is deferred. */
void muse_tts_close(muse_tts_request_t **request);

#ifdef __cplusplus
}
#endif
