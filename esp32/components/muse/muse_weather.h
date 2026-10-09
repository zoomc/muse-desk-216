#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool valid;
    bool failed;
    double temperature, apparent, humidity, wind, high, low;
    int code;
    int64_t fetched_us;
    char time[24];
    char sunrise[24], sunset[24];
} muse_weather_t;
bool muse_weather_parse(const char *json, muse_weather_t *out);
const char *muse_weather_condition(int code);
void muse_weather_start(void);
void muse_weather_refresh(void);
void muse_weather_get(muse_weather_t *out);
