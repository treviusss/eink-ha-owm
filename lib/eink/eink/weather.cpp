#include <WiFi.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <StreamUtils.h>
#include "eink/weather.h"
#include "credentials.h"

WeatherData get_weather() {
    String openweather_url = kOpenWeatherMapAPIUrl;
    WiFiClient client;
    HTTPClient http;
    WeatherData w_data;
    http.useHTTP10(true);
    http.begin(client, openweather_url);
    int httpCode = http.GET();
    w_data.http_code = httpCode;
    Serial.printf("Openweather API [HTTP] GET... code: %d\n", httpCode);

    if (httpCode == HTTP_CODE_OK) {
      
      DynamicJsonDocument doc(4096);
      StaticJsonDocument<200> filter;

      filter["timezone_offset"] = true;
      filter["hourly"][0]["dt"] = true;
      filter["hourly"][0]["temp"] = true;  
      filter["hourly"][0]["pop"] = true;
      filter["hourly"][0]["wind_speed"] = true;
      filter["hourly"][0]["weather"][0]["main"] = true; 

      
      // // For testing
      // ReadLoggingStream loggingStream(http.getStream(), Serial);
      // deserializeJson(doc, loggingStream, DeserializationOption::Filter(filter));

      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
      // serializeJsonPretty(doc, Serial);

      w_data.timezone_offset = doc["timezone_offset"].as<unsigned long>();

      w_data.now_dt = doc["hourly"][0]["dt"].as<unsigned long>();
      w_data.now_temp = doc["hourly"][0]["temp"].as<double>();
      w_data.now_pop = doc["hourly"][0]["pop"].as<double>();
      w_data.now_windspeed = doc["hourly"][0]["wind_speed"].as<double>();
      w_data.now_description = doc["hourly"][0]["weather"][0]["main"].as<String>();

      w_data.now_plus_one_hour_dt = doc["hourly"][1]["dt"].as<unsigned long>();
      w_data.now_plus_one_hour_temp = doc["hourly"][1]["temp"].as<double>();
      w_data.now_plus_one_hour_pop = doc["hourly"][1]["pop"].as<double>();
      w_data.now_plus_one_hour_windspeed = doc["hourly"][1]["wind_speed"].as<double>();
      w_data.now_plus_one_hour_description = doc["hourly"][1]["weather"][0]["main"].as<String>();

      w_data.tomorrow_dt = doc["hourly"][24]["dt"].as<unsigned long>();
      w_data.tomorrow_temp = doc["hourly"][24]["temp"].as<double>();
      w_data.tomorrow_pop = doc["hourly"][24]["pop"].as<double>();
      w_data.tomorrow_windspeed = doc["hourly"][24]["wind_speed"].as<double>();
      w_data.tomorrow_description = doc["hourly"][24]["weather"][0]["main"].as<String>();

      Serial.printf("INSIDE GET WEATHER NOW: \n%lu\n%f\n%f\n", w_data.now_dt, w_data.now_temp, w_data.now_windspeed);
      Serial.printf("INSIDE GET WEATHER NOW PLUS 1h: \n%lu\n%f\n%f\n", w_data.now_plus_one_hour_dt, w_data.now_plus_one_hour_temp, w_data.now_plus_one_hour_windspeed);
      Serial.printf("INSIDE GET WEATHER TOMORROW: \n%lu\n%f\n%f\n", w_data.tomorrow_dt, w_data.tomorrow_temp, w_data.tomorrow_windspeed);

    }
    return w_data;
};