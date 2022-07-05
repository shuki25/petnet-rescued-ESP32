#ifndef PETNET_ESP32_RTC_H
#define PETNET_ESP32_RTC_H

#if ONBOARD_RTC
#include "pcf8563.h"
#endif

esp_err_t external_rtc_init(i2c_dev_t *dev);
esp_err_t sync_rtc_clock(i2c_dev_t *dev, char *tz);
esp_err_t set_rtc_time(i2c_dev_t *dev, struct tm *time);

#endif