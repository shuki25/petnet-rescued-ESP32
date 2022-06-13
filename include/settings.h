#ifndef PETNET_ESP32_SETTINGS_H
#define PETNET_ESP32_SETTINGS_H

#define SETTINGS_VERSION 0x02

typedef struct {
    uint8_t setting_version;
    char device_id[24];
    char ssid[32];
    char password[64];
    char api_key[48];
    char device_key[48];
    char secret[16];
    float manual_feed_amount;
    char unused[4];
    time_t datetime_registered;
    time_t datetime_last_boot;
    uint8_t is_registered;
    uint8_t is_setup_done;
    uint8_t is_24h_mode;
    uint8_t is_notification_on;
    uint8_t is_manual_feeding_on;
    uint8_t manual_feeding_motor_ticks;
    char tz[32];
    char firmware_version[15];
    char future_use[32];
} petnet_rescued_settings_t;

typedef struct {
    uint8_t setting_version;
    char device_id[24];
    char ssid[32];
    char password[64];
    char api_key[48];
    char device_key[48];
    char secret[16];
    char unused[8];
    time_t datetime_registered;
    time_t datetime_last_boot;
    uint8_t is_registered;
    uint8_t is_setup_done;
    uint8_t is_24h_mode;
    uint8_t is_notification_on;
    uint8_t is_manual_feeding_on;
    uint8_t manual_feeding_motor_ticks;
    char tz[32];
    char firmware_version[15];
    char future_use[32];
} petnet_rescued_settings_t_v1;

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
} petnet_rescued_settings_t_v0;

esp_err_t save_settings_to_nvs();
esp_err_t check_upgrade_settings(petnet_rescued_settings_t *settings, void *old_data, size_t size);
void print_settings(petnet_rescued_settings_t *settings);
void get_settings_from_server();

#endif