"""Deadlines use monotonic time, including pauses and long screen sleeps."""
from pathlib import Path
import os, subprocess, tempfile, unittest
ROOT = Path(__file__).resolve().parents[1]

class DeskTimerTest(unittest.TestCase):
    def test_deadlines_pauses_and_one_shot_alarm(self):
        harness = r'''
#include "muse_desk_timer.h"
#include <assert.h>
int main(void) {
    muse_desk_timer_t t;
    muse_desk_timer_reset(&t,25000000);
    assert(!t.running && muse_desk_timer_remaining(&t,0)==25000000);
    muse_desk_timer_toggle(&t,1000000);
    assert(t.running && muse_desk_timer_remaining(&t,6000000)==20000000);
    muse_desk_timer_toggle(&t,6000000);
    assert(!t.running && muse_desk_timer_remaining(&t,999000000)==20000000);
    muse_desk_timer_toggle(&t,999000000);
    assert(t.running && muse_desk_timer_remaining(&t,1000000000)==19000000);
    assert(muse_desk_timer_remaining(&t,1019000000)==0);
    assert(!t.running && muse_desk_timer_take_alarm(&t));
    assert(!muse_desk_timer_take_alarm(&t));
    muse_desk_timer_toggle(&t,1100000000);
    assert(!t.running);
    muse_desk_timer_reset(&t,5000000);
    muse_desk_timer_toggle(&t,1100000000);
    muse_desk_timer_reset(&t,60000000);
    assert(!t.running && !muse_desk_timer_take_alarm(&t));
    muse_desk_timer_toggle(&t,1);
    assert(muse_desk_timer_remaining(&t,9999999999LL)==0);
    assert(muse_desk_timer_take_alarm(&t));
    muse_desk_timer_reset(&t,-1);
    muse_desk_timer_toggle(&t,9999999999LL);
    assert(!t.running && !t.alarm && t.remaining_us==0);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            src=Path(temp)/'test.c'; exe=Path(temp)/'test.exe'
            src.write_text(harness)
            subprocess.run([os.environ.get('CC','cc'), '-std=c11', '-I', str(ROOT/'components/muse'), str(src), str(ROOT/'components/muse/muse_desk_timer.c'), '-o', str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__ == '__main__': unittest.main()
