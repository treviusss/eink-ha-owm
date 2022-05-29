#include <Arduino.h>
#include <driver/adc.h>
#include <time.h>

#include <sstream>

#include "credentials.h"
// #include "eink/display.h"
// #include "eink/eink_display.h"
#include "eink/ha_client.h"
#include "eink/lilygo/runner.h"
#include "eink/tft_display.h"
#include "eink/time.h"
#include "eink/wifi.h"
// #include "epd_driver.h"
#include "esp_sleep.h"
#include "esp_wifi.h"

RTC_DATA_ATTR int runs;

void start_deep_sleep(struct tm *now) {
  // 10 minutes.
  time_t sleep_for_s = 10 * 60;

  // If it's past midnight and before 6 am, sleep until 6am.
  if (now != nullptr && now->tm_hour < 6) {
    struct tm time_6am = *now;
    time_6am.tm_hour = 6;
    sleep_for_s = eink::ToEpoch(time_6am) - eink::ToEpoch(*now);
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

void setup() {
  time_t t0 = millis();

  adc_power_acquire();

  print_wakeup_reason();

  Serial.printf("Before wifi %ld\n", millis() - t0);
  if (eink::WiFiBegin(kWiFiSSID, kWiFiPass) != 0) {
    Serial.printf("Unable to connect to WiFi. Sleeping.\n");
    start_deep_sleep(nullptr);
  }
  Serial.printf("After wifi %ld\n", millis() - t0);
  eink::ConfigNTP();

  eink::HAClient hacli(kHomeAssistantAPIUrl, kHomeAssistantToken);
  eink::HAData data = hacli.FetchData();

  Serial.printf("After getting data %ld\n", millis() - t0);

  Serial.printf("Before getting time %ld\n", millis() - t0);
  struct tm t = eink::GetCurrentTime();
  Serial.printf("After getting time %ld\n", millis() - t0);

  eink::WiFiDisconnect();
  esp_wifi_stop();
  adc_power_release();

  eink::lilygo::LilygoRunner runner;
  runner.Draw(data, t, runs);

  runs++;
  start_deep_sleep(&t);
}

void loop() {}