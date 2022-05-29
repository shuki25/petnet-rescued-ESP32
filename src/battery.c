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


int16_t be16_to_cpu_signed(const uint8_t data[2]) {
    int16_t r;
    uint16_t u = (unsigned)data[1] | ((unsigned)data[0] << 8);
    memcpy(&r, &u, sizeof r);
    return r;
}

int16_t le16_to_cpu_signed(const uint8_t data[2]) {
    int16_t r;
    uint16_t u = (unsigned)data[0] | ((unsigned)data[1] << 8);
    memcpy(&r, &u, sizeof r);
    return r;
}

void get_battery_reading(float *voltage, float *soc_percent, float *crate) {
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;
    uint8_t *fuel_gauge_buf = (uint8_t *)malloc(2);
    *voltage = 0;
    *soc_percent = 0;
    *crate = 0;
    

    esp_err_t ret = i2c_master_read_register(i2c_master_port, MAX1704X_REGISTER_VCELL, fuel_gauge_buf, 2);
    
    if (ret == ESP_OK) {
        // ESP_LOG_BUFFER_HEXDUMP(TAG, fuel_gauge_buf, 2, ESP_LOG_INFO);
        *voltage = (((float)fuel_gauge_buf[0] * 256 + (float)fuel_gauge_buf[1]) * MAX1704X_PRECISION) / 1000;
        ESP_LOGI(TAG, "Voltage: %.2f V", *voltage);
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "I2C timed out.");
    }

    ret = i2c_master_read_register(i2c_master_port, MAX1704X_REGISTER_SOC, fuel_gauge_buf, 2);
    
    if (ret == ESP_OK) {
        // ESP_LOG_BUFFER_HEXDUMP(TAG, fuel_gauge_buf, 2, ESP_LOG_INFO);
        *soc_percent = (float)fuel_gauge_buf[0];
        *soc_percent += ((float)fuel_gauge_buf[1])/256;
        if (*soc_percent > 100) {
            *soc_percent = 100.0;
        }
        ESP_LOGI(TAG, "SOC: %.2f%%", *soc_percent);
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "I2C timed out.");
    }

    ret = i2c_master_read_register(i2c_master_port, MAX1704X_REGISTER_CRATE, fuel_gauge_buf, 2);
    if (ret == ESP_OK) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, fuel_gauge_buf, 2, ESP_LOG_INFO);
        int16_t crate_value = be16_to_cpu_signed(fuel_gauge_buf);
        ESP_LOGI(TAG, "crateValue: %d", (int)crate_value);
        *crate = ((float)crate_value * MAX1704X_CRATE_PRECISION);  // calculated as percent per hour
        ESP_LOGI(TAG, "SOC rate of change: %.2f%%", *crate);
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "I2C timed out.");
    }

    free(fuel_gauge_buf);
}

void reset_fuel_gauge() {
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;
    uint8_t fuel_gauge_buf[2];
    fuel_gauge_buf[0] = 0x40;
    fuel_gauge_buf[1] = 0x00;
    ESP_LOG_BUFFER_HEXDUMP(TAG, fuel_gauge_buf, 2, ESP_LOG_INFO);
    esp_err_t ret = i2c_master_write_word_register(i2c_master_port, MAX1704X_REGISTER_MODE, fuel_gauge_buf);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MAX17048 Reset (Quickstart)");
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "I2C timed out.");
    }
}

esp_err_t __attribute__((unused)) i2c_master_read_register(i2c_port_t i2c_num, uint8_t memory_addr, uint8_t *data_rd, size_t size) {
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX1704X_I2C_SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, memory_addr, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX1704X_I2C_SLAVE_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;

}

esp_err_t __attribute__((unused)) i2c_master_write_word_register(i2c_port_t i2c_num, uint8_t memory_addr, uint8_t *data_wr) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MAX1704X_I2C_SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, memory_addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data_wr[0], ACK_CHECK_EN);
    i2c_master_write_byte(cmd, data_wr[1], ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}