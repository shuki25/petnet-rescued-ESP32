#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "config.h"
#include "main.h"
#include "nextion.h"

#define TAG "nextion.c"

// Nextion response packet sizes

static uint8_t PACKET_LENGTH_MAP[15][2] = {
    {0x00, 6},  // Nextion Startup
    {0x01, 4},  // Success
    {0x1A, 4},  // Invalid Variable
    {0x24, 4},  // Serial Buffer Overflow
    {0x65, 7},  // Touch Event
    {0x66, 5},  // Current Page Number
    {0x67, 9},  // Touch Coordinate(awake)
    {0x68, 9},  // Touch Coordinate(sleep)
    {0x71, 8},  // Numeric Data Enclosed
    {0x86, 4},  // Auto Entered Sleep Mode
    {0x87, 4},  // Auto Wake from Sleep
    {0x88, 4},  // Nextion Ready
    {0x89, 4},  // Start microSD Upgrade
    {0xFD, 4},  // Transparent Data Finished
    {0xFE, 4}   // Transparent Data Ready
};

uint8_t get_packet_length(uint8_t packet_code) {
    uint8_t num_packet_codes = sizeof(PACKET_LENGTH_MAP) / sizeof(PACKET_LENGTH_MAP[0]);
    uint8_t length = 0;

    for (int i = 0; i < num_packet_codes; i++) {
        if (PACKET_LENGTH_MAP[i][0] == packet_code) {
            length = PACKET_LENGTH_MAP[i][1];
            ESP_LOGI(TAG, "Packet code: %x, Packet length: %d", packet_code, length);
        }
    }
    return length;
}

nextion_err_t send_command(uart_port_t uart_port, char *command, nextion_response_t *response) {

    char incoming_msg[RX_BUFFER_SIZE];
    uart_event_t uart_event;
    char *prepared_command = malloc(strlen(command) + 4);
    sprintf(prepared_command, "%s%s", command, EOL);
    esp_err_t status = NEXTION_FAIL;

    // Initialize Response
    if (response->string != NULL) {
        free(response->string);
    }
    memset(response, 0, sizeof(nextion_response_t));

    // Acquire mutex lock
    if (xSemaphoreTake(nextion_mutex, 1000 / portTICK_PERIOD_MS ) == pdTRUE) {
        ESP_LOGI(TAG, "Mutex lock acquired");
        ESP_LOGI(TAG, "Sending command to UART 1: %s", prepared_command);
    
        uart_write_bytes(uart_port, prepared_command, strlen(prepared_command));

        // Wait for response
        memset(incoming_msg, 0, RX_BUFFER_SIZE);
        
        for (int i=0; i < 10; i++) {
            ESP_LOGI(TAG, "Awaiting response... #%d", i);
            if (xQueueReceive(uart_queue, &uart_event, 500 / portTICK_PERIOD_MS)) {
                switch (uart_event.type)
                {
                case UART_DATA:
                    ESP_LOGI(TAG, "UART_DATA");
                    uart_read_bytes(UART_NUM_1, (uint8_t *)incoming_msg, uart_event.size, pdMS_TO_TICKS(100));
                    ESP_LOG_BUFFER_HEXDUMP(TAG, incoming_msg, uart_event.size, ESP_LOG_INFO);
                    ESP_LOGI(TAG, "Received data: %.*s", uart_event.size, incoming_msg);
                    break;
                case UART_BREAK:
                    ESP_LOGI(TAG, "UART_BREAK");
                    break;      
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "UART_BUFFER_FULL");
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "UART_FIFO_OVF");
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(uart_queue);
                    break;   
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "UART_FRAME_ERR");
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(uart_queue);
                    break;
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "UART_PATTERN_DET");
                    uart_read_bytes(UART_NUM_1, (uint8_t *)incoming_msg, uart_event.size, pdMS_TO_TICKS(100));
                    if (incoming_msg[0]) {
                        parse_event((uint8_t *)incoming_msg, uart_event.size, response);
                        ESP_LOG_BUFFER_HEXDUMP(TAG, incoming_msg, 32, ESP_LOG_INFO);
                        i = 255;
                        break;
                    }
                default:
                    break;
                }
            }
        }

        // Release mutex lock
        xSemaphoreGive(nextion_mutex);
        ESP_LOGI(TAG, "Mutex lock released");
        status = NEXTION_OK;
    } else {
        ESP_LOGW(TAG, "Acquiring Mutex Timed-Out. Failed sending command: %s", command);
    }
    
    free(prepared_command);
    return status;
}

