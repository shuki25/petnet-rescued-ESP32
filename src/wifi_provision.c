#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"

#include "wifi_provision.h"
#include "sdkconfig.h"
#include "config.h"

#define MAX_APs 20
#define SCAN_TAG "wifi provision"

static uint16_t s_retry_num = 0;

EventGroupHandle_t s_wifi_event_group;
esp_err_t wifi_status = ESP_FAIL;

static char *get_auth_mode_name(wifi_auth_mode_t auth_mode)
{
    char *names[] = {"OPEN", "WEP", "WPA-PSK", "WPA2-PSK", "WPA-WPA2-PSK", "WPA2-Enterprise", "WPA3-PSK", "WPA2-WPA3-PSK", "WAPI-PSK", "MAX"};
    return names[auth_mode];
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    ESP_LOGI(SCAN_TAG, "In event_handler");
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(SCAN_TAG, "event_sta_start");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(SCAN_TAG, "retrying connection");
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(SCAN_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(SCAN_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(SCAN_TAG, "got ip address");
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(SCAN_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_status = ESP_OK;
    }
}


void list_AP() {
    ESP_LOGI(SCAN_TAG, "Scanning for AP");

    uint16_t max_size = MAX_APs;
    wifi_ap_record_t ap_list[MAX_APs];
    uint16_t ap_count = 0;
    memset(ap_list, 0, sizeof(ap_list));

    wifi_scan_config_t scan_config = {
        .ssid = 0,   
        .bssid = 0, 
        .channel = 0, 
        .show_hidden = true
    };

    esp_wifi_scan_start(&scan_config, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_size, ap_list));
    ESP_LOGI(SCAN_TAG, "Total AP Scanned: %u", ap_count);

    printf("\n");
    printf("               SSID              | Channel | RSSI |   Auth Mode \n");
    printf("----------------------------------------------------------------\n");
    for (int i = 0; (i < MAX_APs) && (i < ap_count); i++) 
        printf("%32s | %7d | %4d | %12s\n", (char *)ap_list[i].ssid, ap_list[i].primary, ap_list[i].rssi, get_auth_mode_name(ap_list[i].authmode));
    printf("----------------------------------------------------------------\n");
}

void bssid2mac(char *mac_addr, uint8_t *bssid) {
    char tmp[18];
    sprintf(tmp, "%x:%x:%x:%x:%x:%x",bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    memcpy(mac_addr, tmp, strlen(tmp));
}

esp_err_t wifi_connect(wifi_info_t *wifi_info, char *ssid, char *password) {

    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); 
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    // wifi_config_t wifi_config;

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = ""
        }
    };
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(SCAN_TAG, "wifi_init_sta completed.");

     /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    printf("Passed Event group wait\n");
    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(SCAN_TAG, "connected to ap SSID:%s password:%s", ssid, password);
        esp_netif_get_ip_info(sta_netif, &wifi_info->netif_info);
        esp_wifi_sta_get_ap_info(&wifi_info->ap_info);
        esp_netif_get_mac(sta_netif, wifi_info->mac);
        wifi_info->state = true;
        // wifi_info->bssid = wifi_config.bssid;
        // wifi_info->ssid = wifi_config.;
        // wifi_info->primary = wifi_config.primary ;
        // wifi_info->rssi = wifi_config.rssi;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(SCAN_TAG, "Failed to connect to SSID:%s, password:%s", wifi_config.sta.ssid, wifi_config.sta.password);
        wifi_info->state = false;
    } else {
        ESP_LOGE(SCAN_TAG, "UNEXPECTED EVENT");
        wifi_info->state = false;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    if(wifi_info->state) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}