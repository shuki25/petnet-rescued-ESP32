#ifndef PETNET_ESP32_JSON_UTIL_H
#define PETNET_ESP32_JSON_UTIL_H

#include <stdio.h>
#include <string.h>
#include <cJSON.h>
#include <cJSON.h>

char *fetch_json_value(cJSON *payload, char *key);

#endif