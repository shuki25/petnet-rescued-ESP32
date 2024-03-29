#ifndef PETNET_ESP32_EVENT_H
#define PETNET_ESP32_EVENT_H

#define SMART_FEEDER_EVENT_MANUAL_FEED          100
#define SMART_FEEDER_EVENT_AUTO_FEED            200
#define SMART_FEEDER_EVENT_SCHEDULE_CHANGE      300
#define SMART_FEEDER_EVENT_SETTINGS_CHANGE      400
#define SMART_FEEDER_EVENT_FACTORY_RESET        500
#define SMART_FEEDER_EVENT_WIFI_SETTING_RESET   600
#define SMART_FEEDER_EVENT_DEVICE_DEACTIVATED   700
#define SMART_FEEDER_EVENT_FIRMWARE_UPDATE      800

uint16_t process_event(cJSON *event);
uint16_t notify_event_completed(uint32_t event_id);

#endif