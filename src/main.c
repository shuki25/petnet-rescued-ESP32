#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "config.h"
#include "sdkconfig.h"

typedef struct {
    uint32_t io_num;
    uint8_t state;
    uint16_t delay;
} led_config_t;

typedef struct {
    uint8_t state;
    uint32_t counter;
} input_state_t;

static xQueueHandle gpio_evt_queue = NULL;
static input_state_t hopper_state;
static input_state_t button_state;

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
                button_state.counter++;
                button_state.state = current_state;
                printf("button count: %d\n", button_state.counter);
            }
            // else {
            //     printf("GPIO[%d] intr, val: %d\n", io_num, current_state);
            // }
        }
    }
}

static void blinky_task(void *data) {
    led_config_t led = *(led_config_t *) data;

    while(true) {
        vTaskDelay(led.delay / portTICK_PERIOD_MS);
        led.state = !led.state;
        gpio_set_level(led.io_num, led.state);
    }
}

void app_main(void) {

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

    // Create a queue to handle isr event
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_handler, "gpio_task_handler", 2048, NULL, 10, NULL);

    // Install isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(MOTOR_SNSR, gpio_isr_handler, (void *) MOTOR_SNSR);
    gpio_isr_handler_add(BUTTON, gpio_isr_handler, (void *) BUTTON);

    printf("Minimum free heap size: %d bytes\n", esp_get_free_heap_size());

    // Set up LEDs
    led_config_t blue_led, red_led, green_led;
    blue_led.io_num = ONBOARD_LED;
    blue_led.delay = 500;
    blue_led.state = 0;
    red_led.io_num = RED_LED;
    red_led.delay = 700;
    red_led.state = 0;
    green_led.io_num = GREEN_LED;
    green_led.delay = 150;
    green_led.state = 0;

    // xTaskCreate(blinky_task, "blink_task", 1024, &blue_led, 10, NULL);
    xTaskCreate(blinky_task, "blink_task", 1024, &green_led, 10, NULL);
    xTaskCreate(blinky_task, "blink_task", 1024, &red_led, 10, NULL);
    
     printf("Minimum free heap size: %d bytes\n", esp_get_free_heap_size());

    while(true) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}