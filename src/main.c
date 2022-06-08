#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cJSON.h>
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/adc.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_event.h"
// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_pm.h"

#include "config.h"
#include "sdkconfig.h"
#include "wifi_provision.h"
#include "api_client.h"
#include "main.h"
#include "json_util.h"
#include "util.h"
#include "nextion.h"
#include "feeding.h"
#include "battery.h"
#include "event.h"
#include "logging.h"
#include "ota_update.h"


static const char *TAG = "main.c";

static xQueueHandle gpio_evt_queue = NULL;
QueueHandle_t uart_queue;
xSemaphoreHandle nextion_mutex = NULL;

static input_state_t hopper_state;
static input_state_t button_state;
static input_state_t food_detect_state;
static input_state_t power_state;
static wifi_info_t wifi_info;
petnet_rescued_settings_t petnet_settings;

int i2c_master_port = 0;
i2c_config_t i2c_config;

bool is_nextion_available = false;
bool is_nextion_sleeping = false;

int heap_size_start, heap_size_end;
bool is_clock_set = false;
feeding_schedule_t *feeding_schedule;
uint8_t num_feeding_times;
bool get_next_meal_slot = true;
bool tz_changed = false;
uint8_t red_blinky, green_blinky;



static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_handler(void *arg) {
    uint32_t io_num;
    uint8_t current_state;
    hopper_state.counter=0;
    // hopper_state.state=0;
    hopper_state.state = gpio_get_level(MOTOR_SNSR);
    button_state.counter=0;
    button_state.state=1;
    power_state.state=gpio_get_level(POWER_SNSR);
    power_state.counter=0;
    food_detect_state.state = gpio_get_level(HOPPER_SNSR);

    while(true) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            current_state = gpio_get_level(io_num);
            ESP_LOGI(TAG, "current_state: %d", current_state);
            if(io_num == MOTOR_SNSR && current_state != hopper_state.state) {
                hopper_state.counter++;
                hopper_state.state = current_state;
                ESP_LOGI(TAG, "hopper_counter: %d", hopper_state.counter);
            }
            else if(io_num == HOPPER_SNSR && current_state != food_detect_state.state) {
                food_detect_state.counter++;
                food_detect_state.state = current_state;
                ESP_LOGI(TAG, "food_detect: %d", current_state);
            }
            else if(io_num == BUTTON && current_state != button_state.state) {
                gpio_set_level(BLUE_LED, !current_state);
                if(current_state == 1) {
                    button_state.counter++;
                    ESP_LOGI(TAG, "button count: %d\n", button_state.counter);
                    ESP_LOGI(TAG, "red_led - active: %d", red_blinky);
                    red_blinky = !red_blinky;
                }
                button_state.state = current_state;
            }
            else if (io_num == POWER_SNSR && current_state != power_state.state) {
                power_state.counter++;
                power_state.state = current_state;
                ESP_LOGI(TAG, "power_state: %d", power_state.state);
            }
            // else {
            //     printf("GPIO[%d] intr, val: %d\n", io_num, current_state);
            // }
        }
    }
}

void ntp_callback(struct timeval *tv) {
    ESP_LOGI(TAG, "Secs: %ld", tv->tv_sec);
    struct tm *timeinfo;
    char buffer[50];

    setenv("TZ", "PLACEHOLDERSINCEITONLYALLOCATEONCE", 1);
    setenv("TZ", petnet_settings.tz, 1);
    tzset();

    time_t now = time(&now);
    struct tm *utc_timeinfo = gmtime(&now);
    strftime(buffer, sizeof(buffer), "%A, %B %d %Y - %H:%M:%S ", utc_timeinfo);
    ESP_LOGI(TAG, "Time in UTC: %s", buffer);

    timeinfo = localtime(&now);
    strftime(buffer, sizeof(buffer), "%A, %B %d %Y - %H:%M:%S ", timeinfo);
    ESP_LOGI(TAG, "Time in %s: %s", petnet_settings.tz, buffer);

    if (is_nextion_available) {
        sync_nextion_clock(UART_NUM_1, timeinfo);
    }
    tz_changed = false;
}


esp_err_t save_settings_to_nvs() {
    size_t nvs_size;

    nvs_handle_t eeprom_handle;

    ESP_ERROR_CHECK(nvs_open("nvs", NVS_READWRITE, &eeprom_handle));
    esp_err_t rs = nvs_set_blob(eeprom_handle, "settings", &petnet_settings, sizeof(petnet_rescued_settings_t));
    
    if (rs == ESP_OK) {
        ESP_ERROR_CHECK(nvs_commit(eeprom_handle));
        ESP_LOGI(TAG, "Settings saved to NVS.");
    } else {
        ESP_LOGI(TAG, "Settings not saved to NVS. Error: %s", esp_err_to_name(rs));
    }
    
    nvs_close(eeprom_handle);
    return rs;
}


