#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"

#include "config.h"
#include "main.h"
#include "rtc.h"

#define TAG "rtc.c"

#if ONBOARD_RTC
#include <pcf8563.h>
#include <i2cdev.h>

esp_err_t external_rtc_init(i2c_dev_t *dev) {
    ESP_LOGI(TAG, "Initializing RTC");
    esp_err_t r = pcf8563_init_desc(dev, I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
    if (r != ESP_OK) {
        ESP_LOGE(TAG, "Error %d: %s", r, esp_err_to_name(r));
        return r;
    }
    return ESP_OK;
}

esp_err_t sync_rtc_clock(i2c_dev_t *dev, char *tz) {

    struct tm time;
    bool valid;

    ESP_ERROR_CHECK(pcf8563_get_time(dev, &time, &valid));

    if (valid) {
        ESP_LOGI(TAG, "RTC time: %04d-%02d-%02d %02d:%02d:%02d", time.tm_year + 1900, time.tm_mon + 1,
                 time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

        time_t t = mktime(&time);
        ESP_LOGI(TAG, "Setting System Time: %s", asctime(&time));
        struct timeval now = { .tv_sec = t };
        setenv("TZ", tz, 1);
        tzset();
        settimeofday(&now, NULL);
    } else {
        ESP_LOGE(TAG, "RTC time is not valid, possibly due to power failure");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t set_rtc_time(i2c_dev_t *dev, struct tm *time) {
    ESP_LOGI(TAG, "Setting RTC time to: %04d-%02d-%02d %02d:%02d:%02d", time->tm_year + 1900, time->tm_mon + 1,
                 time->tm_mday, time->tm_hour, time->tm_min, time->tm_sec);
    ESP_ERROR_CHECK(pcf8563_set_time(dev, time));
    return ESP_OK;
}

#endif