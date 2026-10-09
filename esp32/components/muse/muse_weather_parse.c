#include "muse_weather.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
static bool number(cJSON *o, const char *key, bool daily, double *out)
{
    cJSON *value = cJSON_GetObjectItemCaseSensitive(o, key);
    if (daily) value = cJSON_GetArrayItem(value, 0);
    if (!cJSON_IsNumber(value) || !isfinite(value->valuedouble)) return false;
    *out = value->valuedouble;
    return true;
}
bool muse_weather_parse(const char *json, muse_weather_t *out)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    muse_weather_t w = {0}; double code;
    bool ok = number(current,"temperature_2m",false,&w.temperature)
        && number(current,"apparent_temperature",false,&w.apparent)
        && number(current,"relative_humidity_2m",false,&w.humidity)
        && number(current,"wind_speed_10m",false,&w.wind)
        && number(current,"weather_code",false,&code)
        && number(daily,"temperature_2m_max",true,&w.high)
        && number(daily,"temperature_2m_min",true,&w.low);
    const char *time = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(current,"time"));
    const char *sunrise = cJSON_GetStringValue(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(daily,"sunrise"),0));
    const char *sunset = cJSON_GetStringValue(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(daily,"sunset"),0));
    ok = ok && time && sunrise && sunset && strlen(time) >= 16 && strlen(sunrise) >= 16 && strlen(sunset) >= 16
        && w.humidity >= 0 && w.humidity <= 100 && w.wind >= 0;
    if (ok) {
        w.valid = true; w.code = (int)code;
        snprintf(w.time,sizeof(w.time),"%s",time);
        snprintf(w.sunrise,sizeof(w.sunrise),"%s",sunrise);
        snprintf(w.sunset,sizeof(w.sunset),"%s",sunset);
        *out = w;
    }
    cJSON_Delete(root);
    return ok;
}
const char *muse_weather_condition(int code)
{
    if (code == 0) return "晴";
    if (code == 1 || code == 2) return "晴间多云";
    if (code == 3) return "阴";
    if (code == 45 || code == 48) return "雾";
    if (code >= 51 && code <= 57) return "毛毛雨";
    if (code >= 61 && code <= 67) return "雨";
    if (code >= 71 && code <= 77) return "雪";
    if (code >= 80 && code <= 82) return "阵雨";
    if (code == 85 || code == 86) return "阵雪";
    if (code >= 95 && code <= 99) return "雷雨";
    return "未知天气";
}