// void reset_data(char *buffer, nextion_payload_t *payload, nextion_response_t *response) {
//     // clear buffer and payload struct
//     if (payload->string != NULL) {
//         free(payload->string);
//     }
//     if (response->string != NULL) {
//         free(response->string);
//     }

//     memset(buffer, 0, RX_BUFFER_SIZE);
//     memset(response, 0, sizeof(nextion_response_t));
//     memset(payload, 0, sizeof(nextion_payload_t));
// }

static bool initialize() {
    char *content, *value, *endpoint;
    uint8_t length;
    uint16_t status_code = (uint16_t)500, status = (uint16_t)404;
    cJSON *payload, *results;
    char buffer[RX_BUFFER_SIZE];
    nextion_response_t nextion_response;
    nextion_payload_t nextion_payload;
    bool is_registered = false, nextion_setup_page = false;

    memset(buffer, 0, RX_BUFFER_SIZE);
    memset(&nextion_payload, 0, sizeof(nextion_payload_t));
    memset(&nextion_response, 0, sizeof(nextion_response_t));

    // reset_data(buffer, &nextion_payload, &nextion_response);

    // Send Device ID and Activation Code to nextion
    if (is_nextion_available) {
        nextion_payload.string = malloc(sizeof(char) * strlen(petnet_settings.firmware_version) + 1);
        strcpy((char *)nextion_payload.string, petnet_settings.firmware_version);
        set_value(UART_NUM_1, "page0.feeder_fw.txt", &nextion_payload, &nextion_response);
        reset_data(buffer, &nextion_payload, &nextion_response);
        nextion_payload.string = malloc(sizeof(char) * strlen(petnet_settings.device_id) + 1);
        strcpy((char *)nextion_payload.string, petnet_settings.device_id);
        set_value(UART_NUM_1, "page8.device_id.txt", &nextion_payload, &nextion_response);
        reset_data(buffer, &nextion_payload, &nextion_response);
        nextion_payload.string = malloc(sizeof(char) * strlen(petnet_settings.secret) + 1);
        strcpy((char *)nextion_payload.string, petnet_settings.secret);
        set_value(UART_NUM_1, "page8.activate_code.txt", &nextion_payload, &nextion_response);
        reset_data(buffer, &nextion_payload, &nextion_response);
    }

    // Check if the device is registered

    length = 20 + strlen(petnet_settings.device_id) + strlen(petnet_settings.secret);
    endpoint = malloc(sizeof(char) * length+1);

    while(!is_registered) {
        
        sprintf(endpoint, "/device/verify/%s/%s/", petnet_settings.device_id, petnet_settings.secret);
        status_code = api_get(&content, "", "", endpoint);

        if (status_code == 200) {
            payload = cJSON_Parse(content);
            results = cJSON_GetObjectItem(payload, "status");
            
            if (cJSON_IsNumber(results)) {
                status = (uint8_t)results->valueint;
            }

            if (status == 200) {
                results = cJSON_GetObjectItem(payload, "api_key");
                if (cJSON_IsString(results)) {
                    if (strlen(results->valuestring) <= 48) {
                        strcpy(petnet_settings.api_key, results->valuestring);
                    }
                    else {
                        ESP_LOGI(TAG, "Buffer overflow detected");
                        strncpy(petnet_settings.api_key, results->valuestring, 48);
                    }
                }
                
                results = cJSON_GetObjectItem(payload, "device_key");
                if (cJSON_IsString(results)) {
                    if (strlen(results->valuestring) <= 32) {
                        strcpy(petnet_settings.device_key, results->valuestring);
                    }
                    else {
                        ESP_LOGI(TAG, "Buffer overflow detected");
                        strncpy(petnet_settings.device_key, results->valuestring, 32);
                    }
                } else {
                    petnet_settings.device_key[0] = 0x00;
                }
                is_registered = true;
                ESP_LOGI(TAG, "Device is registered. Receiving information from server.");
            }
            else {
                green_blinky = true;
                ESP_LOGI(TAG, "Device is not registered yet. Please register the device using the QRCODE provided with the control board.");
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                if (!nextion_setup_page) {
                    nextion_setup_page = true;
                    send_command(UART_NUM_1, "page page8", &nextion_response);
                    reset_data(buffer, &nextion_payload, &nextion_response);
                }
            }
            cJSON_Delete(payload);
        }
        else {
            ESP_LOGI(TAG, "Server error: %d", status_code);
        }
        free(content);
    }
    free(endpoint);
    green_blinky = false;
    vTaskDelay(500 / portTICK_PERIOD_MS);
    gpio_set_level(GREEN_LED, LED_ON);

    // Get settings
    api_get(&content, petnet_settings.api_key, petnet_settings.device_key, "/settings/");
    
    ESP_LOGD(TAG, "Content-length: %d - Hex Dump", strlen(content));
    ESP_LOG_BUFFER_HEXDUMP(TAG, content, strlen(content), ESP_LOG_DEBUG);
    
    payload = cJSON_Parse(content);

    if (payload != NULL) {
        results = cJSON_GetObjectItem(payload, "results");
        value = fetch_json_value(results, "is_setup_done");
        if (value) {
            petnet_settings.is_setup_done = atoi(value);
        }
        value = fetch_json_value(results, "tz_esp32");
        if (value) {
            strcpy(petnet_settings.tz, value);
        }
        ESP_LOGI(TAG, "is_setup_done = %d, tz = %s", petnet_settings.is_setup_done, petnet_settings.tz);
    } else {
        ESP_LOGI(TAG, "Error with JSON");
    }

    cJSON_Delete(payload);
    free(content);

    api_get(&content, petnet_settings.api_key, petnet_settings.device_key, "/feeding-schedule/");
    ESP_LOGD(TAG, "Content-length: %d - Hex Dump", strlen(content));
    ESP_LOG_BUFFER_HEXDUMP(TAG, content, strlen(content), ESP_LOG_DEBUG);

    feeding_schedule_init(content, &feeding_schedule, &num_feeding_times);
    ESP_LOGI(TAG, "Feeding Schedule has %d feeding time slots", num_feeding_times);
    free(content);

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }
    strcpy(petnet_settings.firmware_version, running_app_info.version);
    save_settings_to_nvs();

    return true;
}

