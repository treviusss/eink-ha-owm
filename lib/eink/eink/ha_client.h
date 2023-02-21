#ifndef __EINK_HACLIENT_H__
#define __EINK_HACLIENT_H__

#include <WiFiClient.h>
#include <time.h>

#define EINK_HACLIENT_WAIT_TIMEOUT_MS 10000

namespace eink {

struct Weather {
  double temp;
  double wind_speed;
  int humidity;
  std::string condition;
  double temp_tomorrow;
  double wind_speed_tomorrow;
  std::string condition_tomorrow;
  time_t last_updated;
};

struct Temp {
  double temp;
  time_t last_updated;
};

struct Humidity {
  double humidity;
  time_t last_updated;
};
struct CO2 {
  int ppm;
  time_t last_updated;
};

struct HAData {
  int http_code;
  Weather weather;
  CO2 co2;
  Temp temp_livingroom_from_co2;
  Temp temp_livingroom;
  Temp temp_bedroom;
  Temp temp_arek;
  Temp temp_krzysiek;
  Temp temp_bathroom;
  Temp temp_zasilanie;
  Temp temp_house2_room;
  Temp temp_house2_bathroom;
  Temp temp_house2_kitchen;
  Temp temp_czerpnia;

  Humidity humidity_arek;
  Humidity humidity_krzysiek;
  Humidity humidity_livingroom;
  Humidity humidity_bedroom;
  Humidity humidity_bathroom;
  Humidity humidity_house2_room;
  Humidity humidity_house2_bathroom;
  Humidity humidity_house2_kitchen;
};

class HAClient {
 public:
  HAClient(const char *url, const char *token) : url_(url), token_(token) {}
  HAData FetchData();

 private:
  const std::string url_;
  const std::string token_;
};
}  // namespace eink
#endif  // __EINK_HACLIENT_H__