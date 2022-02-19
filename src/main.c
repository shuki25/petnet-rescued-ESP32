#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "config.h"
#include "sdkconfig.h"
#include "wifi_provision.h"
#include "api_client.h"
#include "main.h"
#include "json_util.h"
#include "util.h"
#include "nextion.h"

#define TAG    "main_app"

static xQueueHandle gpio_evt_queue = NULL;
QueueHandle_t uart_queue;
xSemaphoreHandle nextion_mutex = NULL;

static input_state_t hopper_state;
static input_state_t button_state;
static wifi_info_t wifi_info;
static petnet_rescued_settings_t petnet_settings;
int heap_size_start, heap_size_end;


static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_handler(void *arg) {
    uint32_t io_num;
    uint8_t current_state;
    hopper_state.counter=0;
    hopper_state.state=0;
    button_state.counter=0;
    button_state.state=1;
    uint8_t flag_wifi=0;

    while(true) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            current_state = gpio_get_level(io_num);
            if(io_num == MOTOR_SNSR && current_state != hopper_state.state) {
                hopper_state.counter++;
                hopper_state.state = current_state;
                printf("hopper_counter: %d\n", hopper_state.counter);
            }
            else if(io_num == BUTTON && current_state != button_state.state) {
                gpio_set_level(ONBOARD_LED, !current_state);
                if(current_state == 1) {
                    button_state.counter++;
                    printf("button count: %d\n", button_state.counter);
                    print_heap_size("intsig");
                    flag_wifi = !flag_wifi;
                    wifi_info.state = flag_wifi;
                    ESP_LOGI(TAG, "flag_wifi: %d", flag_wifi);
                }
                button_state.state = current_state;
            }
            // else {
            //     printf("GPIO[%d] intr, val: %d\n", io_num, current_state);
            // }
        }
    }
}

static void initialize() {
    char *content, *value;
    cJSON *payload, *results;

    petnet_settings.is_24h_mode = true;
    
    // Get settings
    api_get(&content, API_KEY, "/settings/");
    
    ESP_LOGD(TAG, "Content-length: %d - Hex Dump", strlen(content));
    ESP_LOG_BUFFER_HEXDUMP(TAG, content, strlen(content), ESP_LOG_DEBUG);
    
    print_heap_size("After API call");
    
    payload = cJSON_Parse(content);
    print_heap_size("After cJSON_Parse");
    if (payload != NULL) {
        results = cJSON_GetObjectItem(payload, "results");
        value = fetch_json_value(results, "is_setup_done");
        if (value) {
            petnet_settings.is_setup_done = atoi(value);
        }
        value = fetch_json_value(results, "timezone");
        if (value) {
            strcpy(petnet_settings.tz, value);
        }
        value = fetch_json_value(results, "test");
        ESP_LOGI(TAG, "test = '%s'", value ? value : "");
        ESP_LOGI(TAG, "is_setup_done = %d, tz = %s", petnet_settings.is_setup_done, petnet_settings.tz);
    } else {
        ESP_LOGI(TAG, "Error with JSON");
    }
    print_heap_size("Before cJSON_Delete");
    cJSON_Delete(payload);
    print_heap_size("After cJSON_Delete");
    free(content);
    print_heap_size("After free content");

    api_get(&content, API_KEY, "/feeding-time/");

    ESP_LOGD(TAG, "Content-length: %d - Hex Dump", strlen(content));
    ESP_LOG_BUFFER_HEXDUMP(TAG, content, strlen(content), ESP_LOG_DEBUG);

    free(content);
}

static void blinky_task(void *data) {
    led_config_t led = *(led_config_t *) data;

    while(true) {
        vTaskDelay(led.delay / portTICK_PERIOD_MS);
        led.state = !led.state;
        gpio_set_level(led.io_num, led.state);
    }
}

static void wifi_led(void *data) {
    led_config_t led = *(led_config_t *) data;
    uint16_t loop_delay = led.delay;

    while(true) {
        if(wifi_info.state == 0) {
            led.state = 1;
            for(int i=0; i<4; i++, led.state = !led.state) {
                gpio_set_level(led.io_num, led.state);
                vTaskDelay((loop_delay/4) / portTICK_PERIOD_MS);
            }
            gpio_set_level(led.io_num, 0);
            vTaskDelay((loop_delay) / portTICK_PERIOD_MS);
        }
        else {
            gpio_set_level(led.io_num, led.state);
            vTaskDelay(loop_delay / portTICK_PERIOD_MS);
        }

    }
}

