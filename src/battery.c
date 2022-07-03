#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "config.h"
#include "main.h"
#include "battery.h"

#define TAG "battery.c"

void get_battery_reading(max1704x_t *dev, float *voltage, float *soc_percent, float *crate) {
    *voltage = 0;
    *soc_percent = 0;
    *crate = 0;
    
    esp_err_t r = max1704x_get_voltage(dev, voltage);
    if (r == ESP_OK) {
        ESP_LOGI(TAG, "Voltage: %.2fV", *voltage);
    }
    else
        ESP_LOGI(TAG, "Error %d: %s", r, esp_err_to_name(r));

    r = max1704x_get_soc(dev, soc_percent);
    if (r == ESP_OK) {
        ESP_LOGI(TAG, "SOC: %.2f%%", *soc_percent);
    }
    else
        ESP_LOGI(TAG, "Error %d: %s", r, esp_err_to_name(r));
#if CONFIG_MAX1704X_MODEL_8_9
    r = max1704x_get_crate(dev, crate);
    if (r == ESP_OK) {
        ESP_LOGI(TAG, "SOC rate of change: %.2f%%", *crate);
    }
    else
        ESP_LOGI(TAG, "Error %d: %s", r, esp_err_to_name(r));
#endif
}
