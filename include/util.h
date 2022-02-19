#ifndef PETNET_ESP32_UTIL_H
#define PETNET_ESP32_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

void print_heap_size(char *tag);
void get_chip_id(char *chip_identifier);

#endif