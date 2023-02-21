#ifndef WEATHER_H
#define WEATHER_H

#include <Arduino.h>

struct WeatherData {
    unsigned long timezone_offset;
    int http_code;
    unsigned long now_dt;
    double now_temp;
    double now_pop;
    double now_windspeed;
    String now_description;

    unsigned long now_plus_one_hour_dt;
    double now_plus_one_hour_temp;
    double now_plus_one_hour_pop;
    double now_plus_one_hour_windspeed;
    String now_plus_one_hour_description;

    unsigned long tomorrow_dt;
    double tomorrow_temp;
    double tomorrow_pop;
    double tomorrow_windspeed;
    String tomorrow_description;
};

struct ConvertedTimeStamp {
    String now_dt;
    String now_plus_one_hour_dt;
    String tomorrow_dt;
};

WeatherData get_weather();

#endif