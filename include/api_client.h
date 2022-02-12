#include "esp_http_client.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 1024

esp_err_t client_event_handler(esp_http_client_event_t *event);
void api_get(char **content, char *auth_token, char *endpoint);