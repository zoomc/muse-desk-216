"""Weather parser rejects missing/null values rather than showing invented zeroes."""
from pathlib import Path
import json, os, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[1]
class WeatherTest(unittest.TestCase):
    def test_response_validation_and_cache_preservation(self):
        good={'current':{'time':'2026-10-09T16:30','temperature_2m':18.7,'apparent_temperature':17.3,'relative_humidity_2m':67,'wind_speed_10m':3.8,'weather_code':2},'daily':{'temperature_2m_max':[22.1],'temperature_2m_min':[13.4],'sunrise':['2026-10-09T05:58'],'sunset':['2026-10-09T17:26']}}
        fixtures=[good,{}, {'error':True},json.loads(json.dumps(good)),json.loads(json.dumps(good))]
        fixtures[3]['current']['temperature_2m']=None
        fixtures[4]['daily']['sunrise']=[]
        strings='\n'.join(f'const char *f{i}={json.dumps(json.dumps(f),ensure_ascii=True)};' for i,f in enumerate(fixtures))
        harness='''#include "muse_weather.h"
#include <assert.h>
#include <string.h>
FIXTURES
int main(void) {
 muse_weather_t w={0}; assert(muse_weather_parse(f0,&w));
 assert(w.valid && w.temperature>18.6 && w.temperature<18.8 && w.code==2);
 assert(!strcmp(w.time,"2026-10-09T16:30"));
 const char *bad[]={f1,f2,f3,f4,"{", "null"};
 for(unsigned i=0;i<6;i++) { assert(!muse_weather_parse(bad[i],&w)); assert(w.valid && w.temperature>18.6); }
 assert(!strcmp(muse_weather_condition(2),"晴间多云"));
 assert(!strcmp(muse_weather_condition(999),"未知天气"));
}
'''.replace('FIXTURES',strings)
        with tempfile.TemporaryDirectory() as temp:
            src=Path(temp)/'test.c'; exe=Path(temp)/'test.exe'; src.write_text(harness,encoding='utf-8')
            cjson=ROOT/'managed_components/espressif__cjson/cJSON'
            subprocess.run([os.environ.get('CC','cc'),'-std=c11','-I',str(ROOT/'components/muse'),'-I',str(cjson),str(src),str(ROOT/'components/muse/muse_weather_parse.c'),str(cjson/'cJSON.c'),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)
if __name__ == '__main__': unittest.main()
