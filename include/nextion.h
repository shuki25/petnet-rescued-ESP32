#ifndef PETNET_ESP32_NEXTION_H
#define PETNET_ESP32_NEXTION_H

#include <stdio.h>
#include <stdlib.h>

// Event Types
#define NEXTION_TOUCH               0x65  // Touch event
#define NEXTION_TOUCH_COORDINATE    0x67  // Touch coordinate
#define NEXTION_TOUCH_IN_SLEEP      0x68  // Touch event in sleep mode
#define NEXTION_AUTO_SLEEP          0x86  // Device automatically enters into sleep mode
#define NEXTION_AUTO_WAKE           0x87  // Device automatically wake up
#define NEXTION_STARTUP             0x88  // System successful start up
#define NEXTION_SD_CARD_UPGRADE     0x89  // Start SD card upgrade

// Return Codes
#define NEXTION_INVALID_INSTR           0x00
#define NEXTION_SUCCESS                 0x01             
#define NEXTION_INVALID_COMPONENT_ID    0x02             
#define NEXTION_INVALID_PAGE_ID         0x03             
#define NEXTION_INVALID_PICTURE_ID      0x04             
#define NEXTION_INVALID_FONT_ID         0x05             
#define NEXTION_INVALID_FILE_OP         0x06             
#define NEXTION_INVALID_CRC             0x09             
#define NEXTION_INVALID_BAUD_RATE       0x11             
#define NEXTION_INVALID_WAVEFORM_ID     0x12             
#define NEXTION_INVALID_VARIABLE        0x1A             
#define NEXTION_INVALID_VARIABLE_OP     0x1B             
#define NEXTION_FAIL_ASSIGNMENT         0x1C             
#define NEXTION_FAIL_EEPROM_OP          0x1D             
#define NEXTION_INVALID_NUM_PARAMS      0x1E             
#define NEXTION_FAIL_IO_OP              0x1F
#define NEXTION_INVALID_ESCAPE_CHAR     0x20
#define NEXTION_VARIABLE_NAME_TOO_LONG  0x23

// Response Types
#define NEXTION_NUMBER              0x71
#define NEXTION_STRING              0x70
#define NEXTION_PAGE                0x66
#define NEXTION_READY               0xFE
#define NEXTION_FINISHED            0xFD

// End of transmission
#define EOL                         "\xff\xff\xff"
#define EOL_PATTERN_CHAR            0xFF
#define PATTERN_LEN                 3
#define NEXTION_MAX_BUFFER_SIZE     1024

typedef struct {
    uint8_t event_code;     // Event Type Code
    uint8_t page;
    uint8_t component;
    uint8_t event;          // 0x01 Press and 0x00 Release
    uint16_t x_coordinate;  // 0x01 * 256 + 0x02 (big endian)
    uint16_t y_coordinate;  // 0x01 * 256 + 0x02 (big endian)
    uint32_t number;        // 0x01 + 0x02 * 256 + 0x03 * 65536 + 0x04 * 16777216 (little endian) 
    uint8_t *string;
} nextion_response_t;

typedef struct {
    uint32_t number;
    uint8_t *string;
} nextion_payload_t;

// Error Constants
#define NEXTION_OK      0x01
#define NEXTION_FAIL    0x00
typedef uint8_t nextion_err_t;

uint8_t get_packet_length(uint8_t packet_code);
nextion_err_t send_command(uart_port_t uart_port, char *command, nextion_response_t *response);
uint8_t parse_event(uint8_t *event_packet, uint16_t packet_size, nextion_response_t *response);
nextion_err_t get_value(uart_port_t uart_port, char *key, nextion_response_t *response);
nextion_err_t set_value(uart_port_t uart_port, char *key, nextion_payload_t *payload, nextion_response_t *response);

#endif