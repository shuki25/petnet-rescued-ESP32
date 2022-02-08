#ifndef PETNET_ESP32_MAIN_H
#define PETNET_ESP32_MAIN_H

#include "esp_wifi.h"

typedef struct {
    uint32_t io_num;
    uint8_t state;
    uint16_t delay;
} led_config_t;

typedef struct {
    uint8_t state;
    uint32_t counter;
} input_state_t;

#endif