#include "platform_internal.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
typedef struct {
    message_buffer_t *buffer;
    size_t limit, content_capacity;
    bool overflow;
    bool oom;
    unsigned retry_after;
} response_t;
static esp_err_t http_event(esp_http_client_event_t *event)
{
    response_t *r = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_HEADER && event->header_key && event->header_value) {
        if (strcasecmp(event->header_key, "Retry-After") == 0) {
            char *end = NULL;
            unsigned long value = strtoul(event->header_value, &end, 10);
            if (end != event->header_value && *end == 0 && value <= 3600)
                r->retry_after = value;
        } else if (strcasecmp(event->header_key, "Content-Length") == 0) {
            char *end = NULL;
            unsigned long value = strtoul(event->header_value, &end, 10);
            if (end != event->header_value && *end == 0 && value < r->limit)
                r->content_capacity = (size_t)value + 1;
        }
    }
    if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
        if (!r->buffer->data &&
            !message_buffer_begin(r->buffer, r->content_capacity > 1 ? r->content_capacity : r->limit)) {
            r->oom = true;
            return ESP_ERR_NO_MEM;
        }
        if (!message_buffer_append(r->buffer, r->buffer->used, event->data,
                                   (size_t)event->data_len)) {
            r->overflow = true;
            return ESP_ERR_INVALID_SIZE;
        }
    }
    return ESP_OK;
}
bool json_string_copy(const cJSON *object, const char *key, char *out, size_t capacity)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring) >= capacity)
        return false;
    memcpy(out, v->valuestring, strlen(v->valuestring) + 1);
    return true;
}
esp_err_t platform_http(const char *url, esp_http_client_method_t method, const char *body,
                        const char *token, const http_header_t *headers, size_t header_count,
                        message_buffer_t *response, size_t limit, http_result_t *result)
{
    if (!url || !response || limit < 2 || !result)
        return ESP_ERR_INVALID_ARG;
    message_buffer_release(response);
    memset(result, 0, sizeof(*result));
    result->code = -1;
    response_t r = {.buffer = response, .limit = limit};
    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event,
        .user_data = &r,
        .disable_auto_redirect = true,
        .buffer_size = 1024,
        .buffer_size_tx = 1536,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
        return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_OK;
    char authorization[1100];
    if (token && token[0]) {
        int n = snprintf(authorization, sizeof(authorization), "Bearer %s", token);
        if (n < 0 || n >= sizeof(authorization))
            err = ESP_ERR_INVALID_SIZE;
        else
            err = esp_http_client_set_header(client, "Authorization", authorization);
    }
    for (size_t i = 0; err == ESP_OK && i < header_count; ++i)
        err = esp_http_client_set_header(client, headers[i].name, headers[i].value);
    if (err == ESP_OK && body) {
        err = esp_http_client_set_header(client, "Content-Type", "application/json");
        if (err == ESP_OK)
            err = esp_http_client_set_post_field(client, body, strlen(body));
    }
    int64_t begin = esp_timer_get_time();
    ESP_LOGI("thing_http", "begin: method=%d free=%u largest=%u", method,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    if (err == ESP_OK)
        err = esp_http_client_perform(client);
    result->status = esp_http_client_get_status_code(client);
    result->retry_after = r.retry_after;
    if (r.oom)
        err = ESP_ERR_NO_MEM;
    else if (r.overflow)
        err = ESP_ERR_INVALID_SIZE;
    if (err == ESP_OK && !esp_http_client_is_complete_data_received(client))
        err = ESP_ERR_INVALID_RESPONSE;
    esp_http_client_cleanup(client);
    memset(authorization, 0, sizeof(authorization));
    if (err == ESP_OK && !response->data && !message_buffer_begin(response, 2))
        err = ESP_ERR_NO_MEM;
    if (err == ESP_OK) {
        cJSON *root = cJSON_Parse(response->data);
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
        if (cJSON_IsNumber(code))
            result->code = code->valueint;
        cJSON_Delete(root);
        if (result->status < 200 || result->status >= 300)
            err = ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGI("thing_http", "done: http=%d code=%d bytes=%u response_alloc=%u elapsed_ms=%lld result=%s",
             result->status, result->code, (unsigned)response->used, (unsigned)response->capacity,
             (long long)((esp_timer_get_time() - begin) / 1000), esp_err_to_name(err));
    return err;
}
