#ifndef PETNET_ESP32_BATTERY_H
#define PETNET_ESP32_BATTERY_H

#define FUEL_GAUGE_ALERT    4           // GPIO 4, Pull up

#include <max1704x.h>

void get_battery_reading(max1704x_t *dev, float *voltage, float *soc_percent, float *crate);

#endif