import os, subprocess, tempfile, unittest
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
class MessagePolicy(unittest.TestCase):
    def test_beijing_boundaries_and_ownership(self):
        with tempfile.TemporaryDirectory() as tmp:
            src=Path(tmp)/"policy.c"; exe=Path(tmp)/"policy.exe"
            src.write_text(r"""
#include <assert.h>
#include "muse_message_policy.h"
int main(void) {
    /* 2024-01-01 00:00 UTC = Beijing 08:00. */
    long long base=1704067200LL;
    assert(muse_push_quiet(true, base-7201)); /* 05:59:59 */
    assert(!muse_push_quiet(true, base-7200)); /* 06:00 */
    assert(!muse_push_quiet(true, base+53999)); /* 22:59:59 */
    assert(muse_push_quiet(true, base+54000)); /* 23:00 */
    assert(muse_push_quiet(true, base+57600)); /* midnight */
    assert(muse_push_quiet(true, 0));
    assert(!muse_push_quiet(false, 0));
    assert(!muse_push_allowed(true,true,false,base));
    assert(!muse_push_allowed(false,false,false,base));
    assert(muse_push_allowed(true,false,true,base));
    assert(!muse_push_allowed(true,false,true,base+54000));
    assert(muse_push_allowed(true,false,false,base+54000));
    assert(muse_reply_allowed(true,false,true,false,false));
    assert(!muse_reply_allowed(true,false,false,true,true));
    assert(!muse_reply_allowed(true,false,true,true,false));
    assert(muse_reply_allowed(true,false,true,true,true));
    assert(muse_reply_allowed(true,true,true,false,false));
}
""",encoding="utf-8")
            subprocess.run([os.getenv("CC","cc"),"-Wall","-Wextra","-Werror","-I",str(ROOT/"components/muse"),str(src),"-o",str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe)],check=True)