static void red_blinky_task(void *data) {
    led_config_t led = *(led_config_t *) data;
    uint8_t previous_state = red_blinky;
    uint8_t is_state_off = true;
    ESP_LOGI(TAG, "blinky_task: gpio: %d active_state: %d", led.io_num, red_blinky);

    while(true) {
        if(previous_state != red_blinky) {
            ESP_LOGI(TAG, "blinky_task state changed. active_state: %d", red_blinky);
            previous_state = red_blinky;
        }
        vTaskDelay(led.delay / portTICK_PERIOD_MS);
        if(red_blinky == 1) {    
            is_state_off = false;
            led.state = !led.state;
            gpio_set_level(led.io_num, led.state);
        }
        else if (!is_state_off && green_blinky == 0) {
            is_state_off = true;
            gpio_set_level(led.io_num, LED_OFF);
        }
    }
}

static void green_blinky_task(void *data) {
    led_config_t led = *(led_config_t *) data;
    uint8_t previous_state = green_blinky;
    uint8_t is_state_off = true;
    ESP_LOGI(TAG, "blinky_task: gpio: %d active_state: %d", led.io_num, green_blinky);

    while(true) {
        vTaskDelay(led.delay / portTICK_PERIOD_MS);
        if(previous_state != green_blinky) {
            ESP_LOGI(TAG, "blinky_task state changed. active_state: %d", green_blinky);
            previous_state = green_blinky;
        }
        if(green_blinky == 1) {    
            is_state_off = false;
            led.state = !led.state;
            gpio_set_level(led.io_num, led.state);
        }
        else if (!is_state_off && green_blinky == 0) {
            is_state_off = true;
            gpio_set_level(led.io_num, LED_OFF);
        }
    }
}

static void wifi_led(void *data) {
    led_config_t led = *(led_config_t *) data;
    uint16_t loop_delay = led.delay;

    while(true) {
        if(wifi_info.state == 0) {
            led.state = 0;
            for(int i=0; i<4; i++, led.state = !led.state) {
                gpio_set_level(led.io_num, led.state);
                vTaskDelay((loop_delay/4) / portTICK_PERIOD_MS);
            }
            gpio_set_level(led.io_num, LED_OFF);
            vTaskDelay((loop_delay) / portTICK_PERIOD_MS);
        }
        else {
            gpio_set_level(led.io_num, LED_OFF);
            vTaskDelay(loop_delay / portTICK_PERIOD_MS);
        }

    }
}

void motor_pwm_init() {
    ledc_timer_config_t pwm_timer = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER,
        .duty_resolution  = PWM_DUTY_RES,
        .freq_hz          = PWM_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&pwm_timer));

    ledc_channel_config_t pwm_channel = {
        .gpio_num   = PWM_OUTPUT_IO,
        .speed_mode = PWM_MODE,
        .channel    = PWM_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = PWM_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&pwm_channel));
}

