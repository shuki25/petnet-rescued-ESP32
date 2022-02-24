#ifndef PETNET_ESP32_UTIL_H
#define PETNET_ESP32_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#define TIME_T_MAX_BITS     32

void print_heap_size(char *tag);
void get_chip_id(char *chip_identifier);
float get_temperature();
extern uint8_t temprature_sens_read();
char *secret_generator(char *str, size_t size);
char *f2frac(float real_num, uint16_t limit_denominator);
time_t utc_mktime(struct tm *tm);

#endif