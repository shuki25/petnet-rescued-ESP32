#ifndef PETNET_ESP32_OTA_UPDATE_H
#define PETNET_ESP32_OTA_UPDATE_H

void ota_update_task();
void print_sha256 (const uint8_t *image_hash, const char *label);

#endif