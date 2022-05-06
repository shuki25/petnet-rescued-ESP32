#include <stdio.h>
#include <cJSON.h>
#include "feeding.h"
#include "json_util.h"
#include "esp_log.h"
#include "util.h"

const char *TAG = "feeding.c";
int feeding_dow[7] = {1, 2, 4, 8, 16, 32, 64};


void feeding_schedule_init(char *json_payload, feeding_schedule_t **dp_schedule, uint8_t *nbr_feeding_times) {
    cJSON *payload, *object, *results;

    payload = cJSON_Parse(json_payload);
    results = cJSON_GetObjectItem(payload, "count");
    feeding_schedule_t *schedule;

    if(cJSON_IsNumber(results)) {
        ESP_LOGI(TAG, "count: %d", results->valueint);
    } else {
        ESP_LOGE(TAG, "count has invalid value");
        cJSON_Delete(payload);
        return;
    }
    *nbr_feeding_times = (uint8_t)results->valueint;
    ESP_LOGD(TAG, "sizeof *schedule: %d, sizeof feeding_schedule_t: %d", sizeof(*schedule), sizeof(feeding_schedule_t));
    
    schedule = malloc(sizeof(*schedule) * *nbr_feeding_times);
    memset(schedule, 0, sizeof(*schedule) * *nbr_feeding_times);

    results = cJSON_GetObjectItem(payload, "results");
    if(cJSON_IsArray(results)) {
        int i=0;
        cJSON_ArrayForEach(object, results) {
            cJSON *fs = cJSON_GetObjectItem(object, "feeding_schedule");
            cJSON *dow = cJSON_GetObjectItem(object, "dow");
            cJSON *time = cJSON_GetObjectItem(object, "time");
            cJSON *motor_timing = cJSON_GetObjectItem(object, "motor_timing");
            
            if (cJSON_IsObject(fs)) {
                cJSON *active_flag = cJSON_GetObjectItem(fs, "active_flag");
                cJSON *meal_name = cJSON_GetObjectItem(fs, "meal_name");            
                cJSON *pet = cJSON_GetObjectItem(fs, "pet");
                if (cJSON_IsString(meal_name)) {
                    schedule[i].meal_name = malloc(sizeof(char) * strlen(meal_name->valuestring) + 1);
                    strcpy(schedule[i].meal_name, meal_name->valuestring);
                }
                if (cJSON_IsBool(active_flag)) {
                    schedule[i].is_active = active_flag->valueint;
                }
                if (cJSON_IsObject(pet)) {
                    cJSON *pet_name = cJSON_GetObjectItem(pet, "name");
                    if (cJSON_IsString(pet_name)) {
                        schedule[i].pet_name = malloc(sizeof(char) * strlen(pet_name->valuestring) + 1);
                        strcpy(schedule[i].pet_name, pet_name->valuestring);
                    }
                }
            }

            if (cJSON_IsNumber(dow)) {
                schedule[i].dow = dow->valueint;
            }

            if (cJSON_IsString(time)) {
                strcpy(schedule[i].feed_time_utc, time->valuestring);
            }

            if (cJSON_IsObject(motor_timing)) {
                char *fraction;
                cJSON *feed_amount = cJSON_GetObjectItem(motor_timing, "feed_amount");
                cJSON *interrupter_count = cJSON_GetObjectItem(motor_timing, "interrupter_count");
                if (cJSON_IsNumber(feed_amount)) {
                    schedule[i].feed_amount = feed_amount->valuedouble;
                    fraction = f2frac(feed_amount->valuedouble, 16);
                    strcpy(schedule[i].feed_amount_fraction, fraction);
                    free(fraction);
                }
                if (cJSON_IsNumber(interrupter_count)) {
                    schedule[i].interrupter_count = interrupter_count->valueint;
                }
            }
            ESP_LOGI(TAG, "Record #%d", i);
            ESP_LOG_BUFFER_HEXDUMP(TAG, &schedule[i], sizeof(feeding_schedule_t), ESP_LOG_INFO);
            i++;
        }
        
    }
    *dp_schedule = schedule;
    cJSON_Delete(payload);
}

void feeding_schedule_free(feeding_schedule_t **dp_schedule, uint8_t nbr_feeding_times) {
    print_heap_size("Before freeing memory from schedule");
    feeding_schedule_t *schedule = *dp_schedule;

    ESP_LOG_BUFFER_HEXDUMP(TAG, schedule, sizeof(*schedule) * nbr_feeding_times, ESP_LOG_DEBUG);

    for (int i=0; i < nbr_feeding_times; i++) {
        free(schedule[i].pet_name);
        free(schedule[i].meal_name);
    }
    free(schedule);
    print_heap_size("After freeing memory from schedule");
}

void get_next_feeding_time(time_t *next_time, uint8_t *feed_index, feeding_schedule_t *schedule, uint8_t nbr_feeding_times) {
    time_t current_time, feeding_time;
    current_time = time(&current_time);
    uint8_t hour, min, sec;
    time_t smallest_diff = (time_t)99999999, time_diff;
    char buffer[50];

    struct tm *utc_timeinfo = gmtime(&current_time);
    strftime(buffer, sizeof(buffer), "%A, %B %d %Y - %H:%M:%S ", utc_timeinfo);
    ESP_LOGI(TAG, "Now: %ld -- %s [UTC]", current_time, buffer);
    
    struct tm *timeinfo = localtime(&current_time);
    strftime(buffer, sizeof(buffer), "%A, %B %d %Y - %H:%M:%S ", timeinfo);
    ESP_LOGI(TAG, "Now: %ld -- %s [local]", current_time, buffer);
    *next_time = 0;

    for(uint8_t i=0; i < nbr_feeding_times; i++) {
        timeinfo = gmtime(&current_time);
        sscanf(schedule[i].feed_time_utc, "%d:%d:%d", (int *)&hour, (int *)&min, (int *)&sec);
        timeinfo->tm_hour = hour;
        timeinfo->tm_min = min;
        timeinfo->tm_sec = sec;
        feeding_time = utc_mktime(timeinfo);
        if (feeding_time < current_time) {
            ESP_LOGI(TAG, "Add one day");
            timeinfo->tm_mday++;
            feeding_time = utc_mktime(timeinfo);
        } 
        timeinfo = gmtime(&feeding_time);
        strftime(buffer, sizeof(buffer), "%A, %B %d %Y - %H:%M:%S ", timeinfo);
        ESP_LOGI(TAG, "[%d] %ld -- %ld -- %s", (int)i, feeding_time, feeding_time - current_time, buffer);
        time_diff = feeding_time - current_time;
        ESP_LOGI(TAG, "time_diff: %ld smallest_diff: %ld tm_wday: %d", time_diff, smallest_diff, timeinfo->tm_wday);
        if (time_diff <= smallest_diff && (feeding_dow[timeinfo->tm_wday] & schedule[i].dow)) {
            ESP_LOGI(TAG, "Is smaller, using this");
            *feed_index = (uint8_t)i;
            smallest_diff = time_diff;
            *next_time = feeding_time;
        }
    }
} 