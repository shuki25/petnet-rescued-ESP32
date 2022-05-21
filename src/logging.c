#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cJSON.h>
#include "esp_log.h"

#include "config.h"
#include "main.h"
#include "logging.h"
#include "feeding.h"
#include "json_util.h"
#include "api_client.h"

#define TAG "logging.c"

uint16_t log_feeding(char *pet_name, char *feed_type, float feed_amt) {
    char buffer[256];
    char *api_content;
    uint16_t status_code = 0;


    sprintf(buffer, "{\"pet_name\": \"%s\", \"feed_type\": \"%s\", \"feed_amt\": %.3f}", pet_name, feed_type, feed_amt);
    ESP_LOGI(TAG, "buffer: %s", buffer);
    status_code = api_post(&api_content, petnet_settings.api_key, petnet_settings.device_key, "/feeding-log/", buffer);

    free(api_content);

    return status_code;
}