#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "util.h"
#include "esp_system.h"
#include "esp_log.h"

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3
#include "driver/temp_sensor.h"
#endif

#define TAG "util"
extern int heap_size_start;

void print_heap_size(char *tag) {
    ESP_LOGD(TAG, "%s: Minimum free heap size: %d bytes, %d bytes used.", tag ? tag : "No Tag", esp_get_free_heap_size(), heap_size_start - esp_get_free_heap_size());
}

void get_chip_id(char *chip_identifier) {
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    sprintf(chip_identifier, "ESP32-%02x%02x-%02x%02x%02x%02x", MAC2STR(mac));
}

float get_temperature() {
    float tsens_out;

#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32C3
    // Initialize touch pad peripheral, it will start a timer to run a filter
    ESP_LOGI(TAG, "Initializing Temperature sensor");
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor_get_config(&temp_sensor);
    ESP_LOGI(TAG, "default dac %d, clk_div %d", temp_sensor.dac_offset, temp_sensor.clk_div);
    temp_sensor.dac_offset = TSENS_DAC_DEFAULT; // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();
    temp_sensor_read_celsius(&tsens_out);
#elif CONFIG_IDF_TARGET_ESP32
    tsens_out = temprature_sens_read();
    tsens_out = (tsens_out - 32) / 1.8;
#endif

    return tsens_out;
}

char *secret_generator(char *str, size_t size)
{
    const char charset[] = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKMNOPQRSTUVWXYZ0123456789";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = esp_random() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

/*
 * find rational approximation to given real number
 * https://www.ics.uci.edu/%7Eeppstein/numth/frap.c
 * 
 * by David Eppstein / UC Irvine / 8 Aug 1993
 *
 * With corrections from Arno Formella, May 2008
 *
 */


char *f2frac(float real_num, uint16_t limit_denominator) {
    long m[2][2];
    double x;
    long maxden;
    long ai;
    char *fraction;

    x = real_num;
    maxden = limit_denominator;

    /* initialize matrix */
    m[0][0] = m[1][1] = 1;
    m[0][1] = m[1][0] = 0;

    /* loop finding terms until denom gets too big */
    while (m[1][0] *  ( ai = (long)x ) + m[1][1] <= maxden) {
        long t;
        t = m[0][0] * ai + m[0][1];
        m[0][1] = m[0][0];
        m[0][0] = t;
        t = m[1][0] * ai + m[1][1];
        m[1][1] = m[1][0];
        m[1][0] = t;
        if(x==(double)ai) break;     // AF: division by zero
        x = 1/(x - (double) ai);
        if(x>(double)0x7FFFFFFF) break;  // AF: representation failure
    } 
    fraction = malloc(sizeof(char) * 32);
    sprintf(fraction, "%ld/%ld", m[0][0], m[1][0]);
    return fraction;
}

time_t utc_mktime(struct tm *tm)
{
	char timezone[50], timezone2[50];
    time_t converted_time;
    strlcpy(timezone, getenv("TZ"), sizeof(timezone));
    setenv("TZ", "UTC+0", 1);
    tzset();
    // vTaskDelay(200 / portTICK_PERIOD_MS);
    converted_time = mktime(tm);

    strlcpy(timezone2, getenv("TZ"), sizeof(timezone2));
    setenv("TZ", timezone, 1);
    tzset();

    return converted_time;

}