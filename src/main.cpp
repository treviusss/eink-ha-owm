#include <Arduino.h>
#include <HTTPClient.h>
#include <driver/adc.h>
#include <time.h>
#include <TimeLib.h>

#include <sstream>

#include "credentials.h"
#include "eink/weather.h"
#include "eink/display.h"
#include "eink/ha_client.h"
#include "eink/logger.h"
#include "eink/time.h"
#include "eink/wifi.h"
#include "epd_driver.h"
#include "esp_adc_cal.h"
#include "esp_sleep.h"
#include "esp_wifi.h"

#define BATT_PIN 36

constexpr int kPadding = 10;
enum EpdRotation orientation = epd_get_rotation();
int vref = 1100;

int rssi_int = 0;

RTC_DATA_ATTR int runs;

struct batt_t {
  double v;
  double percent;
};

batt_t get_battery_percentage() {
  // When reading the battery voltage, POWER_EN must be turned on
  epd_poweron();
  delay(50);

  uint16_t v = analogRead(BATT_PIN);
  Serial.printf("Read from Battery Pin: %u\n", v);
  epd_poweroff();

  double_t battery_voltage =
      ((double_t)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);

  Serial.printf("Battery voltage: %f\n", battery_voltage);

  double_t percent_experiment = ((battery_voltage - 3.7) / 0.5) * 100;

  // cap out battery at 100%
  // on charging it spikes higher
  if (percent_experiment > 100) {
    percent_experiment = 100;
  }

  batt_t res;
  res.v = battery_voltage;
  res.percent = percent_experiment;
  return res;
}

void start_deep_sleep(struct tm *now) {
  // Default 1 hour = 60 min = 60 * 60 seconds.
  time_t sleep_for_s = 20 * 60;

  // If it's past midnight and before 5 am, sleep until 5am.
  if (now != nullptr && now->tm_hour < 5) {
    struct tm time_5am = *now;
    time_5am.tm_hour = 5;
    sleep_for_s = eink::ToEpoch(time_5am) - eink::ToEpoch(*now);
  }

  esp_sleep_enable_timer_wakeup(sleep_for_s * 1e6);
  esp_deep_sleep_start();
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

template <typename T>
std::string to_fixed_str(const T value, const int n = 0) {
  std::ostringstream out;
  out.precision(n);
  out << std::fixed << value;
  return out.str();
}

void correct_adc_reference() {
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
      ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV\n", adc_chars.vref);
    vref = adc_chars.vref;
  }
}

void convert_timestamp_to_datetime(WeatherData &w, ConvertedTimeStamp &timestamp) {
  int timezone_offset = w.timezone_offset;

  char buffer1 [50];
  char buffer2 [50];
  char buffer3 [50];
  unsigned long date_now = w.now_dt + timezone_offset;
  unsigned long date_now_plus_one_hour = w.now_plus_one_hour_dt + timezone_offset;
  unsigned long date_tomorrow = w.tomorrow_dt + timezone_offset;
  
  sprintf(buffer1, "%4d-%02d-%02d %02d:%02d\n", year(date_now), month(date_now), day(date_now), hour(date_now), minute(date_now));
  sprintf(buffer2, "%4d-%02d-%02d %02d:%02d\n", year(date_now_plus_one_hour), month(date_now_plus_one_hour), day(date_now_plus_one_hour), hour(date_now_plus_one_hour), minute(date_now_plus_one_hour));
  sprintf(buffer3, "%4d-%02d-%02d %02d:%02d\n", year(date_tomorrow), month(date_tomorrow), day(date_tomorrow), hour(date_tomorrow), minute(date_tomorrow));
  
  timestamp.now_dt = buffer1;
  timestamp.now_plus_one_hour_dt = buffer2;
  timestamp.tomorrow_dt = buffer3;

  // Serial.println(timestamp.now_dt);
  // Serial.println(timestamp.tomorrow_dt);

}