nextion_err_t get_value(uart_port_t uart_port, char *key, nextion_response_t *response) {

    char incoming_msg[RX_BUFFER_SIZE];
    uart_event_t uart_event;
    char *prepared_command = malloc(strlen(key) + 8);
    sprintf(prepared_command, "get %s%s", key, EOL);
    esp_err_t status = NEXTION_FAIL;

    nextion_response_t tmp_response;
    nextion_payload_t tmp_payload;

    memset(incoming_msg, 0, RX_BUFFER_SIZE);
    memset(&tmp_response, 0, sizeof(nextion_response_t));
    memset(&tmp_payload, 0, sizeof(nextion_payload_t));
    
    // Wake up Nextion if it is sleeping
    if (is_nextion_sleeping) {
        send_command(UART_NUM_1, "sleep=0", &tmp_response);
        reset_data(incoming_msg, &tmp_payload, &tmp_response);
    }

    // Initialize Response
    if (response->string != NULL) {
        free(response->string);
    }
    memset(response, 0, sizeof(nextion_response_t));

    // Acquire mutex lock
    if (xSemaphoreTake(nextion_mutex, 1000 / portTICK_PERIOD_MS ) == pdTRUE) {
        ESP_LOGI(TAG, "Mutex lock acquired");
        ESP_LOGI(TAG, "Sending command to UART 1: %s", prepared_command);
        // uart_disable_pattern_det_intr(UART_NUM_1);
        uart_write_bytes(uart_port, prepared_command, strlen(prepared_command));

        // Wait for response
        memset(incoming_msg, 0, RX_BUFFER_SIZE);
        
        for (int i=0; i < 10; i++) {
            ESP_LOGI(TAG, "Awaiting response... #%d", i);
            if (xQueueReceive(uart_queue, &uart_event, 500 / portTICK_PERIOD_MS)) {
                switch (uart_event.type)
                {
                case UART_DATA:
                    ESP_LOGI(TAG, "UART_DATA");
                    uart_read_bytes(UART_NUM_1, (uint8_t *)incoming_msg, uart_event.size, pdMS_TO_TICKS(100));
                    ESP_LOG_BUFFER_HEXDUMP(TAG, incoming_msg, uart_event.size, ESP_LOG_INFO);
                    ESP_LOGI(TAG, "Received data: %.*s", uart_event.size, incoming_msg);
                    break;
                case UART_BREAK:
                    ESP_LOGI(TAG, "UART_BREAK");
                    break;      
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "UART_BUFFER_FULL");
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "UART_FIFO_OVF");
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(uart_queue);
                    break;   
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "UART_FRAME_ERR");
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(uart_queue);
                    break;
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "UART_PATTERN_DET");
                    uart_read_bytes(UART_NUM_1, (uint8_t *)incoming_msg, uart_event.size, pdMS_TO_TICKS(100));
                    if (incoming_msg[0]) {
                        parse_event((uint8_t *)incoming_msg, uart_event.size, response);
                        ESP_LOG_BUFFER_HEXDUMP(TAG, incoming_msg, 32, ESP_LOG_INFO);
                        i = 255;
                        break;
                    }
                default:
                    break;
                }
            }
        }

        // Release mutex lock
        xSemaphoreGive(nextion_mutex);
        ESP_LOGI(TAG, "Mutex lock released");
        if (response->event_code == NEXTION_SUCCESS || response->event_code == NEXTION_STRING ||
            response->event_code == NEXTION_NUMBER || response->event_code == NEXTION_PAGE) {
            status = NEXTION_OK;
        } 
    } else {
        ESP_LOGW(TAG, "Acquiring Mutex Timed-Out. Failed getting value of variable  '%s'", key);
    }
    
    free(prepared_command);
    return status;
}

