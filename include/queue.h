#ifndef PETNET_ESP32_QUEUE_H
#define PETNET_ESP32_QUEUE_H

typedef struct __queue_node {
    void *data;
    struct __queue_node *next;
} queue_node_t;

typedef struct __queue {
    int count;
    queue_node_t *front;
    queue_node_t *rear;
} queue_t;


void queue_init(queue_t *q);
int8_t queue_isempty(queue_t *q);
void enqueue(queue_t *q, void *data);
void enqueue_front(queue_t *q, void *data);
void *dequeue(queue_t *q);
void queue_print(queue_t *q, void (*print_data)(void *));

#endif
