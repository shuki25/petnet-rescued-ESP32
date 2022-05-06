#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <search.h>

#include "config.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_log.h"

#define TAG "api_client"

extern const uint8_t cert[] asm("_binary_R3_cer_start");

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read

    char **data = evt->user_data;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (output_buffer == NULL) {
                    output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client)+1);
                    output_len = 0;
                    if (output_buffer == NULL) {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                int content_length = esp_http_client_get_content_length(evt->client);
                output_buffer[content_length] = '\0';
                *data = output_buffer;
                // ESP_LOGI(TAG, "user_data address: 0x%x", (unsigned int)*data);
                // ESP_LOGI(TAG, "user_data: %s", (char *)*data);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}


uint8_t api_get(char **content, char *auth_token, char *device_key, char *endpoint) {

    esp_err_t err;
    uint8_t status_code = (uint8_t)500;
    char url[256];
    char authorization[256];
    char *data;

    // ESP_LOGI(TAG, "address: 0x%x", (unsigned int)&content);

    sprintf(url, "%s%s", API_BASE_URL, endpoint);
    if (strlen(auth_token) > 1) {
        sprintf(authorization, "Token %s", auth_token);
    }
    ESP_LOGI(TAG, "API Endpoint: %s", url);

    esp_http_client_config_t client_config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = _http_event_handler,
        .user_data = &data,
        .disable_auto_redirect = true,
        .cert_pem = (char *)cert
    };

    // ESP_LOGI(TAG, "[before] data address: 0x%x", (unsigned int)data);

    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    if (strlen(auth_token) > 1) {
        ESP_ERROR_CHECK(esp_http_client_set_header(client, "Authorization", authorization));
    }
    if (strlen(auth_token) > 1) {
        ESP_ERROR_CHECK(esp_http_client_set_header(client, "X-Device-Key", device_key));
    }

    err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP STATUS CODE: %d", status_code);
        *content = data;
        // ESP_LOGD(TAG, "[after] content address: 0x%x", (unsigned int)*content);
        // ESP_LOGD(TAG, "*content:");
        // ESP_LOG_BUFFER_HEXDUMP(TAG, *content, 32, ESP_LOG_DEBUG);
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        *content = NULL;
    }
    // ESP_LOG_BUFFER_HEX(TAG, content, strlen(content));

    esp_http_client_cleanup(client);

    return status_code;
}

uint8_t api_post(char **content, char *auth_token, char *device_key, char *endpoint, char *post_data) {

    esp_err_t err;
    uint8_t status_code = (uint8_t)500;
    char url[256];
    char authorization[256];
    char *data;

    // ESP_LOGI(TAG, "address: 0x%x", (unsigned int)&content);

    sprintf(url, "%s%s", API_BASE_URL, endpoint);
    if (strlen(auth_token) > 1) {
        sprintf(authorization, "Token %s", auth_token);
    }
    ESP_LOGI(TAG, "API Endpoint: %s", url);

    esp_http_client_config_t client_config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = &data,
        .disable_auto_redirect = true,
        .cert_pem = (char *)cert
    };

    // ESP_LOGI(TAG, "[before] data address: 0x%x", (unsigned int)data);

    esp_http_client_handle_t client = esp_http_client_init(&client_config);
    if (strlen(auth_token) > 1) {
        ESP_ERROR_CHECK(esp_http_client_set_header(client, "Authorization", authorization));
    }
    if (strlen(auth_token) > 1) {
        ESP_ERROR_CHECK(esp_http_client_set_header(client, "X-Device-Key", device_key));
    }
    if (strlen(post_data) > 1) {
        ESP_ERROR_CHECK(esp_http_client_set_post_field(client, post_data, strlen(post_data)));
    }

    err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP STATUS CODE: %d", status_code);
        *content = data;
        // ESP_LOGD(TAG, "[after] content address: 0x%x", (unsigned int)*content);
        // ESP_LOGD(TAG, "*content:");
        // ESP_LOG_BUFFER_HEXDUMP(TAG, *content, 32, ESP_LOG_DEBUG);
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        *content = NULL;
    }
    // ESP_LOG_BUFFER_HEX(TAG, content, strlen(content));

    esp_http_client_cleanup(client);

    return status_code;
}