void DrawRSSI(eink::Display &display, int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 30; //            <-20dbm displays 5-bars
    if (_rssi <= -40)  WIFIsignal = 24; //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60)  WIFIsignal = 18; //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80)  WIFIsignal = 12; //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 6;  // -100dbm to  -81dbm displays 1-bar
    display.DrawRect(y - WIFIsignal, x + xpos * 8, WIFIsignal, 7, 0, true);
    xpos++;
  }
}

int DrawHeader_Vertical(eink::Display &display, const struct tm &now, int y) {
  int text_size = display.FontHeight(eink::FontSize::Size12);

  std::string tstring = eink::FormatTime(now);
  y += text_size;
  display.DrawText(y, kPadding, tstring.c_str(), 0, eink::FontSize::Size12,
                   eink::DrawTextDirection::LTR);

  batt_t batt = get_battery_percentage();
  std::string batt_str = to_fixed_str(batt.v, 2) + " V";

  display.DrawText(y, EPD_HEIGHT - kPadding, batt_str.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);
  return y;
}


int DrawHeader_Horizontal(eink::Display &display, const struct tm &now, int y, time_t runtime_ms) {
  int text_size = display.FontHeight(eink::FontSize::Size12);

  batt_t batt = get_battery_percentage();
  std::string batt_str = to_fixed_str(batt.v, 2) + " V";
  std::string rssi_string = to_fixed_str(rssi_int, 1) + " dBm";
  std::string txt = "Run #" + to_fixed_str(runs) + " took " +
                    to_fixed_str(runtime_ms / 1000.0, 1) + " s";
  std::string tstring = eink::FormatTime(now);

  y += text_size;

  // Battery voltage
  display.DrawText(y, kPadding, batt_str.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::LTR);

  // WiFi RSSI
  display.DrawText(y, 120, rssi_string.c_str(), 0,
                  eink::FontSize::Size12, eink::DrawTextDirection::LTR);

  // WiFi bars
  DrawRSSI(display, 220, y, rssi_int);
  
  // Time of update
  display.DrawText(y, (EPD_WIDTH / 3) + 60, tstring.c_str(), 0, eink::FontSize::Size12,
                   eink::DrawTextDirection::LTR);

  // Number of runs
  display.DrawText(y, EPD_WIDTH - kPadding, txt.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);

  return y;
}

void DrawFooter_Vertical(eink::Display &display, time_t runtime_ms) {
  std::string txt = "Run #" + to_fixed_str(runs) + " took " +
                    to_fixed_str(runtime_ms / 1000.0, 1) + " s";
  display.DrawText(EPD_WIDTH - kPadding, EPD_HEIGHT - kPadding, txt.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);
}

int DrawWeather_Vertical(eink::Display &display, const eink::Weather &w, time_t now,
                int y) {
  int header_size = display.FontHeight(eink::FontSize::Size16);
  int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);

  display.DrawText(y + header_size + kPadding, kPadding, "Pogoda", 0,
                   eink::FontSize::Size16b);
  y += header_size + kPadding;

  int weather_y = y + kPadding + weather_size - kPadding;
  std::string val = to_fixed_str(w.temp, 1) + " °C";
  display.DrawText(weather_y, kPadding, val.c_str(), 0, eink::FontSize::Size24);

  int state_y = y + kPadding + text_size;
  display.DrawText(state_y, EINK_DISPLAY_HEIGHT - kPadding, w.condition.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);
  display.DrawText(state_y + kPadding + text_size,
                   EINK_DISPLAY_HEIGHT - kPadding,
                   (eink::ToHumanDiff(now - w.last_updated) + " ago").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::RTL);
  return weather_y;
}

