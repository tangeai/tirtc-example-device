#pragma once
/* Application-level choices only. Do not put SDK ABI or stack sizes here. */
#define WECHAT_VERSION_RELEASE 0
#define WECHAT_VERSION_DEVELOPMENT 1
#define WECHAT_VERSION_TRIAL 2
#if __has_include("app_config.local.h")
#include "app_config.local.h"
#endif
#ifndef APP_WIFI_SSID
#define APP_WIFI_SSID ""
#endif
#ifndef APP_WIFI_PASSWORD
#define APP_WIFI_PASSWORD ""
#endif
#ifndef APP_DISCOVERY_URL
#define APP_DISCOVERY_URL "https://ep-open.tangeopen.com/services"
#endif
#ifndef APP_WECHAT_VERSION
#define APP_WECHAT_VERSION WECHAT_VERSION_RELEASE
#endif
#ifndef APP_BOOT_BUTTON_GPIO
#define APP_BOOT_BUTTON_GPIO 9
#endif
#if APP_WECHAT_VERSION < 0 || APP_WECHAT_VERSION > 2
#error "Invalid WeChat mini-program version"
#endif
