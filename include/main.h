#ifndef PETNET_ESP32_MAIN_H
#define PETNET_ESP32_MAIN_H

#include "esp_wifi.h"
#include "freertos/semphr.h"

typedef struct {
    uint32_t io_num;
    uint8_t state;
    uint16_t delay;
} led_config_t;

typedef struct {
    uint8_t state;
    uint32_t counter;
} input_state_t;

typedef struct {
    char tz[32];
    uint8_t is_setup_done;
    uint8_t is_24h_mode;
} petnet_rescued_settings_t;

extern QueueHandle_t uart_queue;
extern xSemaphoreHandle nextion_mutex;
extern uint8_t temprature_sens_read();

#endif