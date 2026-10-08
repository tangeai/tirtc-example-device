#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct {
    char device_id[65];
    char device_key[257];
} platform_identity_t;
typedef enum {
    PLATFORM_ONLINE,
    PLATFORM_OFFLINE,
    PLATFORM_UNBOUND,
    PLATFORM_MESSAGE,
    PLATFORM_RESPONSE
} platform_event_kind_t;
typedef enum { PLATFORM_PROFILE, PLATFORM_CONTACTS, PLATFORM_CALL } platform_operation_t;
typedef struct {
    platform_event_kind_t kind;
    platform_operation_t operation;
    uint32_t generation;
    esp_err_t error;
    const char *json;
} platform_event_t;
/* Called by platform/MQTT tasks. Copy borrowed data; never block. False means
 * not accepted and prevents a command ACK so the sender can retry. */
typedef bool (*platform_observer_t)(const platform_event_t *event);
esp_err_t platform_start(const char *ssid, const char *password, const char *discovery_url,
                         platform_observer_t observer);
esp_err_t platform_save_wifi(const char *ssid, const char *password);
bool platform_load_wifi(char *ssid, size_t ssid_size, char *password, size_t password_size);
void platform_identity_snapshot(platform_identity_t *identity);
const char *platform_client_id(void);
const char *platform_rtc_endpoint(void);
bool platform_is_online(void);
esp_err_t platform_request(platform_operation_t operation, uint32_t generation, const char *json);
/* Explicit retry after a bounded startup/provision failure, not a factory reset. */
void platform_retry(void);
