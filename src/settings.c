#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "cJSON.h"

#include "main.h"
#include "api_client.h"
#include "config.h"
#include "json_util.h"

#define TAG "settings.c"


esp_err_t save_settings_to_nvs() {
    nvs_handle_t eeprom_handle;

    ESP_ERROR_CHECK(nvs_open("nvs", NVS_READWRITE, &eeprom_handle));
    esp_err_t rs = nvs_set_blob(eeprom_handle, "settings", &petnet_settings, sizeof(petnet_rescued_settings_t));
    
    if (rs == ESP_OK) {
        ESP_ERROR_CHECK(nvs_commit(eeprom_handle));
        ESP_LOGI(TAG, "Settings saved to NVS.");
    } else {
        ESP_LOGI(TAG, "Settings not saved to NVS. Error: %s", esp_err_to_name(rs));
    }
    
    nvs_close(eeprom_handle);
    return rs;
}

esp_err_t check_upgrade_settings(petnet_rescued_settings_t *settings, void *old_data, size_t size) {
    petnet_rescued_settings_t_v0 *settings_v0 = (petnet_rescued_settings_t_v0 *)old_data;
    petnet_rescued_settings_t_v1 *settings_v1 = (petnet_rescued_settings_t_v1 *)old_data;
    char *data = (char *)old_data;
    esp_err_t rs = ESP_OK;
    
    if (data[0] == SETTINGS_VERSION) {
        ESP_LOGI(TAG, "Settings are already up to date.");
        return ESP_OK;
    } else {
        ESP_LOGI(TAG, "Settings are from an older version. Converting...");
        ESP_LOGI(TAG, "Current Settings Blob");
        switch(data[0]) {
            case 0x00:
                ESP_LOG_BUFFER_HEXDUMP(TAG, old_data, sizeof(petnet_rescued_settings_t_v0), ESP_LOG_INFO);
                memset(settings, 0, sizeof(petnet_rescued_settings_t));
                memcpy(settings, data, size);
                memset(settings->unused, 0, sizeof(settings->unused));
                memcpy(settings->firmware_version, settings_v0->firmware_version, sizeof(settings_v0->firmware_version));
                memcpy(settings->tz, settings_v0->tz, sizeof(settings_v0->tz));
                settings->setting_version = 0x02;
                settings->is_manual_feeding_on = false;
                settings->manual_feeding_motor_ticks = 5;
                settings->manual_feed_amount = 0.25;
                rs = ESP_ERR_NVS_NEW_VERSION_FOUND;
                break;
            case 0x01:
                ESP_LOG_BUFFER_HEXDUMP(TAG, old_data, sizeof(petnet_rescued_settings_t_v1), ESP_LOG_INFO);
                memset(settings, 0, sizeof(petnet_rescued_settings_t));
                memcpy(settings, data, size);
                memset(settings->unused, 0, sizeof(settings->unused));
                memcpy(settings->firmware_version, settings_v1->firmware_version, sizeof(settings_v1->firmware_version));
                memcpy(settings->tz, settings_v1->tz, sizeof(settings_v1->tz));
                settings->setting_version = 0x02;
                settings->is_manual_feeding_on = false;
                settings->manual_feeding_motor_ticks = 5;
                settings->manual_feed_amount = 0.25;
                rs = ESP_ERR_NVS_NEW_VERSION_FOUND;
                break;
            default:
                ESP_LOGI(TAG, "No matching settings version found.");
                break;
        } 
    }
    if (rs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Upgraded Settings Blob");
        ESP_LOG_BUFFER_HEXDUMP(TAG,(char *)&petnet_settings, sizeof(petnet_rescued_settings_t), ESP_LOG_INFO);
    }
    
    return rs;
}

void print_settings(petnet_rescued_settings_t *settings) {
    ESP_LOG_BUFFER_HEXDUMP(TAG,(char *)&petnet_settings, sizeof(petnet_rescued_settings_t), ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "Current Settings");
    ESP_LOGI(TAG, "Settings Version: %d", settings->setting_version);
    ESP_LOGI(TAG, "Device ID: %s", settings->device_id);
    ESP_LOGI(TAG, "SSID: %s", settings->ssid);
    ESP_LOGI(TAG, "Password: %s", settings->password);
    ESP_LOGI(TAG, "API Key: %s", settings->api_key);
    ESP_LOGI(TAG, "Device Key: %s", settings->device_key);
    ESP_LOGI(TAG, "Secret: %s", settings->secret);
    ESP_LOGI(TAG, "Firmware Version: %s", settings->firmware_version);
    ESP_LOGI(TAG, "Datetime Registered: %ld", settings->datetime_registered);
    ESP_LOGI(TAG, "Datetime Last Boot: %ld", settings->datetime_last_boot);
    ESP_LOGI(TAG, "Is Registered: %d", settings->is_registered);
    ESP_LOGI(TAG, "Is Setup Done: %d", settings->is_setup_done);
    ESP_LOGI(TAG, "Is 24h Mode: %d", settings->is_24h_mode);
    ESP_LOGI(TAG, "Is Notification On: %d", settings->is_notification_on);
    ESP_LOGI(TAG, "Is Manual Feeding On: %d", settings->is_manual_feeding_on);
    ESP_LOGI(TAG, "Is Manual Motor Ticks: %d", settings->manual_feeding_motor_ticks);
    ESP_LOGI(TAG, "Manual Feed Amount: %.2f", settings->manual_feed_amount);
    ESP_LOGI(TAG, "Timezone: %s", settings->tz);
}