nextion_err_t set_value(uart_port_t uart_port, char *key, nextion_payload_t *payload, nextion_response_t *response) {

    char incoming_msg[RX_BUFFER_SIZE];
    uart_event_t uart_event;
    char *prepared_command = NULL;
    uint8_t num_digits=0;
    nextion_response_t tmp_response;
    nextion_payload_t tmp_payload;

    memset(incoming_msg, 0, RX_BUFFER_SIZE);
    memset(&tmp_response, 0, sizeof(nextion_response_t));
    memset(&tmp_payload, 0, sizeof(nextion_payload_t));

    // Wake up Nextion if it is sleeping
    if (is_nextion_sleeping) {
        send_command(UART_NUM_1, "sleep=0", &tmp_response);
        reset_data(incoming_msg, &tmp_payload, &tmp_response);
    }

    // Initialize Response
    if (response->string != NULL) {
        free(response->string);
    }
    memset(response, 0, sizeof(nextion_response_t));

    // Preparing Nextion set value payload

    if (payload->string != NULL)
    {
        prepared_command = malloc(strlen(key) + strlen((char *)payload->string) + 9);
        sprintf(prepared_command, "%s=\"%s\"%s", key, payload->string, EOL);
    }
    else {
        if(payload->number > 0) {
            num_digits = log10(payload->number);
        } else {
            num_digits = 1;
        }
        prepared_command = malloc(strlen(key) + num_digits + 7);
        if(prepared_command != NULL) {
            sprintf(prepared_command, "%s=%d%s", key, payload->number, EOL);
        } else {
            ESP_LOGE(TAG, "malloc failed.");
        }
        
    }    
    
    esp_err_t status = NEXTION_FAIL;

    // Acquire mutex lock
    if (xSemaphoreTake(nextion_mutex, 1000 / portTICK_PERIOD_MS ) == pdTRUE) {
        ESP_LOGI(TAG, "Mutex lock acquired");
        ESP_LOGI(TAG, "Sending command to UART 1: %s", prepared_command);

        uart_write_bytes(uart_port, prepared_command, strlen(prepared_command));

        // Wait for response
        memset(incoming_msg, 0, RX_BUFFER_SIZE);
        
        for (int i=0; i < 10; i++) {
            ESP_LOGI(TAG, "Awaiting response... #%d", i);
            if (xQueueReceive(uart_queue, &uart_event, 500 / portTICK_PERIOD_MS)) {
                switch (uart_event.type)
                {
                case UART_DATA:
                    ESP_LOGI(TAG, "UART_DATA");
                    uart_read_bytes(UART_NUM_1, (uint8_t *)incoming_msg, uart_event.size, pdMS_TO_TICKS(100));
                    ESP_LOG_BUFFER_HEXDUMP(TAG, incoming_msg, uart_event.size, ESP_LOG_INFO);
                    ESP_LOGI(TAG, "Received data: %.*s", uart_event.size, incoming_msg);
                    break;
                case UART_BREAK:
                    ESP_LOGI(TAG, "UART_BREAK");
                    break;      
                case UART_BUFFER_FULL:
                    ESP_LOGI(TAG, "UART_BUFFER_FULL");
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGI(TAG, "UART_FIFO_OVF");
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(uart_queue);
                    break;   
                case UART_FRAME_ERR:
                    ESP_LOGI(TAG, "UART_FRAME_ERR");
                    uart_flush_input(UART_NUM_1);
                    xQueueReset(uart_queue);
                    break;
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "UART_PATTERN_DET");
                    uart_read_bytes(UART_NUM_1, (uint8_t *)incoming_msg, uart_event.size, pdMS_TO_TICKS(100));
                    if (incoming_msg[0]) {
                        parse_event((uint8_t *)incoming_msg, uart_event.size, response);
                        ESP_LOG_BUFFER_HEXDUMP(TAG, incoming_msg, 32, ESP_LOG_INFO);
                        i = 255;
                        break;
                    }
                default:
                    break;
                }
            }
        }

        // Release mutex lock
        xSemaphoreGive(nextion_mutex);
        ESP_LOGI(TAG, "Mutex lock released");
        if (response->event_code == NEXTION_SUCCESS) {
            status = NEXTION_OK;
        } 
    } else {
        ESP_LOGW(TAG, "Acquiring Mutex Timed-Out. Failed getting value of variable  '%s'", key);
    }
    
    free(prepared_command);
    return status;
}