int DrawWeather_Horizontal(eink::Display &display, const eink::Weather &w, time_t now,
                int y) {
  int header_size = display.FontHeight(eink::FontSize::Size16);
  int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);

  // WEATHER LEFT SIDE
  display.DrawText(y + header_size + kPadding, kPadding, "POGODA DZISIAJ", 0,
                   eink::FontSize::Size16b);
  display.DrawText(y + header_size + kPadding, (EPD_WIDTH / 2) - kPadding, w.condition.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);

  // WEATHER RIGHT SIDE
  display.DrawText(y + header_size + kPadding, (EPD_WIDTH / 2) + kPadding, "POGODA JUTRO", 0,
                   eink::FontSize::Size16b);
  display.DrawText(y + header_size + kPadding, EPD_WIDTH - kPadding, w.condition_tomorrow.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);

  y += header_size + kPadding;

  int weather_y = y + kPadding + weather_size - kPadding;

  std::string temp_val = to_fixed_str(w.temp, 1) + " °C";
  std::string humidity_val = to_fixed_str(w.humidity, 0) + "%";
  std::string windspeed_val = to_fixed_str(w.wind_speed, 1) + "km/h";

  std::string temp_tomorrow_val = to_fixed_str(w.temp_tomorrow, 1) + " °C";
  std::string windspeed_tomorrow_val = to_fixed_str(w.wind_speed_tomorrow, 1) + "km/h";
  
  // TABLE
  display.DrawRect(text_size + kPadding, 0, weather_y - kPadding, EPD_WIDTH / 2, 0, false);
  display.DrawRect(text_size + kPadding, EPD_WIDTH / 2, weather_y - kPadding, EPD_WIDTH / 2, 0, false);

  // LEFT SIDE VALUES
  display.DrawText(weather_y, kPadding, temp_val.c_str(), 0, eink::FontSize::Size24);
  display.DrawText(weather_y, (EPD_WIDTH / 4) - 3 * kPadding, humidity_val.c_str(), 0, eink::FontSize::Size16);
  display.DrawText(weather_y, (EPD_WIDTH / 2) - kPadding, windspeed_val.c_str(), 0, eink::FontSize::Size16, eink::DrawTextDirection::RTL);

  // RIGHT SIDE VALUES
  display.DrawText(weather_y, (EPD_WIDTH / 2) + kPadding , temp_tomorrow_val.c_str(), 0, eink::FontSize::Size24);
  display.DrawText(weather_y, EPD_WIDTH - kPadding, windspeed_tomorrow_val.c_str(), 0, eink::FontSize::Size16, eink::DrawTextDirection::RTL);

  return weather_y;
}


