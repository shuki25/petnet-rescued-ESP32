#ifndef PETNET_ESP32_CONFIG_H
#define PETNET_ESP32_CONFIG_H

#include "driver/ledc.h"
#include "driver/i2c.h"

#define BLUE_LED        27
#define RED_LED         25
#define GREEN_LED       26
#define MOTOR_SNSR      32
#define MOTOR_RELAY     19
#define HOPPER_SNSR     18
#define BUTTON          23
#define BATTERY_ALERT   4
#define POWER_SNSR      5  

#define GPIO_OUTPUT_PIN_SEL     ((1ULL<<RED_LED) | (1ULL<<GREEN_LED) | (1ULL<<BLUE_LED))
#define GPIO_INPUT_PIN_SEL      ((1ULL<<MOTOR_SNSR) | (1ULL<<BATTERY_ALERT))
#define GPIO_ANYEDGE_PIN_SEL    ((1ULL<<HOPPER_SNSR))
#define GPIO_BUTTON_SEL         ((1ULL<<BUTTON))
#define GPIO_POWER_SNSR_SEL     ((1ULL<<POWER_SNSR))
#define ESP_INTR_FLAG_DEFAULT   0

#define UART_RX_PIN     16
#define UART_TX_PIN     17
#define UART_BAUD_RATE  9600
// #define UART_BAUD_RATE  115200
#define RX_BUFFER_SIZE  1024
#define TX_BUFFER_SIZE  1024

// PWM configuration for motor slow starter to reduce inrush current
#define PWM_TIMER       LEDC_TIMER_0
#define PWM_MODE        LEDC_LOW_SPEED_MODE
#define PWM_OUTPUT_IO   (MOTOR_RELAY)
#define PWM_CHANNEL     LEDC_CHANNEL_0
#define PWM_DUTY_RES    LEDC_TIMER_10_BIT
#define PWM_DUTY        (512)  // duty set at 50%
#define PWM_DUTY_MAX    (1023)
#define PWM_FREQUENCY   (5000)

// I2C configuration
#define I2C_MASTER_SCL_IO           22                          /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           21                          /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0                           /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                      /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define WIFI_SSID       "hiccup"
#define WIFI_PASS       "dragonrider2014"
#define WIFI_MAX_RETRY  5
#define API_KEY         "63b31b00a770ac4fc22154fdc8eb4875958607fe"
// #define API_BASE_URL    "http://192.168.2.118:8111/api"
// #define API_BASE_URL    "https://7453-24-59-154-128.ngrok.io/api"
#define API_BASE_URL    "https://smartpetfeeder.net/api"
#define OTA_UPDATE_URL  "https://smartpetfeeder.net/static/firmware/firmware-revC-2g-current.bin"

#define HASH_LEN 32
#define CONTROL_BOARD_REVISION   "C-2g"

#ifndef WIFI_SSID
#define WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define WIFI_PASS       CONFIG_ESP_WIFI_PASSWORD
#define WIFI_MAX_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#endif

extern i2c_config_t i2c_config;

#endif