void nextion_monitor() {
    
    char incoming_msg[RX_BUFFER_SIZE];
    uart_event_t uart_event;
    size_t data_len;
    uint8_t event_code = 0;
    uint16_t loop_counter = 1;
    bool is_display_sleeping = false;
    float cpu_temp_max = 0.0, cpu_temp_min = 100.0;
    uint8_t temp_reading;
    nextion_response_t response;
    nextion_err_t err;
    nextion_payload_t payload;
    uint8_t wifi_connect = 0xff;

    while(true) {

        // One time setup
        if (loop_counter == 1) {
            // send_command(UART_NUM_1, "bkcmd=3", &response);
            payload.number=3;
            payload.string=NULL;
            if (set_value(UART_NUM_1, "bkcmd", &payload, &response) == NEXTION_OK) {
                ESP_LOGI(TAG, "Set bkcmd to 3");
            }
            else {
                ESP_LOGE(TAG, "setting value bkcmd=3 failed. Code: %0x", response.event_code);
            }
        }
        
        if (loop_counter == 2) {
            send_command(UART_NUM_1, "page page1", &response);
            ESP_LOGI(TAG, "Message sent!");
        }

        if (loop_counter == 3) {
            if (get_value(UART_NUM_1, "page0.flag_24h.val", &response) == NEXTION_FAIL) {
                ESP_LOGW(TAG, "Invalid Variable: %s", "flag_24h");
            } else {
                if (response.string != NULL) {
                    ESP_LOGI(TAG, "flag_24h string: '%s' or [number]: %d", response.string, response.number);
                } else {
                    ESP_LOGI(TAG, "flag_24h [number]: %d", response.number);
                }
            }
        }

        if (loop_counter % 30 == 0) {
            temp_reading = temprature_sens_read();
            ESP_LOGI(TAG, "ESP32 onchip temperature: %.1f\xb0 C", (temp_reading - 32) / 1.8);
        }
        

        if (loop_counter == 10) {
            if (get_value(UART_NUM_1, "page1.next_meal_name.txt", &response) == NEXTION_FAIL) {
                ESP_LOGW(TAG, "Invalid Variable: %s", "page1.next_meal_name.txt");
            } else {
                if (response.string != NULL) {
                    ESP_LOGI(TAG, "page1.next_meal_name.txt string: '%s' or [number]: %d", response.string, response.number);
                } else {
                    ESP_LOGI(TAG, "page1.next_meal_name.txt [number]: %d", response.number);
                }
            }
        }

        if (loop_counter % 120 == 0) {

        }

        if (loop_counter % 120 == 0) {
            ESP_LOGI(TAG, "loop counter: %d", loop_counter);
            send_command(UART_NUM_1, "page page1", &response);
        }

        if (loop_counter % 250 == 0) {
            ESP_LOGI(TAG, "loop counter: %d", loop_counter);
            send_command(UART_NUM_1, "page page8", &response);
        }

        // If Wifi connection status changes, update the wifi icon
        if (wifi_connect != wifi_info.state) {
            printf("\n\nBefore payload\nwinfi_info.state=%d", wifi_info.state);
            payload.number=(int)wifi_info.state;
            printf("\n\nAfter payload\n");
            if (set_value(UART_NUM_1, "page0.flag_wifi.val", &payload, &response) == NEXTION_OK) {
                wifi_connect = wifi_info.state;
            };
        } 

        if (response.string != NULL) {
            free(response.string);
        }
        if (payload.string != NULL) {
            free(payload.string);
        }
        memset(incoming_msg, 0, RX_BUFFER_SIZE);
        memset(&response, 0, sizeof(nextion_response_t));
        memset(&payload, 0, sizeof(nextion_payload_t));

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
                break;   
            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "UART_FRAME_ERR");
                break;  
            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "UART_PARITY_ERR");
                break; 
            case UART_DATA_BREAK:
                ESP_LOGI(TAG, "UART_DATA_BREAK");
                break; 
            case UART_PATTERN_DET:
                ESP_LOGI(TAG, "UART_PATTERN_DET");
                uart_read_bytes(UART_NUM_1, (uint8_t *)incoming_msg, uart_event.size, pdMS_TO_TICKS(100));
                if (incoming_msg[0]) {
                uint8_t event_code = parse_event((uint8_t *)incoming_msg, 32, &response);
                ESP_LOG_BUFFER_HEXDUMP(TAG, incoming_msg, 32, ESP_LOG_INFO);
                get_packet_length(incoming_msg[0]);
                ESP_LOGI(TAG, "code: 0x%0x, page: %d, component: %d, event: %d, x: %d, y: %d",
                    response.event_code,
                    response.page,
                    response.component,
                    response.event,
                    response.x_coordinate,
                    response.y_coordinate   
                );
            }
                break;
            default:
                break;  
            }
        }
        
        // Process Nextion Event

        switch (event_code)
        {
        case NEXTION_TOUCH:
            ESP_LOGI(TAG, "NEXTION_TOUCH");
            break;           
        case NEXTION_TOUCH_COORDINATE:
            ESP_LOGI(TAG, "NEXTION_TOUCH_COORDINATE");
            break;
        case NEXTION_TOUCH_IN_SLEEP:
            ESP_LOGI(TAG, "NEXTION_TOUCH_IN_SLEEP");
            break;  
        case NEXTION_AUTO_SLEEP:
            ESP_LOGI(TAG, "NEXTION_AUTO_SLEEP");
            break;      
        case NEXTION_AUTO_WAKE:
            ESP_LOGI(TAG, "NEXTION_AUTO_WAKE");
            break;       
        case NEXTION_STARTUP:
            ESP_LOGI(TAG, "NEXTION_STARTUP");
            break;         
        case NEXTION_SD_CARD_UPGRADE:
            ESP_LOGI(TAG, "NEXTION_SD_CARD_UPGRADE");
            break; 
        }

        // clear buffer and response struct

        if (response.string != NULL) {
            free(response.string);
        }
        if (payload.string != NULL) {
            free(payload.string);
        }

        memset(incoming_msg, 0, RX_BUFFER_SIZE);
        memset(&response, 0, sizeof(nextion_response_t));
        memset(&payload, 0, sizeof(nextion_payload_t));

        vTaskDelay(500 / portTICK_PERIOD_MS);
        loop_counter++;
    }
}

