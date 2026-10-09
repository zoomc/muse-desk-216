"""MiMo PCM resampling must keep its phase across streamed audio chunks."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MiMoAudioTest(unittest.TestCase):
    def test_streamed_resampling_matches_contiguous_audio(self):
        source = (ROOT / 'components/muse/muse_chat_session.cpp').read_text(encoding='utf-8')
        start = source.index('static void resampler_init(')
        end = source.index('/* ---- Turn: dictation', start)
        functions = source[start:end]
        harness = r'''
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cassert>
struct resampler_t { uint32_t step, pos; int16_t prev; };
FUNCTIONS
int main() {
    std::vector<int16_t> input(24000), whole(16010), streamed(16010);
    for (size_t i=0; i<input.size(); ++i) input[i]=int16_t((i*701)%65536-32768);
    resampler_t a,b;
    resampler_init(&a,24000,16000); resampler_init(&b,24000,16000);
    size_t expected=resample(&a,input.data(),input.size(),whole.data());
    assert(expected==16000);
    size_t offset=0, count=0;
    const size_t chunks[]={1,3,17,1024,7,512};
    for (size_t index=0; offset<input.size(); ++index) {
        size_t n=chunks[index%6];
        if(n>input.size()-offset) n=input.size()-offset;
        count+=resample(&b,input.data()+offset,n,streamed.data()+count);
        offset+=n;
    }
    assert(count==expected);
    for(size_t i=0;i<count;++i) assert(whole[i]==streamed[i]);
    assert(resample(&b,input.data(),0,streamed.data())==0);
}
'''.replace('FUNCTIONS', functions)
        with tempfile.TemporaryDirectory() as temp:
            cpp=Path(temp)/'test.cpp'
            exe=Path(temp)/'test.exe'
            cpp.write_text(harness,encoding='utf-8')
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++17',str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)


    def test_stopped_consumer_times_out_and_partial_writes_progress(self):
        source = (ROOT / 'components/muse/muse_tts.cpp').read_text(encoding='utf-8')
        start = source.index('static bool audio_line(')
        end = source.index('static void worker(', start)
        functions = source[start:end]
        harness = r'''
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cassert>
#include "cJSON.h"
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
static int64_t clock_us;
static int sends, mode;
struct muse_tts_request_t { bool cancelled; void *bytes; };
static int64_t esp_timer_get_time() { return clock_us; }
static size_t xStreamBufferSend(void *, const void *, size_t n, int) {
    ++sends; clock_us += 100000;
    return mode==1 ? (n>2 ? 2 : n) : 0;
}
static int mbedtls_base64_decode(unsigned char *out, size_t, size_t *n, const unsigned char *, size_t) {
    *n=8; memset(out,0,8); return 0;
}
FUNCTIONS
int main() {
    muse_tts_request_t r{};
    const char *json="data: {\"choices\":[{\"delta\":{\"audio\":{\"data\":\"AAAAAAAAAAAA\"}}}]}";
    char line[256]; size_t total=0; bool done=false;
    strcpy(line,json); mode=0;
    assert(!audio_line(&r,line,total,done));
    assert(total==0 && clock_us>15000000 && sends<=152);
    mode=1; sends=0; total=0; strcpy(line,json);
    assert(audio_line(&r,line,total,done)); assert(total==8 && sends==4);
    r.cancelled=true; sends=0; total=0; strcpy(line,json);
    assert(audio_line(&r,line,total,done)); assert(total==0 && sends==0);
    strcpy(line,"data: [DONE]"); assert(audio_line(&r,line,total,done) && done);
}
'''.replace('FUNCTIONS',functions)
        with tempfile.TemporaryDirectory() as tmp:
            cpp=Path(tmp)/'stall.cpp'; obj=Path(tmp)/'json.o'; exe=Path(tmp)/'stall.exe'
            cpp.write_text(harness,encoding='utf-8')
            jsondir=ROOT/'managed_components/espressif__cjson/cJSON'
            subprocess.run([os.getenv('CC','cc'),'-I',str(jsondir),'-c',str(jsondir/'cJSON.c'),'-o',str(obj)],check=True)
            subprocess.run([os.getenv('CXX','c++'),'-std=c++17','-Wall','-Wextra','-Werror','-I',str(jsondir),str(cpp),str(obj),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)


if __name__ == '__main__':
    unittest.main()
