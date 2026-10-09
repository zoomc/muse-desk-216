import os, subprocess, tempfile, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class LcdHealth(unittest.TestCase):
    def test_idle_sleep_and_completed_dma_do_not_reset_but_lost_final_dma_does(self):
        s=(ROOT/'components/muse/boards/muse_lcd_bands.c').read_text(encoding='utf-8')
        functions=s[s.index('static void lcd_health_check('):s.index('static bool IRAM_ATTR on_chunk_sent(')]
        code=r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#define IRAM_ATTR
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_SAFE(x) ((void)(x))
#define portEXIT_CRITICAL_SAFE(x) ((void)(x))
static int s_lock, s_chunks_out, resets;
static int64_t s_pending_since_us, now;
static int64_t esp_timer_get_time(void) { return now; }
static void lcd_timeout(uint32_t reason) { assert(reason==5); ++resets; }
FUNCTIONS
int main(void) {
    now=300000000; lcd_health_check(0); assert(!resets); /* sleeping for five minutes */
    s_pending_since_us=now; s_chunks_out=3;
    now+=100000; assert(!chunk_done()); lcd_health_check(0); assert(!resets);
    now+=100000; assert(!chunk_done());
    now+=100000; assert(chunk_done()); assert(!s_pending_since_us);
    now+=300000000; lcd_health_check(0); assert(!resets); /* sleep after completion */
    s_pending_since_us=now; s_chunks_out=2; assert(!chunk_done());
    now+=9999999; lcd_health_check(0); assert(!resets);
    now+=2; lcd_health_check(0); assert(resets==1); /* lost final interrupt */
}
'''.replace('FUNCTIONS',functions)
        with tempfile.TemporaryDirectory() as tmp:
            src=Path(tmp)/'health.c'; exe=Path(tmp)/'health.exe'; src.write_text(code,encoding='utf-8')
            subprocess.run([os.getenv('CC','cc'),'-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
