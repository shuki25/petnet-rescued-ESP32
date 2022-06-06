#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cJSON.h>
#include "esp_log.h"

#include "config.h"
#include "main.h"
#include "event.h"
#include "json_util.h"
#include "api_client.h"
#include "feeding.h"
#include "logging.h"
#include "ota_update.h"

#define TAG "event.c"


uint16_t notify_event_completed(uint32_t event_id) {
    char buffer[256];
    char *api_content;
    uint16_t status_code = 0;


    sprintf(buffer, "{\"id\": %d}", event_id);
    status_code = api_post(&api_content, petnet_settings.api_key, petnet_settings.device_key, "/event/task-completed/", buffer);

    free(api_content);

    return status_code;
}

uint16_t process_event(cJSON *event) {
    cJSON *rs, *json_payload;
    uint32_t event_id = (uint32_t)99999999, value_number;
    uint16_t event_code = (uint16_t)999, status_code;
    char *value_string, *content;
    float feed_amt = 0.0;

    rs = cJSON_GetObjectItem(event, "id");
    if (cJSON_IsNumber(rs)) {
        event_id = (uint32_t)rs->valueint;
    }

    rs = cJSON_GetObjectItem(event, "event_code");
    if (cJSON_IsNumber(rs)) {
        event_code = (uint16_t)rs->valueint;
    }

    ESP_LOGI(TAG, "Event ID: %d Event Code: %d,", event_id, event_code);
    switch(event_code) {
        case SMART_FEEDER_EVENT_MANUAL_FEED:
            ESP_LOGI(TAG, "in SMART_FEEDER_EVENT_MANUAL_FEED");
            json_payload = cJSON_GetObjectItem(event, "json_payload");
            if (cJSON_IsObject(json_payload)) {
                rs = cJSON_GetObjectItem(json_payload, "feed_amt");
                if (cJSON_IsNumber(rs)) {
                    feed_amt = (float)rs->valuedouble;
                }
                rs = cJSON_GetObjectItem(json_payload, "ticks");
                if (cJSON_IsNumber(rs)) {
                    value_number = (uint32_t)rs->valueint;
                    ESP_LOGI(TAG, "Dispensing food: %d ticks", value_number);
                    dispense_food(value_number);
                    ESP_LOGI(TAG, "Food dispensed");
                    status_code = log_feeding("Remote Manual", "R", feed_amt);
                    status_code = notify_event_completed(event_id);
                }
            }            
            break;

        case SMART_FEEDER_EVENT_SCHEDULE_CHANGE:
            ESP_LOGI(TAG, "in SMART_FEEDER_EVENT_SCHEDULE_CHANGE");
            feeding_schedule_free(&feeding_schedule, num_feeding_times);
            status_code = api_get(&content, petnet_settings.api_key, petnet_settings.device_key, "/feeding-schedule/");
            if (status_code == 200) {
                feeding_schedule_init(content, &feeding_schedule, &num_feeding_times);
                get_next_meal_slot = true;
                ESP_LOGI(TAG, "Feeding Schedule has %d feeding time slots", num_feeding_times);
            }
            free(content);
            status_code = notify_event_completed(event_id);
            break;
            

        case SMART_FEEDER_EVENT_SETTINGS_CHANGE:
            ESP_LOGI(TAG, "in SMART_FEEDER_EVENT_SETTINGS_CHANGE");
            status_code = api_get(&content, petnet_settings.api_key, petnet_settings.device_key, "/settings/");
            if (status_code == 200) {
                json_payload = cJSON_Parse(content);
                if (json_payload != NULL) {
                    rs = cJSON_GetObjectItem(json_payload, "results");
                    value_string = fetch_json_value(rs, "is_setup_done");
                    if (value_string) {
                        petnet_settings.is_setup_done = atoi(value_string);
                    }
                    value_string = fetch_json_value(rs, "tz_esp32");
                    if (value_string) {
                        ESP_LOGI(TAG, "Old timezone: %s", petnet_settings.tz); 
                        ESP_LOGI(TAG, "Setting timezone to %s", value_string);
                        if (strcmp(value_string, petnet_settings.tz) != 0) {
                            ESP_LOGI(TAG, "Timezone changed, updating time");
                            strcpy(petnet_settings.tz, value_string);
                            tz_changed = true;
                        }
                    }
                } else {
                    ESP_LOGI(TAG, "Error with JSON");
                }
                cJSON_Delete(json_payload);
            }
            status_code = notify_event_completed(event_id);
            free(content);
            break;

        case SMART_FEEDER_EVENT_FACTORY_RESET:
            ESP_LOGI(TAG, "Resetting to Factory Settings");
            status_code = notify_event_completed(event_id);
            break;

        case SMART_FEEDER_EVENT_WIFI_SETTING_RESET:
            ESP_LOGI(TAG, "Resetting WiFi Credentials");
            status_code = notify_event_completed(event_id);
            break;
        
        case SMART_FEEDER_EVENT_FIRMWARE_UPDATE:
            ESP_LOGI(TAG, "Downloading new Firmware update");
            status_code = notify_event_completed(event_id);
            red_blinky = true;
            ota_update_task();
            red_blinky = false;
            break;

        default:
            ESP_LOGI(TAG, "Unknown Event Code: %d", event_code);
            status_code = notify_event_completed(event_id);
            break;
    }
    return event_code;
}
