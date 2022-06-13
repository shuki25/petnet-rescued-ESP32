#include <search.h>
#include "json_util.h"

char *fetch_json_value(cJSON *payload, char *key) {
    cJSON *row, *name, *value;
    cJSON_ArrayForEach(row, payload) {
            name = cJSON_GetObjectItem(row, "name");
            if (!strcasecmp(name->valuestring, key)) {
                value = cJSON_GetObjectItem(row, "value");
                if (value != NULL) {
                    if (cJSON_IsString(value)) {
                        return value->valuestring;
                    }
                    else {
                        return NULL;
                    }
                return NULL;
            }
        }
    }
    return NULL;
}