void get_settings_from_server() {
    char *content, *value;
    uint16_t status_code = (uint16_t)500;
    cJSON *payload, *setting_payload, *results, *setting_data;
    payload = NULL;
    setting_payload = NULL;
    results = NULL;
    setting_data = NULL;

    // Get global settings
    status_code = api_get(&content, petnet_settings.api_key, petnet_settings.device_key, "/settings/");

    if (status_code == 200) {  
        ESP_LOGD(TAG, "Content-length: %d - Hex Dump", strlen(content));
        ESP_LOG_BUFFER_HEXDUMP(TAG, content, strlen(content), ESP_LOG_DEBUG);
        
        payload = cJSON_Parse(content);

        if (payload != NULL) {
            results = cJSON_GetObjectItem(payload, "results");
            value = fetch_json_value(results, "is_setup_done");
            if (value) {
                petnet_settings.is_setup_done = atoi(value);
            }
            value = fetch_json_value(results, "tz_esp32");
            if (value) {
                ESP_LOGI(TAG, "Old timezone: %s", petnet_settings.tz); 
                ESP_LOGI(TAG, "Setting timezone to %s", value);
                if (strcmp(value, petnet_settings.tz) != 0) {
                    ESP_LOGI(TAG, "Timezone changed, updating time");
                    strcpy(petnet_settings.tz, value);
                    tz_changed = true;
                }
            }
            ESP_LOGI(TAG, "is_setup_done = %d, tz = %s", petnet_settings.is_setup_done, petnet_settings.tz);
        } else {
            ESP_LOGE(TAG, "Error with JSON");
        }
        cJSON_Delete(payload);
    }
    else if (status_code >= 500) {
        ESP_LOGE(TAG, "Server error: %d", status_code);
        ESP_LOGE(TAG, "Using settings from NVS");
    }
    free(content);
    content = NULL;

    // Get device settings
    status_code = api_get(&content, petnet_settings.api_key, petnet_settings.device_key, "/device/settings/");
    if (status_code == 200) {  
        ESP_LOGD(TAG, "Content-length: %d - Hex Dump", strlen(content));
        ESP_LOG_BUFFER_HEXDUMP(TAG, content, strlen(content), ESP_LOG_DEBUG);
        
        payload = cJSON_Parse(content);

        if (payload != NULL) {
            setting_payload = cJSON_GetObjectItem(payload, "results");
            results = cJSON_GetArrayItem(setting_payload, 0);
            if(cJSON_IsObject(results)) {
                setting_payload = cJSON_GetObjectItem(results, "manual_motor_timing");
                if (cJSON_IsObject(setting_payload)) {
                    setting_data = cJSON_GetObjectItem(setting_payload, "feed_amount");
                    if (cJSON_IsNumber(setting_data)) {
                        petnet_settings.manual_feed_amount = (float)setting_data->valuedouble;
                    }
                    else {
                        ESP_LOGE(TAG, "Error with JSON");
                    }
                    setting_data = cJSON_GetObjectItem(setting_payload, "interrupter_count");
                    if (cJSON_IsNumber(setting_data)) {
                        petnet_settings.manual_feeding_motor_ticks = (uint8_t)setting_data->valueint;
                    }
                    else {
                        ESP_LOGE(TAG, "Error with JSON");
                    }
                }

                setting_data = cJSON_GetObjectItem(results, "manual_button");
                if (cJSON_IsBool(setting_data)) {
                    petnet_settings.is_manual_feeding_on = cJSON_IsTrue(setting_data);
                }
                else {
                    ESP_LOGE(TAG, "Error with JSON");
                }
            }
            ESP_LOGI(TAG, "is_setup_done = %d, tz = %s", petnet_settings.is_setup_done, petnet_settings.tz);
        } else {
            ESP_LOGE(TAG, "Error with JSON");
        }
        cJSON_Delete(payload);
    }
    else if (status_code >= 500) {
        ESP_LOGE(TAG, "Server error: %d", status_code);
        ESP_LOGE(TAG, "Using settings from NVS");
    }
    free(content);
    content = NULL;

    print_settings(&petnet_settings);
    save_settings_to_nvs();
}
