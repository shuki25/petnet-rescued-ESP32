#ifndef PETNET_ESP32_MAIN_H
#define PETNET_ESP32_MAIN_H

#include "esp_wifi.h"
#include "freertos/semphr.h"
#include "time.h"
#include "feeding.h"

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
    uint8_t setting_version;
    char device_id[24];
    char ssid[32];
    char password[64];
    char api_key[48];
    char device_key[48];
    char secret[16];
    char firmware_version[8];
    time_t datetime_registered;
    time_t datetime_last_boot;
    uint8_t is_registered;
    uint8_t is_setup_done;
    uint8_t is_24h_mode;
    uint8_t is_notification_on;
    char tz[32];
} petnet_rescued_settings_t;

extern QueueHandle_t uart_queue;
extern xSemaphoreHandle nextion_mutex;
extern petnet_rescued_settings_t petnet_settings;
extern feeding_schedule_t *feeding_schedule;
extern uint8_t num_feeding_times;
extern bool get_next_meal_slot;
extern uint8_t red_blinky, green_blinky;
extern bool is_nextion_available;
void dispense_food(uint8_t encoder_ticks);

#endif