#include "eink/ha_client.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <StreamUtils.h>

#include <string>

#include "eink/logger.h"
#include "eink/time.h"

#define EINK_HA_CLIENT_HTTP_RESP_SIZE 110000

namespace eink {
namespace {

void ParseWeatherData(const JsonObject& entity, Weather& w) {
  const std::string name = entity["entity_id"].as<std::string>();
  // Temperature
  if (name == "sensor.openweathermap_temperature") {
    w.temp = entity["state"].as<double>();
    struct tm last_updated =
        ParseISODate(entity["last_updated"].as<const char*>());
    w.last_updated = ToEpoch(last_updated);
    LOG("Weather temp: %f\n", w.temp);

  // Condition
  } else if (name == "sensor.openweathermap_condition") {
    w.condition = entity["state"].as<std::string>();
    w.condition[0] = std::toupper(w.condition[0]);
    LOG("Weather condition: %s\n", w.condition.c_str());

  // Humidity
  } else if (name == "sensor.openweathermap_humidity") {
    w.humidity = entity["state"].as<int>();
    LOG("Weather humidity: %d\n", w.humidity);

  // Wind speed (kmh)
  } else if (name == "sensor.openweathermap_wind_speed") {
    w.wind_speed = entity["state"].as<double>();
    w.wind_speed = w.wind_speed * 3.6;
    LOG("Weather windspeed: %f\n", w.wind_speed);

  // Temperature tomorrow
  } else if (name == "sensor.openweathermap_forecast_temperature") {
    w.temp_tomorrow = entity["state"].as<double>();
    LOG("Weather temp tomorrow: %f\n", w.temp_tomorrow);

  // Condition tomorrow
  } else if (name == "sensor.openweathermap_forecast_condition") {
    w.condition_tomorrow = entity["state"].as<std::string>();
    w.condition_tomorrow[0] = std::toupper(w.condition_tomorrow[0]);
    LOG("Weather condition tomorrow: %s\n", w.condition_tomorrow.c_str());
    
  // Wind speed tomorrow (kmh)
  } else if (name == "sensor.openweathermap_forecast_wind_speed") {
    w.wind_speed_tomorrow = entity["state"].as<double>();
    w.wind_speed_tomorrow = w.wind_speed_tomorrow * 3.6;
    LOG("Weather windspeed: %f\n", w.wind_speed_tomorrow);
  }
}

void ParseCO2Data(const JsonObject& entity, CO2& res) {
  res.ppm = entity["state"].as<int>();
  struct tm last_updated =
      ParseISODate(entity["last_updated"].as<const char*>());
  res.last_updated = ToEpoch(last_updated);
  LOG("CO2: %d, updated %s ago\n", res.ppm, FormatTime(last_updated).c_str());
}

void ParseTempData(const JsonObject& entity, Temp& res) {
  res.temp = entity["state"].as<double>();
  struct tm last_updated =
      ParseISODate(entity["last_updated"].as<const char*>());
  res.last_updated = ToEpoch(last_updated);
  LOG("Temp: %f, updated %s ago\n", res.temp, FormatTime(last_updated).c_str());
}

void ParseHumidityData(const JsonObject& entity, Humidity& res) {
  res.humidity = entity["state"].as<double>();
  struct tm last_updated =
      ParseISODate(entity["last_updated"].as<const char*>());
  res.last_updated = ToEpoch(last_updated);
  LOG("Humidity: %f, updated %s ago\n", res.humidity, FormatTime(last_updated).c_str());
}

}  // namespace

HAData HAClient::FetchData() {
  HTTPClient http;
  http.useHTTP10(true);
  std::string url = url_ + "/states";
  http.begin(url.c_str());
  std::string header = std::string("Bearer ") + token_;
  http.addHeader("Authorization", header.c_str());
  int code = http.GET();
  
  LOG("HA client Fetch Data HTTP status code: %d\n", code);

  DynamicJsonDocument doc(EINK_HA_CLIENT_HTTP_RESP_SIZE);
  deserializeJson(doc, http.getStream());
  // serializeJsonPretty(doc, Serial);

  // For testing
  // ReadLoggingStream loggingStream(http.getStream(), Serial);
  // deserializeJson(doc, loggingStream);

  LOG("HA client Fetch Data JSON size: %d\n", doc.size());

  HAData data;
  data.http_code = code;

  
  for (const auto& el : doc.as<JsonArray>()) {
    std::string entity_id(el["entity_id"].as<std::string>());

    if (entity_id.find("sensor.openweathermap") !=
              entity_id.npos) {
      ParseWeatherData(el.as<JsonObject>(), data.weather);
      
    // Salon CO2
    } else if (entity_id == "sensor.mh_z19_co2_value") {
      ParseCO2Data(el.as<JsonObject>(), data.co2);
    } else if (entity_id == "sensor.mh_z19_temperature") {
      ParseTempData(el.as<JsonObject>(), data.temp_livingroom_from_co2);

    // Salon
    } else if (entity_id == "sensor.humidity_1") {
      ParseHumidityData(el.as<JsonObject>(), data.humidity_livingroom);
    } else if (entity_id == "sensor.temperature_1") {
      ParseTempData(el.as<JsonObject>(), data.temp_livingroom);

    // Rozdzielacz zasilanie
    } else if (entity_id == "sensor.temperature_5") { 
      ParseTempData(el.as<JsonObject>(), data.temp_zasilanie);

    // A
    } else if (entity_id == "sensor.arek_humidity") {
      ParseHumidityData(el.as<JsonObject>(), data.humidity_arek);
    } else if (entity_id == "sensor.arek_temperature") {
      ParseTempData(el.as<JsonObject>(), data.temp_arek);

    // K
    } else if (entity_id == "sensor.broken_screen_humidity") {
      ParseHumidityData(el.as<JsonObject>(), data.humidity_krzysiek);
    } else if (entity_id == "sensor.broken_screen_temperature") {
      ParseTempData(el.as<JsonObject>(), data.temp_krzysiek);

    // Bedroom
    } else if (entity_id == "sensor.bedroom_humidity") {
      ParseHumidityData(el.as<JsonObject>(), data.humidity_bedroom);
    } else if (entity_id == "sensor.bedroom_temperature") {
      ParseTempData(el.as<JsonObject>(), data.temp_bedroom);

    // Bathroom
    } else if (entity_id == "sensor.bme280_2_humidity") {
      ParseHumidityData(el.as<JsonObject>(), data.humidity_bathroom);
    } else if (entity_id == "sensor.bme280_2_temperature") {
      ParseTempData(el.as<JsonObject>(), data.temp_bathroom);

    // Czerpnia
    } else if (entity_id == "sensor.reku_bmetemp") { 
      ParseTempData(el.as<JsonObject>(), data.temp_czerpnia);


    // Dom 2 ROOM
    } else if (entity_id == "sensor.ble_humidity_room") {
      ParseHumidityData(el.as<JsonObject>(), data.humidity_house2_room);
    } else if (entity_id == "sensor.ble_temperature_room") {
      ParseTempData(el.as<JsonObject>(), data.temp_house2_room);

    // Dom 2 BATHROOM
    } else if (entity_id == "sensor.bathroom_humidity"){
      ParseHumidityData(el.as<JsonObject>(), data.humidity_house2_bathroom);
    } else if (entity_id == "sensor.bathroom_temperature") {
      ParseTempData(el.as<JsonObject>(), data.temp_house2_bathroom);

    // Dom 2 KITCHEN
    } else if (entity_id == "sensor.ble_humidity_kitchen"){
      ParseHumidityData(el.as<JsonObject>(), data.humidity_house2_kitchen);
    } else if (entity_id == "sensor.ble_temperature_kitchen") {
      ParseTempData(el.as<JsonObject>(), data.temp_house2_kitchen);
    }
}
return data;
}

}  // namespace eink