int DrawOpenweather_Horizontal(eink::Display &display, const WeatherData &w, ConvertedTimeStamp &date, time_t now,
                int y) {
  int header_size = display.FontHeight(eink::FontSize::Size16);
  // int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);

  double now_temp_celsius = w.now_temp - 273.15;
  double now_precipitation_percent = w.now_pop * 100;
  double now_windspeed_kmh = w.now_windspeed * 3.6;

  double now_plus_one_hour_temp_celsius = w.now_plus_one_hour_temp - 273.15;
  double now_plus_one_hour_precipitation_percent = w.now_plus_one_hour_pop * 100;
  double now_plus_one_hour_windspeed_kmh = w.now_plus_one_hour_windspeed * 3.6;

  double tomorrow_temp_celsius = w.tomorrow_temp - 273.15;
  double tomorrow_precipitation_percent = w.tomorrow_pop * 100;
  double tomorrow_windspeed_kmh = w.tomorrow_windspeed * 3.6;

  std::string now_temp_str = to_fixed_str(now_temp_celsius, 1) + "°C";
  std::string now_plus_one_hour_temp_str = to_fixed_str(now_plus_one_hour_temp_celsius, 1) + "°C";
  std::string tomorrow_temp_str = to_fixed_str(tomorrow_temp_celsius, 1) + "°C";
  
  std::string now_pop_str = to_fixed_str(now_precipitation_percent) + "%";
  std::string now_plus_one_hour_pop_str = to_fixed_str(now_plus_one_hour_precipitation_percent) + "%";
  std::string tomorrow_pop_str = to_fixed_str(tomorrow_precipitation_percent) + "%";

  std::string now_windspeed_str = to_fixed_str(now_windspeed_kmh) + "kmh";
  std::string now_plus_one_hour_windspeed_str = to_fixed_str(now_plus_one_hour_windspeed_kmh) + "kmh";
  std::string tomorrow_windspeed_str = to_fixed_str(tomorrow_windspeed_kmh) + "kmh";


  // TABLE
  display.DrawRect(y + kPadding, 0, text_size + header_size + kPadding, EPD_WIDTH / 3, 0, false);
  display.DrawRect(y + kPadding, EPD_WIDTH / 3, text_size + header_size + kPadding, EPD_WIDTH / 3, 0, false);
  display.DrawRect(y + kPadding, (2 * EPD_WIDTH) / 3, text_size + header_size + kPadding, EPD_WIDTH / 3, 0, false);

  y += text_size + kPadding;

  // FIRST ROW

  // LEFT SIDE
  display.DrawText(y, kPadding, date.now_dt.c_str(), 0,
                   eink::FontSize::Size12);
  display.DrawText(y, (EPD_WIDTH / 3) - kPadding, w.now_description.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);

  // MIDDLE SIDE
  display.DrawText(y, (EPD_WIDTH / 3) + kPadding, date.now_plus_one_hour_dt.c_str(), 0,
                   eink::FontSize::Size12);
  display.DrawText(y, (2 * EPD_WIDTH) / 3 - kPadding, w.now_plus_one_hour_description.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);

  // RIGHT SIDE
  display.DrawText(y, (2 * EPD_WIDTH) / 3 + kPadding, date.tomorrow_dt.c_str(), 0,
                   eink::FontSize::Size12);
  display.DrawText(y, EPD_WIDTH - kPadding, w.tomorrow_description.c_str(), 0,
                   eink::FontSize::Size12, eink::DrawTextDirection::RTL);

  y += text_size + kPadding;

  // SECOND ROW

  // LEFT SIDE 
  display.DrawText(y, kPadding, now_temp_str.c_str(), 0, eink::FontSize::Size16b);
  display.DrawText(y, 130, now_pop_str.c_str(), 0, eink::FontSize::Size16);
  display.DrawText(y, (EPD_WIDTH / 3) - kPadding, now_windspeed_str.c_str(), 0, eink::FontSize::Size16, eink::DrawTextDirection::RTL);

  // MIDDLE SIDE 
  display.DrawText(y, (EPD_WIDTH / 3) + kPadding , now_plus_one_hour_temp_str.c_str(), 0, eink::FontSize::Size16b);
  display.DrawText(y, (EPD_WIDTH / 3) + 130, now_plus_one_hour_pop_str.c_str(), 0, eink::FontSize::Size16);
  display.DrawText(y, ((2 * EPD_WIDTH) / 3) - kPadding, now_plus_one_hour_windspeed_str.c_str(), 0, eink::FontSize::Size16, eink::DrawTextDirection::RTL);

  // RIGHT SIDE 
  display.DrawText(y, ((2 * EPD_WIDTH) / 3) + kPadding , tomorrow_temp_str.c_str(), 0, eink::FontSize::Size16b);
  display.DrawText(y, ((2 * EPD_WIDTH) / 3) + 130, tomorrow_pop_str.c_str(), 0, eink::FontSize::Size16);
  display.DrawText(y, EPD_WIDTH - kPadding, tomorrow_windspeed_str.c_str(), 0, eink::FontSize::Size16, eink::DrawTextDirection::RTL);
  return y;
}


