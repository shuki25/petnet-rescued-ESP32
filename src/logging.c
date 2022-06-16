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
#include "queue.h"

#define TAG "logging.c"

queue_t *logging_queue = NULL;

uint16_t log_feeding(char *pet_name, char *feed_type, float feed_amt) {
    char buffer[256];
    char timestamp[64];
    char *api_content;
    uint16_t status_code = 0;
    time_t current_time;
    
    current_time = time(&current_time);
    struct tm *utc_timeinfo = gmtime(&current_time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", utc_timeinfo);
    sprintf(buffer, "{\"pet_name\": \"%s\", \"feed_type\": \"%s\", \"feed_amt\": %.3f, \"feed_timestamp\": \"%s\"}", pet_name, feed_type, feed_amt, timestamp);
    ESP_LOGI(TAG, "buffer: %s", buffer);
    status_code = api_post(&api_content, petnet_settings.api_key, petnet_settings.device_key, "/feeding-log/", buffer);

    free(api_content);

    if (status_code >= 200) {
        add_to_queue(buffer, false);
        print_logging_queue();
    }

    return status_code;
}

void post_logging_queue(void) {
    char *buffer = NULL;
    char *api_content = NULL;
    uint16_t status_code = 0;

    buffer = dequeue(logging_queue);
    while (buffer != NULL) {
        status_code = api_post(&api_content, petnet_settings.api_key, petnet_settings.device_key, "/feeding-log/", buffer);
        free(api_content);
        if (status_code >= 500) {
            ESP_LOGI(TAG, "Error posting to API, adding back to queue...");
            add_to_queue(buffer, true);
            free(buffer);
            return;
        } else {
            free(buffer);
            buffer = dequeue(logging_queue);
        }
    }

}

void add_to_queue(char *buffer, bool front) {
    if (buffer != NULL) {
        char *cache_content = malloc(strlen(buffer) + 1);
        if (cache_content == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for cache_content");
            return;
        } else {
            strcpy(cache_content, buffer);
        }
        if (!front) {
            enqueue(logging_queue, cache_content);
        } else {
            enqueue_front(logging_queue, cache_content);
        }
        ESP_LOGI(TAG, "Added to queue: %s", cache_content);
    }   
}

void print_queue_content(void *data) {
    if (data != NULL) {
        ESP_LOGI(TAG, "Cache content: %s", (char *)data);
    }
}

void print_logging_queue(void) {
    queue_print(logging_queue, print_queue_content);
}
