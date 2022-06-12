#ifndef PETNET_ESP32_FEEDING_H
#define PETNET_ESP32_FEEDING_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_err.h"

typedef struct {
    char meal_name[21];
    char pet_name[26];
    float feed_amount;
    char feed_amount_fraction[6];   // e.g. 1/4 cup, 15/16 cup
    uint8_t interrupter_count;      // Count number of photointerrupt for feed dispensing motor
    char feed_time_utc[9];
    char feed_time_tz[9];
    time_t next_feeding_time;
    uint8_t is_active;
    uint8_t dow;
} feeding_schedule_t;

void feeding_schedule_init(char *json_payload, feeding_schedule_t **dp_schedule, uint8_t *nbr_feeding_times);
void feeding_schedule_free(feeding_schedule_t **dp_schedule, uint8_t nbr_feeding_times);
void get_next_feeding_time(time_t *next_time, uint8_t *feed_index, feeding_schedule_t *schedule, uint8_t nbr_feeding_times);
esp_err_t store_feeding_schedule(feeding_schedule_t **dp_schedule, uint8_t nbr_feeding_times);
esp_err_t load_feeding_schedule(feeding_schedule_t **dp_schedule, uint8_t *nbr_feeding_times);

#endif