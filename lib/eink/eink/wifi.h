#ifndef __EINK_WIFI_H__
#define __EINK_WIFI_H__
#include <stdint.h>
// #include <Arduino.h>
// #include <Wi

#define EINK_WIFI_CONN_TIMEOUT_MS 10000

namespace eink {

int WiFiBegin(const char* SSID, const char* password);

int WiFiDisconnect();

int8_t GetWiFiRSSI();

}  // namespace eink
#endif  // __EINK_WIFI_H__