uint8_t parse_event(uint8_t *event_packet, uint16_t packet_size, nextion_response_t *response) {
    uint8_t event_type = event_packet[0];
    response->event_code = event_type;

    switch (event_type)
    {
    case NEXTION_TOUCH:
        ESP_LOGI(TAG, "Event triggered: NEXTION_TOUCH");
        response->page = event_packet[1];
        response->component = event_packet[2];
        response->event = event_packet[3];
        break;           
    case NEXTION_TOUCH_COORDINATE:
        ESP_LOGI(TAG, "Event triggered: NEXTION_TOUCH_COORDINATE");
        response->event = event_packet[5];
        response->x_coordinate = event_packet[1] * 256 + event_packet[2];
        response->y_coordinate = event_packet[3] * 256 + event_packet[4];
        break;
    case NEXTION_TOUCH_IN_SLEEP:
        ESP_LOGI(TAG, "Event triggered: NEXTION_TOUCH_IN_SLEEP");
        response->event = event_packet[5];
        response->x_coordinate = event_packet[1] * 256 + event_packet[2];
        response->y_coordinate = event_packet[3] * 256 + event_packet[4];
        break;
    case NEXTION_AUTO_SLEEP:
        ESP_LOGI(TAG, "Event triggered: NEXTION_AUTO_SLEEP");
        is_nextion_sleeping = true;
        break;      
    case NEXTION_AUTO_WAKE:
        ESP_LOGI(TAG, "Event triggered: NEXTION_AUTO_WAKE");
        is_nextion_sleeping = false;
        break;       
    case NEXTION_STARTUP:
        ESP_LOGI(TAG, "Event triggered: NEXTION_STARTUP");
        break;         
    case NEXTION_SD_CARD_UPGRADE:
        ESP_LOGI(TAG, "Event triggered: NEXTION_SD_CARD_UPGRADE");
        break;
    case NEXTION_READY:
        ESP_LOGI(TAG, "Event triggered: NEXTION_READY");
        break; 
    case NEXTION_FINISHED:
        ESP_LOGI(TAG, "Event triggered: NEXTION_FINISHED");
        break;
    case NEXTION_NUMBER:    // 0x01 + 0x02 * 256 + 0x03 * 65536 + 0x04 * 16777216
        ESP_LOGI(TAG, "Event triggered: NEXTION_NUMBER");
        response->number = event_packet[1] + event_packet[2] * 256 +  event_packet[3] * 65536 +  event_packet[4] * 16777216;
        break;          
    case NEXTION_STRING:
        ESP_LOGI(TAG, "Event triggered: NEXTION_STRING");
        response->string = malloc(sizeof(char) * (packet_size - 3));
        strlcpy((char *)response->string, (char *)event_packet+1, packet_size-3);
        break;          
    case NEXTION_PAGE:
        ESP_LOGI(TAG, "Event triggered: NEXTION_PAGE");
        break;            
    case NEXTION_INVALID_VARIABLE:
        ESP_LOGI(TAG, "Event triggered: NEXTION_INVALID_VARIABLE");
        break;
    case NEXTION_SUCCESS:
        ESP_LOGI(TAG, "Event triggered: NEXTION_SUCCESS");
        break;         
    default:
        ESP_LOGI(TAG, "Other event: %0x", event_type);
        break;
    }
    return event_type;
}

nextion_err_t sync_nextion_clock(uart_port_t uart_port, struct tm *timeinfo) {
    nextion_payload_t payload;
    nextion_response_t response;

    memset(&payload, 0, sizeof(nextion_payload_t));
    memset(&response, 0, sizeof(nextion_response_t));

    payload.number = timeinfo->tm_year  + 1900;
    set_value(uart_port, "rtc0", &payload, &response);
    payload.number = timeinfo->tm_mon + 1;
    set_value(uart_port, "rtc1", &payload, &response);
    payload.number = timeinfo->tm_mday;
    set_value(uart_port, "rtc2", &payload, &response);
    payload.number = timeinfo->tm_hour;
    set_value(uart_port, "rtc3", &payload, &response);
    payload.number = timeinfo->tm_min;
    set_value(uart_port, "rtc4", &payload, &response);
    payload.number = timeinfo->tm_sec;
    set_value(uart_port, "rtc5", &payload, &response);

    return NEXTION_OK;
}

void reset_data(char *buffer, nextion_payload_t *payload, nextion_response_t *response) {
    // clear buffer and payload struct
    if (payload->string != NULL) {
        free(payload->string);
    }
    if (response->string != NULL) {
        free(response->string);
    }

    memset(buffer, 0, RX_BUFFER_SIZE);
    memset(response, 0, sizeof(nextion_response_t));
    memset(payload, 0, sizeof(nextion_payload_t));
}

nextion_err_t initialize_nextion_connection(uart_port_t uart_port) {
    char buffer[RX_BUFFER_SIZE];
    nextion_response_t response;
    nextion_payload_t payload;
    nextion_err_t status;

    // send_command(UART_NUM_1, "bkcmd=3", &response);
    payload.number=3;
    payload.string=NULL;
    status = set_value(uart_port, "bkcmd", &payload, &response);
    if (status == NEXTION_OK) {
        ESP_LOGI(TAG, "Set bkcmd to 3");
        ESP_LOGI(TAG, "Nextion display is connected.");
    }
    else {
        ESP_LOGE(TAG, "setting value bkcmd=3 failed. Code: %0x", response.event_code);
        ESP_LOGI(TAG, "Nextion display is not connected, continuing without the display.");
    }
    reset_data(buffer, &payload, &response);

    return status;
}