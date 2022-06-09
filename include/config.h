#ifndef PETNET_ESP32_CONFIG_H
#define PETNET_ESP32_CONFIG_H

#include "driver/ledc.h"
#include "driver/i2c.h"
#if CONFIG_IDF_TARGET_ESP32
#define BLUE_LED        27
#define RED_LED         25
#define GREEN_LED       26
#define MOTOR_SNSR      32
#define MOTOR_RELAY     19
#define HOPPER_SNSR     18
#define BUTTON          23
#define BATTERY_ALERT   4
#define POWER_SNSR      5  
#elif CONFIG_IDF_TARGET_ESP32S2
#define BLUE_LED        8
#define RED_LED         6
#define GREEN_LED       7
#define MOTOR_SNSR      14
#define MOTOR_RELAY     10
#define HOPPER_SNSR     11
#define BUTTON          12
#define BATTERY_ALERT   4
#define POWER_SNSR      5  
#endif

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
#if CONFIG_IDF_TARGET_ESP32
#define I2C_MASTER_SCL_IO           22                          /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           21                          /*!< GPIO number used for I2C master data  */
#elif CONFIG_IDF_TARGET_ESP32S2
#define I2C_MASTER_SCL_IO           37                          /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           38                          /*!< GPIO number used for I2C master data  */
#endif
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

#define HASH_LEN 32

#ifndef WIFI_SSID
#define WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define WIFI_PASS       CONFIG_ESP_WIFI_PASSWORD
#define WIFI_MAX_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#endif

#if CONFIG_IDF_TARGET_ESP32
#define CONFIG_MAX_CPU_FREQ_MHZ     CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ
#elif CONFIG_IDF_TARGET_ESP32S2
#define CONFIG_MAX_CPU_FREQ_MHZ     CONFIG_ESP32S2_DEFAULT_CPU_FREQ_MHZ
#endif

#if CONFIG_IDF_TARGET_ESP32S2
#define LED_ON  1
#define LED_OFF 0
#define CONTROL_BOARD_REVISION   "D-1g"
#define OTA_UPDATE_URL  "https://smartpetfeeder.net/static/firmware/firmware-revD-1g-current.bin"
#elif GEN1
#define LED_ON  1
#define LED_OFF 0
#define CONTROL_BOARD_REVISION   "C-1g"
#define OTA_UPDATE_URL  "https://smartpetfeeder.net/static/firmware/firmware-revC-1g-current.bin"
#else
#define LED_ON  0
#define LED_OFF 1
#define CONTROL_BOARD_REVISION   "C-2g"
#define OTA_UPDATE_URL  "https://smartpetfeeder.net/static/firmware/firmware-revC-2g-current.bin"
#endif

extern i2c_config_t i2c_config;

#endif
