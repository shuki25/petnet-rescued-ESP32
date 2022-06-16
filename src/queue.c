#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "esp_log.h"

#define TAG "queue.c"

void queue_init(queue_t *q) {
    if(q == NULL) {
        ESP_LOGE(TAG, "queue_init: q is NULL");
        return;
    }
    q->count = 0;
    q->front = NULL;
    q->rear = NULL;
}

int8_t queue_isempty(queue_t *q) {
    return (q->rear == NULL);
}

void enqueue(queue_t *q, void *data) {
    queue_node_t *node = malloc(sizeof(queue_node_t));
    node->data = data;
    node->next = NULL;

    if (queue_isempty(q)) {
        q->front = q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }
    q->count++;
}

void enqueue_front(queue_t *q, void *data) {
    queue_node_t *node = malloc(sizeof(queue_node_t));
    node->data = data;
    node->next = NULL;

    if (queue_isempty(q)) {
        q->front = q->rear = node;
    } else {
        node->next = q->front;
        q->front = node;
    }
    q->count++;
}

void *dequeue(queue_t *q) {
    if (queue_isempty(q)) {
        return NULL;
    }

    queue_node_t *tmp = q->front;
    void *data = q->front->data;
    q->front = q->front->next;
    if(q->front == NULL) {
        q->rear = NULL;
    }
    q->count--;
    free(tmp);
    
    return data;
}

void queue_print(queue_t *q, void (*print_data)(void *)) {
    queue_node_t *tmp = q->front;
    while (tmp != NULL) {
        print_data(tmp->data);
        tmp = tmp->next;
    }
}