int DrawTempCO2_Vertical(eink::Display &display, const eink::Temp &t, const eink::CO2 &w,
                const char* text, time_t now, int y) {
  int header_size = display.FontHeight(eink::FontSize::Size16);
  int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);

  display.DrawText(y + header_size + kPadding, kPadding, text, 0,
                   eink::FontSize::Size16b);
  y += weather_size + 3 * kPadding;

  std::string val = to_fixed_str(t.temp, 1) + " °C";
  display.DrawText(y, kPadding, val.c_str(), 0, eink::FontSize::Size16);

  std::string co2_val = to_fixed_str(w.ppm, 0) + " ppm";
  display.DrawText(y, EINK_DISPLAY_HEIGHT / 2, co2_val.c_str(), 0,
                   eink::FontSize::Size16);

  y += text_size;
  display.DrawText(y, kPadding,
                   (eink::ToHumanDiff(now - t.last_updated) + " ago").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);
  display.DrawText(y, EINK_DISPLAY_HEIGHT / 2,
                   (eink::ToHumanDiff(now - w.last_updated) + " ago").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);
  return y;
}

int DrawTempCO2_Horizontal(eink::Display &display, const eink::Temp &t, const eink::CO2 &w, const char* text, time_t now, int x, int y) {
  int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);
  int x_2_column = x + (EPD_WIDTH / 3);

  std::string temp_val = to_fixed_str(t.temp, 1) + "°C";
  std::string co2_val = to_fixed_str(w.ppm, 0) + "ppm";

  // FIRST ROW
  display.DrawText(y, x, text, 0, eink::FontSize::Size16b);

  y += text_size + kPadding;

  // // SECOND ROW
  // display.DrawText(y, x, temp_val.c_str(), 0, eink::FontSize::Size16b);
  // display.DrawText(y, x_2_column - 2 * kPadding, co2_val.c_str(), 0, eink::FontSize::Size16b, eink::DrawTextDirection::RTL);
  display.DrawText(y, x, co2_val.c_str(), 0, eink::FontSize::Size16b);

  y += text_size;

  // THIRD ROW
  // display.DrawText(y, x,
  //                  (eink::ToHumanDiff(now - t.last_updated) + " temu").c_str(),
  //                  0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);
  // display.DrawText(y, x_2_column - 2 * kPadding,
  //                  (eink::ToHumanDiff(now - w.last_updated) + " temu").c_str(),
  //                  0, eink::FontSize::Size12, eink::DrawTextDirection::RTL);
  display.DrawText(y, x,
                   (eink::ToHumanDiff(now - w.last_updated) + " temu").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);

  y += weather_size;

  return y;
}


int DrawTempHumidity_Vertical(eink::Display &display, const eink::Temp &t, const eink::Humidity &h, const char* text, time_t now, int y) {
  int header_size = display.FontHeight(eink::FontSize::Size16);
  int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);

  display.DrawText(y + header_size + kPadding, kPadding, text, 0,
                   eink::FontSize::Size16b);
  y += weather_size + 3 * kPadding;

  std::string val = to_fixed_str(t.temp, 1) + " °C";
  display.DrawText(y, kPadding, val.c_str(), 0, eink::FontSize::Size16);

  std::string co2_val = to_fixed_str(h.humidity, 0) + " %";
  display.DrawText(y, EINK_DISPLAY_HEIGHT / 2, co2_val.c_str(), 0,
                   eink::FontSize::Size16);

  y += text_size;
  display.DrawText(y, kPadding,
                   (eink::ToHumanDiff(now - t.last_updated) + " ago").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);
  display.DrawText(y, EINK_DISPLAY_HEIGHT / 2,
                   (eink::ToHumanDiff(now - h.last_updated) + " ago").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);
  return y;
}

