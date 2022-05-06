#include <stdio.h>
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "config.h"
#include "main.h"
#include "battery.h"

#define TAG "battery.c"

static esp_adc_cal_characteristics_t *adc_chars;
static uint32_t previous_reading;
static const adc_channel_t channel = ADC_CHANNEL_5;     //GPIO33
static const adc_bits_width_t width = ADC_WIDTH_BIT_12;

static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;

void check_efuse(void) {
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

void print_char_val_type(esp_adc_cal_value_t val_type) {
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}

void setup_adc_channel(void) {
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();

    //Configure ADC
    if (unit == ADC_UNIT_1) {
        adc1_config_width(width);
        adc1_config_channel_atten(channel, atten);
    } else {
        adc2_config_channel_atten((adc2_channel_t)channel, atten);
    }

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);
}

// void get_voltage_reading(void) {
//     uint32_t adc_reading = 0;
//     uint32_t reading = 0;
//     int j = 0;
//     float min_voltage = 1529.0 * 2;
//     float max_voltage = 2088.0 * 2;
//     float voltage_range = max_voltage - min_voltage;

//     adc1_config_width(width);
//     adc1_config_channel_atten(channel, atten);

//     ESP_LOGI(TAG, "Previous reading: %d", previous_reading);
//     //Multisampling
//     for (int i = 0; i < NO_OF_SAMPLES; i++) {
//         reading = adc1_get_raw((adc1_channel_t)channel);
//         if (previous_reading == 0) {
//             adc_reading += reading;
//             j++;
//         }
//         if (reading - 25 <= previous_reading && reading + 25 >= previous_reading) {
//             adc_reading += reading;
//             j++;
//         }
//     }
//     if (j) {
//         adc_reading /= j;
//     } else {
//         adc_reading = reading;
//     }
    
//     previous_reading = adc_reading;
//     //Convert adc_reading to voltage in mV
//     uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
//     // printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
//     float battery_percent = (((((float)voltage*2) - min_voltage) / voltage_range) * 100.0) + 0.5;
//     if (battery_percent > 100 && battery_percent < 103) {
//         battery_percent = 100.0;
//     }
//     // ESP_LOGI(TAG, "voltage: %d min_voltage: %f voltage_range: %f j: %d", voltage, min_voltage, voltage_range, j);
//     if (battery_percent >= 103) {
//         ESP_LOGI(TAG, "Battery is not detected. (%dmV)", voltage * 2);
//     } else {
//         ESP_LOGI(TAG, "Battery: %.2f%% (%dmV)", battery_percent, voltage * 2);
//     }
// }

void get_battery_reading(float *voltage, float *soc_percent) {
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;
    uint8_t *fuel_gauge_buf = (uint8_t *)malloc(2);
    *voltage = 0;
    *soc_percent = 0;

    esp_err_t ret = i2c_master_read_register(i2c_master_port, MAX1704X_REGISTER_VCELL, fuel_gauge_buf, 2);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "i2c vcell register read ok");
        // ESP_LOG_BUFFER_HEXDUMP(TAG, fuel_gauge_buf, 2, ESP_LOG_INFO);
        *voltage = (((fuel_gauge_buf[0] * 16) + (fuel_gauge_buf[1]>>4)) * MAX1704X_PRECISION) / 1000;
        ESP_LOGI(TAG, "Voltage: %.2f V", *voltage);
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "I2C timed out.");
    }

    ret = i2c_master_read_register(i2c_master_port, MAX1704X_REGISTER_SOC, fuel_gauge_buf, 2);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "i2c soc register read ok");
        // ESP_LOG_BUFFER_HEXDUMP(TAG, fuel_gauge_buf, 2, ESP_LOG_INFO);
        *soc_percent = (float)fuel_gauge_buf[0];
        *soc_percent += ((float)fuel_gauge_buf[1])/256;
        ESP_LOGI(TAG, "SOC: %.2f%%", *soc_percent);
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "I2C timed out.");
    }

    free(fuel_gauge_buf);
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
