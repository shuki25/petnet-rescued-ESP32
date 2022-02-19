#include "util.h"
#include "esp_system.h"

#define TAG "util"
extern int heap_size_start;

void print_heap_size(char *tag) {
    ESP_LOGD(TAG, "%s: Minimum free heap size: %d bytes, %d bytes used.", tag ? tag : "No Tag", esp_get_free_heap_size(), heap_size_start - esp_get_free_heap_size());
}

void get_chip_id(char *chip_identifier) {
    uint8_t mac[6] = {0};
    esp_efuse_mac_get_default(mac);
    sprintf(chip_identifier, "ESP32-%02x%02x-%02x%02x%02x%02x", MAC2STR(mac));
}