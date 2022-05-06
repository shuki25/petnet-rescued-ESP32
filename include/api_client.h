#ifndef PETNET_ESP32_API_CLIENT_H
#define PETNET_ESP32_API_CLIENT_H

#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 1024

esp_err_t client_event_handler(esp_http_client_event_t *event);
uint8_t api_get(char **content, char *auth_token, char *device_key, char *endpoint);
uint8_t api_post(char **content, char *auth_token, char *device_key, char *endpoint, char *post_data);

#endif