int DrawTempHumidity_Horizontal(eink::Display &display, const eink::Temp &t, const eink::Humidity &h, const char* text, time_t now, int x, int y) {
  int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);
  int x_2_column = x + (EPD_WIDTH / 3);

  std::string temp_val = to_fixed_str(t.temp, 1) + "°C";
  std::string humidity_val = to_fixed_str(h.humidity, 0) + "%";

  // FIRST ROW
  display.DrawText(y, x, text, 0, eink::FontSize::Size16b);

  y += text_size + kPadding;

  // SECOND ROW
  display.DrawText(y, x, temp_val.c_str(), 0, eink::FontSize::Size16b);
  display.DrawText(y, x_2_column - 2 * kPadding, humidity_val.c_str(), 0, eink::FontSize::Size16b, eink::DrawTextDirection::RTL);

  y += text_size;

  // THIRD ROW
  display.DrawText(y, x,
                   (eink::ToHumanDiff(now - t.last_updated) + " temu").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);
  display.DrawText(y, x_2_column - 2 * kPadding,
                   (eink::ToHumanDiff(now - h.last_updated) + " temu").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::RTL);

  y += weather_size;

  return y;
}

int DrawTemp_Horizontal(eink::Display &display, const eink::Temp &t, const char* text, time_t now, int x, int y) {
  int weather_size = display.FontHeight(eink::FontSize::Size24);
  int text_size = display.FontHeight(eink::FontSize::Size12);
  int x_2_column = x + (EPD_WIDTH / 3);

  std::string temp_val = to_fixed_str(t.temp, 1) + "°C";
  
  // FIRST ROW
  display.DrawText(y, x, text, 0, eink::FontSize::Size16b);

  y += text_size + kPadding;

  // SECOND ROW
  display.DrawText(y, x, temp_val.c_str(), 0, eink::FontSize::Size16b);

  y += text_size;

  // THIRD ROW
  display.DrawText(y, x,
                   (eink::ToHumanDiff(now - t.last_updated) + " temu").c_str(),
                   0, eink::FontSize::Size12, eink::DrawTextDirection::LTR);
  y += weather_size;

  return y;
}

void DrawGrid(eink::Display &display, int y) {

  // VERTICAL LINES
  display.drawLine(EPD_WIDTH / 3, y, EPD_WIDTH / 3, EPD_HEIGHT, 0);
  display.drawLine(2 * EPD_WIDTH / 3, y, 2 * EPD_WIDTH / 3, EPD_HEIGHT, 0);
  // HORIZONTAL LINES - Y values taken by trial and error
  display.drawLine(0, 249, EPD_WIDTH, 249, 0);
  display.drawLine(0, 387, EPD_WIDTH, 387, 0);

}