void app_main(void) {
    heap_size_start = esp_get_free_heap_size();
    print_heap_size("");

    char chip_id[24];
    get_chip_id(chip_id);
    ESP_LOGI(TAG, "Chip ID: %s", chip_id);

    // Initialize GPIO output config
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Initialize GPIO input config
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // Initialize GPIO button input config
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_BUTTON_SEL;
    gpio_config(&io_conf);

    // Set up LEDs
    led_config_t blue_led, red_led, green_led;
    blue_led.io_num = ONBOARD_LED;
    blue_led.delay = 1000;
    blue_led.state = 0;
    red_led.io_num = RED_LED;
    red_led.delay = 700;
    red_led.state = 0;
    green_led.io_num = GREEN_LED;
    green_led.delay = 350;
    green_led.state = 0;

    wifi_info.state = false;

    // Set up UART connection with Nextion
    uart_config_t uart_config = {
        .baud_rate = 115200,                   
        .data_bits = UART_DATA_8_BITS,    
        .parity = UART_PARITY_DISABLE,            
        .stop_bits = UART_STOP_BITS_1,      
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE 
    };

    // Delay to allow boot up to stablize
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Initialize NVS
    printf("Initializing NVS\n");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
    vTaskDelay(200 / portTICK_PERIOD_MS);

    xTaskCreate(wifi_led, "wifi_status_led", 1024, &blue_led, 10, NULL);
    print_heap_size("");

    ESP_LOGI(TAG, "Connecting to WiFi -- SSID: %s", WIFI_SSID);
    esp_err_t err = wifi_connect(&wifi_info, WIFI_SSID, WIFI_PASS);
    
    if(err == ESP_OK) {
        // char mac_addr[18];
        // bssid2mac(mac_addr, wifi_info.ap_info.bssid);
        ESP_LOGI(TAG, "WiFi Connected. AP MAC ID:" MACSTR "\n", MAC2STR(wifi_info.ap_info.bssid));
        ESP_LOGI(TAG, "Station IP:" IPSTR " MAC ID:" MACSTR "\n", IP2STR(&wifi_info.netif_info.ip), MAC2STR(wifi_info.mac));
    }
    
    print_heap_size("");

    list_AP();
    initialize();

    // Set up Semahpore Mutex
    nextion_mutex = xSemaphoreCreateMutex();
    if (nextion_mutex != NULL) {
        ESP_LOGI(TAG, "Created Semaphore Mutex Successfully.");
    }
    else {
        ESP_LOGE(TAG, "Create Semaphore Mutex Failed.");
    }
    
    // Activate UART with Nextion
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, RX_BUFFER_SIZE, TX_BUFFER_SIZE, 20, &uart_queue, 0));
    uart_enable_pattern_det_baud_intr(UART_NUM_1, EOL_PATTERN_CHAR, PATTERN_LEN, 9, 0, 0);
    uart_pattern_queue_reset(UART_NUM_1, 20);

    // Create a queue to handle isr event
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_handler, "gpio_task_handler", 4096, NULL, 10, NULL);

    // Install isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(MOTOR_SNSR, gpio_isr_handler, (void *) MOTOR_SNSR);
    gpio_isr_handler_add(BUTTON, gpio_isr_handler, (void *) BUTTON);

    print_heap_size("");

    xTaskCreate(blinky_task, "blink_task", 1024, &green_led, 10, NULL);
    xTaskCreate(blinky_task, "blink_task", 1024, &red_led, 10, NULL);
    xTaskCreate(nextion_monitor, "nextion_monitor", 8192, NULL, 10, NULL);
    print_heap_size("");

    while(true) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}