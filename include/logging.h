#ifndef PETNET_ESP32_LOGGING_H
#define PETNET_ESP32_LOGGING_H

#include "queue.h"

extern queue_t *logging_queue;

uint16_t log_feeding(char *pet_name, char *feed_type, float feed_amt);
void add_to_queue(char *buffer, bool front);
void print_queue_content(void *data);
void print_logging_queue(void);
void post_logging_queue(void);

#endif