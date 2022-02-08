#ifndef PETNET_ESP32_CONFIG_H
#define PETNET_ESP32_CONFIG_H

#define ONBOARD_LED 2
#define RED_LED     5
#define GREEN_LED   18
#define MOTOR_SNSR  4
#define BUTTON      19

#define GPIO_OUTPUT_PIN_SEL     ((1ULL<<RED_LED) | (1ULL<<GREEN_LED) | (1ULL<<ONBOARD_LED))
#define GPIO_INPUT_PIN_SEL      ((1ULL<<MOTOR_SNSR))
#define GPIO_BUTTON_SEL         ((1ULL<<BUTTON))
#define ESP_INTR_FLAG_DEFAULT   0

#define WIFI_SSID       "hiccup"
#define WIFI_PASS       "dragonrider2014"
#define WIFI_MAX_RETRY  5

#ifndef WIFI_SSID
#define WIFI_SSID       CONFIG_ESP_WIFI_SSID
#define WIFI_PASS       CONFIG_ESP_WIFI_PASSWORD
#define WIFI_MAX_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#endif

#endif
