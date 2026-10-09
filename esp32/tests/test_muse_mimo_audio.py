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


if __name__ == '__main__':
    unittest.main()
