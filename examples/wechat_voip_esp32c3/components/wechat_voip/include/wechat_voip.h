#pragma once
#include "demo_platform.h"
typedef enum {
    VOIP_STATUS,
    VOIP_CONTACTS,
    VOIP_CALL,
    VOIP_ANSWER,
    VOIP_REJECT,
    VOIP_HANGUP,
    VOIP_RX_DUMP,
    VOIP_RX_RECORD,
    VOIP_RETRY,
    VOIP_PRIMARY_ACTION
} voip_command_t;
esp_err_t wechat_voip_start(unsigned version_type);
bool wechat_voip_platform_event(const platform_event_t *event);
esp_err_t wechat_voip_command(voip_command_t command, unsigned contact_index);