void setup() {
  time_t t0 = millis();
  auto &logger = eink::Logger::Get();

  adc_power_acquire();

  correct_adc_reference();

  print_wakeup_reason();

  Serial.printf("Before wifi %ld\n", millis() - t0);

  if (eink::WiFiBegin(kWiFiSSID, kWiFiPass) != 0) {
    logger.Printf("Unable to connect to WiFi. Sleeping.\n");
    // display.DrawText(EPD_HEIGHT / 2, EPD_WIDTH / 2, "Unable to connect to WiFi", 0, eink::FontSize::Size24);
    start_deep_sleep(nullptr);
  }
  Serial.printf("After wifi %ld\n", millis() - t0);

  rssi_int = eink::GetWiFiRSSI();
  eink::ConfigNTP();

  WeatherData weather_data = get_weather();
  ConvertedTimeStamp timestamp;
  convert_timestamp_to_datetime(weather_data, timestamp);

  eink::HAClient hacli(kHomeAssistantAPIUrl, kHomeAssistantToken);
  eink::HAData data = hacli.FetchData();
  
  Serial.printf("After getting data %ld\n", millis() - t0);
  Serial.printf("Before getting time %ld\n", millis() - t0);
  struct tm t = eink::GetCurrentTime();

  Serial.printf("After getting time %ld\n", millis() - t0);

  eink::WiFiDisconnect();
  esp_wifi_stop();
  adc_power_release();

  eink::Display display;
  int header_size = display.FontHeight(eink::FontSize::Size16);

  time_t now = eink::ToEpoch(t);

  int y = 0;
  int y_after_weather = 0;
  int x = kPadding;


  Serial.printf("Before drawing %ld\n", millis() - t0);
  if ((weather_data.http_code != HTTP_CODE_OK) || (data.http_code != HTTP_CODE_OK)) {
    display.DrawText(EPD_HEIGHT / 2, kPadding, "Unable to retrieve HTTP data", 0, eink::FontSize::Size24);
  } else {

    if (orientation == EPD_ROT_PORTRAIT) {
    // VERTICAL ORIENTATION 
    y = DrawHeader_Vertical(display, t, y);
    y = DrawWeather_Vertical(display, data.weather, now, y);
    y = DrawTempCO2_Vertical(display, data.temp_zasilanie, data.co2, "Rozdzielacz, CO2", now, y);
    y = DrawTempHumidity_Vertical(display, data.temp_livingroom, data.humidity_livingroom, "Salon", now, y);
    y = DrawTempHumidity_Vertical(display, data.temp_bedroom, data.humidity_bedroom, "Sypialnia", now, y);
    y = DrawTempHumidity_Vertical(display, data.temp_arek, data.humidity_arek, "Pokoj A", now, y);
    y = DrawTempHumidity_Vertical(display, data.temp_krzysiek, data.humidity_krzysiek, "Pokoj K", now, y);
    y = DrawTempHumidity_Vertical(display, data.temp_bathroom, data.humidity_bathroom, "Lazienka (gora)", now, y);
    DrawFooter_Vertical(display, millis() - t0);
    } else if (orientation == EPD_ROT_LANDSCAPE) {
    // HORIZONTAL ORIENTATION
    y = DrawHeader_Horizontal(display, t, y, millis() - t0);
    y = DrawOpenweather_Horizontal(display, weather_data, timestamp, now, y);
    DrawGrid(display, y);

    y += header_size;
    y_after_weather = y;

    y = DrawTempHumidity_Horizontal(display, data.temp_livingroom, data.humidity_livingroom, "Salon", now, x, y);
    y = DrawTempCO2_Horizontal(display, data.temp_livingroom_from_co2, data.co2, "Salon (CO2)", now, x, y);
    y = DrawTempHumidity_Horizontal(display, data.temp_bedroom, data.humidity_bedroom, "Sypialnia", now, x, y);

    x += EPD_WIDTH / 3;
    y = y_after_weather;
    
    y = DrawTempHumidity_Horizontal(display, data.temp_arek, data.humidity_arek, "Pokoj A", now, x, y);
    y = DrawTempHumidity_Horizontal(display, data.temp_krzysiek, data.humidity_krzysiek, "Pokoj K", now, x, y);
    y = DrawTempHumidity_Horizontal(display, data.temp_bathroom, data.humidity_bathroom, "Lazienka (gora)", now, x, y);

    x += EPD_WIDTH / 3;
    y = y_after_weather;

    // y = DrawTempHumidity_Horizontal(display, data.temp_house2_room, data.humidity_house2_room, "Dom 2 Pokoj", now, x, y);
    y = DrawTemp_Horizontal(display, data.temp_czerpnia, "Czerpnia", now, x, y);
    y = DrawTempHumidity_Horizontal(display, data.temp_house2_kitchen, data.humidity_house2_kitchen, "Dom 2 Kuchnia", now, x, y);
    y = DrawTempHumidity_Horizontal(display, data.temp_house2_bathroom, data.humidity_house2_bathroom, "Dom 2 Lazienka", now, x, y);


    }
  }


  Serial.printf("Before updating %ld\n", millis() - t0);
  display.Update();
  Serial.printf("After updating %ld\n", millis() - t0);

  runs++;
  start_deep_sleep(&t);
}

void loop() {}