void dispense_food(uint8_t encoder_ticks) {
    uint32_t start_ticks = hopper_state.counter;
    uint32_t target_ticks = start_ticks + encoder_ticks;

    int duty = PWM_DUTY;

    if(!wifi_info.state) {
        duty = PWM_DUTY_MAX;
    }

    bool fade = true;

    green_blinky = true;

    while (hopper_state.counter < target_ticks)
    {
        vTaskDelay(200 / portTICK_PERIOD_MS);
        duty += 32;
        if (duty > PWM_DUTY_MAX) {
            duty = PWM_DUTY_MAX;
        }
        if (duty <= PWM_DUTY_MAX && fade) {
            ledc_set_duty(PWM_MODE, PWM_CHANNEL, duty);
            ledc_update_duty(PWM_MODE, PWM_CHANNEL);
            if (duty == PWM_DUTY_MAX) {
                fade = false;
            }
        }
    }
    ledc_set_duty(PWM_MODE, PWM_CHANNEL, 0);
    ledc_update_duty(PWM_MODE, PWM_CHANNEL);

    green_blinky = false;
}

void task_manager() {
    
    char incoming_msg[RX_BUFFER_SIZE];
    char buffer[RX_BUFFER_SIZE], format_dt_buffer[50];
    char heartbeat_data[TX_BUFFER_SIZE];
    char *api_content;
    uart_event_t uart_event;
    uint16_t loop_counter = 1, event_code;
    bool has_feeding_slot = false, nextion_next_meal = false;
    float temp_reading, battery_soc, battery_voltage, battery_crate;
    nextion_response_t response;
    nextion_response_t response2;
    nextion_payload_t payload;
    uint8_t wifi_connect = 0xff;
    time_t next_feeding_slot, motor_timeout, current_time, time_diff;
    uint8_t next_feeding_index, status = (uint8_t)404;
    uint8_t prev_food_state, prev_power_state;
    uint16_t status_code = (uint16_t)500;
    cJSON *json_payload, *results, *event_payload;
    struct tm *timeinfo;

    motor_timeout = time(&motor_timeout);
    prev_food_state = 0xff;
    prev_power_state = 0xff;

    while(true) {

        current_time = time(&current_time);

        if (loop_counter == 2 && is_nextion_available) {
            send_command(UART_NUM_1, "page page1", &response);
            ESP_LOGI(TAG, "Message sent!");
        }

        if (loop_counter == 3 && is_nextion_available) {
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

        if (loop_counter == 4) {
             // Sync Nextion clock with NTP Time
            sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
            sntp_setservername(0, "pool.ntp.org");
            sntp_init();
            sntp_set_time_sync_notification_cb(ntp_callback);
        }

        if (tz_changed) {
            ESP_LOGI(TAG, "Setting to Timezone: %s", petnet_settings.tz);
            setenv("TZ", petnet_settings.tz, 1);
            tzset();

            time_t now = time(&now);
            struct tm *utc_timeinfo = gmtime(&now);
            strftime(buffer, sizeof(buffer), "%A, %B %d %Y - %H:%M:%S ", utc_timeinfo);
            ESP_LOGI(TAG, "Time in UTC: %s", buffer);

            timeinfo = localtime(&now);
            strftime(buffer, sizeof(buffer), "%A, %B %d %Y - %H:%M:%S ", timeinfo);
            ESP_LOGI(TAG, "Time in %s: %s", petnet_settings.tz, buffer);

            if (is_nextion_available) {
                sync_nextion_clock(UART_NUM_1, timeinfo);
            }
            tz_changed = false;
        }

        if (loop_counter % 10 == 0 && get_next_meal_slot) {
            get_next_feeding_time(&next_feeding_slot, &next_feeding_index, feeding_schedule, num_feeding_times);
            ESP_LOGI(TAG, "Next feed time: %ld, index=%d", next_feeding_slot, next_feeding_index);
            if (next_feeding_slot) {
                if (petnet_settings.is_24h_mode) {
                    strftime(format_dt_buffer, sizeof(format_dt_buffer), "%H:%M", localtime(&next_feeding_slot));
                } else {
                    strftime(format_dt_buffer, sizeof(format_dt_buffer), "%l:%M %p", localtime(&next_feeding_slot));
                }
                sprintf(buffer, "%s at %s", feeding_schedule[next_feeding_index].meal_name, format_dt_buffer);
                if (is_nextion_available) {
                    payload.string = malloc(sizeof(char) * strlen(buffer) + 1);
                    strcpy((char *)payload.string, buffer);
                    set_value(UART_NUM_1, "page1.next_meal_name.txt", &payload, &response);
                    reset_data(buffer, &payload, &response);
                    sprintf(buffer, "%s cup for %s", feeding_schedule[next_feeding_index].feed_amount_fraction, feeding_schedule[next_feeding_index].pet_name);
                    payload.string = malloc(sizeof(char) * strlen(buffer) + 1);
                    strcpy((char *)payload.string, buffer);
                    set_value(UART_NUM_1, "page1.next_meal_desc.txt", &payload, &response);
                    reset_data(buffer, &payload, &response);
                }
                has_feeding_slot = true;
                get_next_meal_slot = false;
                nextion_next_meal = false;
            } else if (num_feeding_times == 0 && is_nextion_available && !nextion_next_meal) {
                    sprintf(buffer, "No Scheduled Meals");
                    payload.string = malloc(sizeof(char) * strlen(buffer) + 1);
                    strcpy((char *)payload.string, buffer);
                    set_value(UART_NUM_1, "page1.next_meal_name.txt", &payload, &response);
                    reset_data(buffer, &payload, &response);
                    sprintf(buffer, " ");
                    payload.string = malloc(sizeof(char) * strlen(buffer) + 1);
                    strcpy((char *)payload.string, buffer);
                    set_value(UART_NUM_1, "page1.next_meal_desc.txt", &payload, &response);
                    reset_data(buffer, &payload, &response);
                    nextion_next_meal = true;
            }

        }

        if (loop_counter == 10 && is_nextion_available) {
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

        if (loop_counter > 10 && is_nextion_available && prev_food_state != food_detect_state.state) {
            prev_food_state = food_detect_state.state;
            reset_data(buffer, &payload, &response);
            payload.number = (uint32_t)food_detect_state.state;
            ESP_LOGI(TAG, "set_value: %d", payload.number);

            if (set_value(UART_NUM_1, "page0.flag_hopper.val", &payload, &response) == NEXTION_FAIL) {
                ESP_LOGW(TAG, "Invalid Variable: %s", "page0.flag_hopper.val");
            } 
        }

         if (loop_counter > 10 && is_nextion_available && prev_power_state != power_state.state) {
            prev_power_state = power_state.state;
            reset_data(buffer, &payload, &response);
            payload.number = !(uint32_t)power_state.state;
            ESP_LOGI(TAG, "set_value: %d", payload.number);

            if (set_value(UART_NUM_1, "page0.flag_charging.val", &payload, &response) == NEXTION_FAIL) {
                ESP_LOGW(TAG, "Invalid Variable: %s", "page0.flag_charging.val");
            } 
        }

        if (loop_counter % 20 == 0) {
            get_battery_reading(&battery_voltage, &battery_soc, &battery_crate);
            sprintf(heartbeat_data, "{\"battery_soc\": %.2f, \"battery_voltage\": %.2f, \"battery_crate\":%.2f, \"on_power\": %d, \"control_board_revision\": \"%s\", \"firmware_version\": \"%s\", \"is_hopper_low\": %d }", 
                battery_soc, battery_voltage, battery_crate ,!power_state.state, CONTROL_BOARD_REVISION, petnet_settings.firmware_version, food_detect_state.state);
            status_code = api_post(&api_content, petnet_settings.api_key, petnet_settings.device_key, "/device/heartbeat/", heartbeat_data);
            if (status_code == 200) {
                json_payload = cJSON_Parse(api_content);
                results = cJSON_GetObjectItem(json_payload, "status");
                
                if (cJSON_IsNumber(results)) {
                    status = (uint8_t)results->valueint;
                }

                if (status == 200) {
                    results = cJSON_GetObjectItem(json_payload, "has_event");
                    if (cJSON_IsTrue(results)) {
                        ESP_LOGI(TAG, "An event has been triggered by the server, processing the event now.");
                        event_payload = cJSON_GetObjectItem(json_payload, "event");
                        if (cJSON_IsObject(event_payload)) {
                            ESP_LOGI(TAG, "Calling event handler");
                            event_code = process_event(event_payload);
                            ESP_LOGI(TAG, "Event Processed. Code: %d", event_code);
                            if (event_code == SMART_FEEDER_EVENT_SETTINGS_CHANGE) {
                                ESP_LOGI(TAG, "Saving settings to NVS.");
                                save_settings_to_nvs();
                            }
                        }
                    }
                }
                else {
                    ESP_LOGI(TAG, "Status code: %d", status);
                }
                cJSON_Delete(json_payload);
            }
            else if (status_code == 403)
            {
               ESP_LOGI(TAG, "Authentication failed. Restarting.");
               esp_restart();
            }
            
            else {
                ESP_LOGI(TAG, "Server error. Status code: %d ", status_code);
            }
            free(api_content);
        }

        // If Wifi connection status changes, update the wifi icon
        if (wifi_connect != wifi_info.state && is_nextion_available) {
            payload.number=(int)wifi_info.state;
            if (set_value(UART_NUM_1, "page0.flag_wifi.val", &payload, &response) == NEXTION_OK) {
                wifi_connect = wifi_info.state;
            };
        }

        // Is it time to feed the pet?
        if (has_feeding_slot) {
            time_diff = abs(next_feeding_slot -  current_time);
            if (current_time > next_feeding_slot && time_diff < 60 && current_time > motor_timeout && feeding_schedule[next_feeding_index].is_active) {

                ESP_LOGI(TAG, "Scheduled meal is triggered");
                ESP_LOGI(TAG, "current_time: %ld next_feeding_slot: %ld, difference: %ld, motor_timeout: %ld", current_time, next_feeding_slot, time_diff, motor_timeout);

                if (is_nextion_available) {
                    reset_data(buffer, &payload, &response);
                    sprintf(buffer, "Dispensing %s", feeding_schedule[next_feeding_index].meal_name);
                    payload.string = malloc(sizeof(char) * strlen(buffer) + 1);
                    strcpy((char *)payload.string, buffer);
                    ESP_LOGI(TAG, "set_value: %s", payload.string);
                    set_value(UART_NUM_1, "page6.meal_name.txt", &payload, &response);
                    
                    reset_data(buffer, &payload, &response);
                    sprintf(buffer, "%s cup for %s", feeding_schedule[next_feeding_index].feed_amount_fraction,
                        feeding_schedule[next_feeding_index].pet_name);
                    payload.string = malloc(sizeof(char) * strlen(buffer) + 1);
                    strcpy((char *)payload.string, buffer);
                    ESP_LOGI(TAG, "set_value: %s", payload.string);
                    set_value(UART_NUM_1, "page6.meal_desc.txt", &payload, &response);
                    reset_data(buffer, &payload, &response);
                    send_command(UART_NUM_1, "page page6", &response);
                    reset_data(buffer, &payload, &response);
                }

                dispense_food(feeding_schedule[next_feeding_index].interrupter_count);
                status_code = log_feeding(feeding_schedule[next_feeding_index].pet_name, "S", feeding_schedule[next_feeding_index].feed_amount);
                vTaskDelay(1500 / portTICK_PERIOD_MS);
                
                if (is_nextion_available) {
                    send_command(UART_NUM_1, "page page1", &response);
                }
                motor_timeout = current_time + 120;
                get_next_meal_slot = true;
            }
        }
        

        reset_data(incoming_msg, &payload, &response);

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
                parse_event((uint8_t *)incoming_msg, 32, &response);
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
        switch (response.event_code)
        {
        case NEXTION_TOUCH:
            ESP_LOGI(TAG, "NEXTION_TOUCH");
            if (response.page == 1 && response.component == 12) {
                dispense_food(16);
                if (is_nextion_sleeping) {
                        send_command(UART_NUM_1, "sleep=0", &response);
                        reset_data(buffer, &payload, &response);
                }
                send_command(UART_NUM_1, "page page1", &response2);
            }
            break;           
        case NEXTION_TOUCH_COORDINATE:
            ESP_LOGI(TAG, "NEXTION_TOUCH_COORDINATE");
            break;
        case NEXTION_TOUCH_IN_SLEEP:
            ESP_LOGI(TAG, "NEXTION_TOUCH_IN_SLEEP");
            break;  
        case NEXTION_AUTO_SLEEP:
            ESP_LOGI(TAG, "NEXTION_AUTO_SLEEP");
            is_nextion_sleeping = true;
            break;      
        case NEXTION_AUTO_WAKE:
            ESP_LOGI(TAG, "NEXTION_AUTO_WAKE");
            is_nextion_sleeping = false;
            break;       
        case NEXTION_STARTUP:
            ESP_LOGI(TAG, "NEXTION_STARTUP");
            break;         
        case NEXTION_SD_CARD_UPGRADE:
            ESP_LOGI(TAG, "NEXTION_SD_CARD_UPGRADE");
            break; 
        }

        reset_data(incoming_msg, &payload, &response);
        reset_data(incoming_msg, &payload, &response2);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        loop_counter++;
    }
}

void app_main(void) {
    size_t nvs_size;
    nvs_handle_t eeprom_handle;
    srand(rand() % 1000);
    
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for the partition table
    partition.address   = ESP_PARTITION_TABLE_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_MAX_LEN;
    partition.type      = ESP_PARTITION_TYPE_DATA;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for the partition table: ");

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");

    ESP_LOGI(TAG, "Firmware Platform: %s", CONTROL_BOARD_REVISION);

    // Initialize NVS partiton
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // OTA app partition table has a smaller NVS partition size than the non-OTA
        // partition table. This size mismatch may cause NVS initialization to fail.
        // If this happens, we erase NVS partition and initialize NVS again.
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(nvs_open("nvs", NVS_READWRITE, &eeprom_handle));

    esp_err_t rs = nvs_get_blob(eeprom_handle, "settings", (petnet_rescued_settings_t *)&petnet_settings, &nvs_size);
    
    if(nvs_size != sizeof(petnet_rescued_settings_t) && rs == ESP_OK) {
        ESP_LOGI(TAG, "The size of petnet_rescued_settings_t has changed. Erasing Flash and Reinitialize.");
        nvs_close(eeprom_handle);
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
        ESP_ERROR_CHECK(nvs_open("nvs", NVS_READWRITE, &eeprom_handle));
        rs = ESP_ERR_NVS_NOT_FOUND;
    }
    switch (rs)
    {
    case ESP_ERR_NVS_NOT_FOUND:
        memset(&petnet_settings, 0, sizeof(petnet_rescued_settings_t));
        const esp_partition_t *running = esp_ota_get_running_partition();
        esp_app_desc_t running_app_info;
        if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
            ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
        }
        strcpy(petnet_settings.firmware_version, running_app_info.version);
        get_chip_id(petnet_settings.device_id);
        strcpy(petnet_settings.tz, "UTC");
        strcpy(petnet_settings.api_key, API_KEY);
        secret_generator(petnet_settings.secret, sizeof(petnet_settings.secret));
        petnet_settings.is_24h_mode = false;
        ESP_ERROR_CHECK(nvs_set_blob(eeprom_handle, "settings", &petnet_settings, sizeof(petnet_rescued_settings_t)));
        ESP_ERROR_CHECK(nvs_commit(eeprom_handle));
        ESP_LOGI(TAG, "New settings was created and saved to NVS");
        break;
    
    default:
        break;
    }

    nvs_close(eeprom_handle);

    // Get number of used entries and free entries in the NVS partitions
    nvs_stats_t nvs_stats;
    nvs_get_stats(NULL, &nvs_stats);
    ESP_LOGI(TAG, "Count: UsedEntries = (%d), FreeEntries = (%d), AllEntries = (%d)\n", nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries);

    // Configure dynamic frequency scaling:
    // maximum and minimum frequencies are set in sdkconfig,
    // automatic light sleep is enabled if tickless idle support is enabled.

    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
            .light_sleep_enable = true
#endif
    };
    ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );

    ESP_LOGI(TAG, "device id: %s, tz: %s, api: %s, secret: %s, 24h: %d", petnet_settings.device_id, petnet_settings.tz,
        petnet_settings.api_key, petnet_settings.secret ,petnet_settings.is_24h_mode);

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

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // Initialize GPIO input config for power sensor
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_POWER_SNSR_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // Initialize GPIO input config for hopper sensor
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_ANYEDGE_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    // Initialize GPIO button input config
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_BUTTON_SEL;
    gpio_config(&io_conf);

    // Initialize motor pin for PWM
    motor_pwm_init();

    // Set up LEDs
    led_config_t blue_led, red_led, green_led;
    blue_led.io_num = BLUE_LED;
    blue_led.delay = 1000;
    blue_led.state = 0;
    red_led.io_num = RED_LED;
    red_led.delay = 500;
    red_led.state = 0;
    green_led.io_num = GREEN_LED;
    green_led.delay = 350;
    green_led.state = 0;

    red_blinky = false;
    green_blinky = false;
    wifi_info.state = false;

    gpio_set_level(GREEN_LED, LED_OFF);
    gpio_set_level(RED_LED, LED_OFF);

    // Set up I2C
    i2c_port_t i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &i2c_config);
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, i2c_config.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0));
    ESP_LOGI(TAG, "I2C Initialized successfully.");
    uint8_t *fuel_gauge_buf = (uint8_t *)malloc(2);

    ret = i2c_master_read_register(i2c_master_port, MAX1704X_REGISTER_VERSION, fuel_gauge_buf, 2);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "i2c version register read ok");
        ESP_LOG_BUFFER_HEXDUMP(TAG, fuel_gauge_buf, 2, ESP_LOG_INFO);
        reset_fuel_gauge();
    } else if (ret == ESP_ERR_TIMEOUT) {
        ESP_LOGI(TAG, "I2C timed out.");
    } else {
        ESP_LOGI(TAG, "Unknown I2C error: %x", ret);
    }

    // Set up UART connection with Nextion
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,                   
        .data_bits = UART_DATA_8_BITS,    
        .parity = UART_PARITY_DISABLE,            
        .stop_bits = UART_STOP_BITS_1,      
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE 
    };

    gpio_set_level(RED_LED, LED_OFF);
    gpio_set_level(GREEN_LED, LED_OFF);

    if(gpio_get_level(BUTTON) == 0) {
        int count = 0;
        ESP_LOGI(TAG, "Button is pressed down on boot.");
        gpio_set_level(RED_LED, LED_ON);
        while (gpio_get_level(BUTTON) == 0) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            count++;
            if (count > 50) {
                break;
            }
        }
        if (count > 50) {
            ESP_LOGI(TAG, "Button pressed for more than 5 seconds.");
            ESP_LOGI(TAG, "Resetting to factory configuration");
            gpio_set_level(RED_LED, LED_OFF);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            gpio_set_level(RED_LED, LED_ON);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            gpio_set_level(RED_LED, LED_OFF);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(GREEN_LED, LED_ON);
            memset(&petnet_settings.ssid, 0, sizeof(petnet_settings.ssid));
            memset(&petnet_settings.password, 0, sizeof(petnet_settings.password));
            petnet_settings.datetime_registered = 0;
            petnet_settings.datetime_last_boot = 0;
            petnet_settings.is_registered = false;
            petnet_settings.is_setup_done = false;
            petnet_settings.is_24h_mode = false;
            petnet_settings.is_notification_on = false;
            memset(&petnet_settings.tz, 0, sizeof(petnet_settings.tz));
            save_settings_to_nvs();
            ESP_ERROR_CHECK(nvs_flash_erase_partition("otadata"));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            esp_restart();
        } else {
            ESP_LOGI(TAG, "Button pressed for less than 5 seconds.");
            gpio_set_level(RED_LED, LED_OFF);
            gpio_set_level(GREEN_LED, LED_OFF);
            force_reprovisioning = true;
        }
    }

    /*
     *   Wi-Fi Provisioning
     */
    

    vTaskDelay(200 / portTICK_PERIOD_MS);

    xTaskCreate(wifi_led, "wifi_status_led", 1024, &blue_led, 10, NULL);

    ESP_LOGI(TAG, "Starting WiFi Provisioning");
    esp_err_t err = wifi_provisioning(&wifi_info);
    gpio_set_level(RED_LED, LED_OFF);
    gpio_set_level(GREEN_LED, LED_ON);

    ESP_LOGI(TAG, "WiFi Provisioning Completed");

    if(err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi Connected. AP MAC ID:" MACSTR "", MAC2STR(wifi_info.ap_info.bssid));
        ESP_LOGI(TAG, "Station IP:" IPSTR " MAC ID:" MACSTR "", IP2STR(&wifi_info.netif_info.ip), MAC2STR(wifi_info.mac));
    }

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

    if (initialize_nextion_connection(UART_NUM_1) == NEXTION_OK) {
        is_nextion_available = true;
    }
    else {
        is_nextion_available = false;
    }

    xTaskCreate(green_blinky_task, "blink_task", 2048, &green_led, 10, NULL);
    xTaskCreate(red_blinky_task, "blink_task", 2048, &red_led, 10, NULL);
    bool diagnostic_is_ok = initialize();

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            if (diagnostic_is_ok) {
                ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }

    // Create a queue to handle isr event
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_handler, "gpio_task_handler", 4096, NULL, 10, NULL);

    // Install isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(MOTOR_SNSR, gpio_isr_handler, (void *) MOTOR_SNSR);
    gpio_isr_handler_add(HOPPER_SNSR, gpio_isr_handler, (void *) HOPPER_SNSR);
    gpio_isr_handler_add(BUTTON, gpio_isr_handler, (void *) BUTTON);
    gpio_isr_handler_add(POWER_SNSR, gpio_isr_handler, (void *) POWER_SNSR);

    gpio_set_level(GREEN_LED, LED_OFF);
    // xTaskCreate(green_blinky_task, "blink_task", 2048, &green_led, 10, NULL);
    // xTaskCreate(red_blinky_task, "blink_task", 2048, &red_led, 10, NULL);

    xTaskCreate(task_manager, "task_manager", 12288, NULL, 10, NULL);

    while(true) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}