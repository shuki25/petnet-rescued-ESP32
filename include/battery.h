#ifndef PETNET_ESP32_BATTERY_H
#define PETNET_ESP32_BATTERY_H

#define DEFAULT_VREF        1100        // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES       64          // Multisampling
#define FUEL_GAUGE_ALERT    4           // GPIO 4, Pull up

// MAX1704X Fuel Gauge
#define MAX1704X_I2C_SLAVE_ADDR         0x36
#define MAX1704X_REGISTER_VCELL         0x02
#define MAX1704X_REGISTER_SOC           0x04
#define MAX1704X_REGISTER_MODE          0x06
#define MAX1704X_REGISTER_VERSION       0x08
#define MAX1704X_REGISTER_CONFIG        0x0C
#define MAX1704X_REGISTER_COMMAND       0xFE
#define WRITE_BIT I2C_MASTER_WRITE              /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ                /*!< I2C master read */
#define ACK_CHECK_EN 0x1                        /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0                       /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                             /*!< I2C ack value */
#define NACK_VAL 0x1                            /*!< I2C nack value */

#define MAX1704X_RESET_COMMAND          0x5400
#define MAX1704X_QUICKSTART_MODE        0x4000

#define MAX1704X_DEFER_ADDRESS          (uint8_t)0

#define MAX17043_mV                     1.25
#define MAX1704X_PRECISION              MAX17043_mV

void check_efuse(void);
void print_char_val_type(esp_adc_cal_value_t val_type);
void setup_adc_channel(void);
void get_battery_reading(float *voltage, float *soc_percent);

esp_err_t __attribute__((unused)) i2c_master_read_register(i2c_port_t i2c_num, uint8_t memory_addr, uint8_t *data_rd, size_t size);

#endif