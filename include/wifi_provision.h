#ifndef PETNET_ESP32_WIFI_PROVISION_H
#define PETNET_ESP32_WIFI_PROVISION_H

#include "esp_wifi_types.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

typedef struct {
    uint8_t state;
    uint8_t mac[6];
    esp_netif_ip_info_t netif_info;
    wifi_ap_record_t ap_info;
} wifi_info_t;

extern EventGroupHandle_t s_wifi_event_group;
extern esp_err_t wifi_status;
extern uint8_t force_reprovisioning;

esp_err_t wifi_provisioning(wifi_info_t *wifi_info);
void smartconfig_task(void * parm);

#endif