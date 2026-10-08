#include "app_config.h"
#include "boot_button.h"
#include "demo_platform.h"
#include "wechat_voip.h"
#include "memory_probe.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
static bool s_started;
static char s_line[256];
static atomic_uint s_alloc_failure_state, s_alloc_failures;
static size_t s_failed_size;
static uint32_t s_failed_caps;
static const char *s_failed_function;

static void boot_button_pressed(void)
{
    esp_err_t err = wechat_voip_command(VOIP_PRIMARY_ACTION, 0);
    if (err != ESP_OK)
        ESP_LOGW("app", "BOOT action not queued: %s", esp_err_to_name(err));
}

static void allocation_failed(size_t size, uint32_t caps, const char *function)
{
    /* The allocator callback only records evidence; logging may allocate too. */
    atomic_fetch_add(&s_alloc_failures, 1);
    unsigned empty = 0;
    if (atomic_compare_exchange_strong(&s_alloc_failure_state, &empty, 1)) {
        s_failed_size = size;
        s_failed_caps = caps;
        s_failed_function = function;
        atomic_store(&s_alloc_failure_state, 2);
    }
}
static void report_allocation_failure(void)
{
    static int64_t last_report;
    if (atomic_load(&s_alloc_failure_state) != 2)
        return;
    int64_t now = esp_timer_get_time();
    if (last_report && now - last_report < 1000000)
        return;
    multi_heap_info_t pool;
    heap_caps_get_info(&pool, s_failed_caps);
    ESP_LOGE("memory",
             "allocation failed: requested=%uB caps=0x%lx function=%s total=%u; "
             "sample=report-time internal_free=%uB internal_largest=%uB "
             "requested_pool_free=%uB pool_largest=%uB pool_min=%uB",
             (unsigned)s_failed_size, (unsigned long)s_failed_caps, s_failed_function,
             (unsigned)atomic_load(&s_alloc_failures),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)pool.total_free_bytes, (unsigned)pool.largest_free_block,
             (unsigned)pool.minimum_free_bytes);
    atomic_store(&s_alloc_failure_state, 0);
    last_report = now;
}
static void help(void)
{
    puts("\nC3 WeChat VoIP: ESP-IDF 5.5.5 / TiRTC 2.5.0 / recorded Opus 16kHz mono");
    puts("wifi {\"ssid\":\"YOUR_SSID\",\"password\":\"YOUR_PASSWORD\"}");
    puts("status | memory | id | contacts | call 1 | answer | reject | hangup | rx-record | "
         "rx-dump | retry "
         "| help");
    puts("BOOT: idle=call contact 1 | incoming=answer | calling/connected=hangup");
    puts("First boot: configure Wi-Fi, enter printed BIND CODE in developer platform.");
    printf(
        "Then open WeChat mini-program and authorize this device. Mini-program version=%u (%s).\n",
        (unsigned)APP_WECHAT_VERSION,
        APP_WECHAT_VERSION == WECHAT_VERSION_RELEASE       ? "release"
        : APP_WECHAT_VERSION == WECHAT_VERSION_DEVELOPMENT ? "development"
                                                           : "trial");
}
static void start_network(const char *ssid, const char *password)
{
    if (s_started) {
        puts("Wi-Fi saved. Restart device to use the new network.");
        return;
    }
    esp_err_t err = platform_start(ssid, password, APP_DISCOVERY_URL, wechat_voip_platform_event);
    if (err == ESP_OK)
        s_started = true;
    else
        ESP_LOGE("app", "platform start: %s", esp_err_to_name(err));
}
static void command(char *line)
{
    if (strncmp(line, "wifi ", 5) == 0) {
        cJSON *root = cJSON_Parse(line + 5);
        cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
        cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");
        if (cJSON_IsString(ssid) && cJSON_IsString(password)) {
            esp_err_t err = platform_save_wifi(ssid->valuestring, password->valuestring);
            if (err == ESP_OK)
                start_network(ssid->valuestring, password->valuestring);
            else
                ESP_LOGE("app", "save Wi-Fi: %s", esp_err_to_name(err));
            memset(password->valuestring, 0, strlen(password->valuestring));
        } else
            puts("Invalid wifi JSON");
        cJSON_Delete(root);
        return;
    }
    if (strcmp(line, "help") == 0) {
        help();
        return;
    }
    if (strcmp(line, "memory") == 0) {
        memory_probe_snapshot();
        return;
    }
    if (strcmp(line, "id") == 0) {
        platform_identity_t id;
        platform_identity_snapshot(&id);
        printf("device_id=%s client_id=%s\n", id.device_id, platform_client_id());
        memset(&id, 0, sizeof(id));
        return;
    }
    voip_command_t action;
    unsigned index = 0;
    if (strcmp(line, "status") == 0)
        action = VOIP_STATUS;
    else if (strcmp(line, "contacts") == 0)
        action = VOIP_CONTACTS;
    else if (strcmp(line, "answer") == 0)
        action = VOIP_ANSWER;
    else if (strcmp(line, "reject") == 0)
        action = VOIP_REJECT;
    else if (strcmp(line, "hangup") == 0 || strcmp(line, "cancel") == 0)
        action = VOIP_HANGUP;
    else if (strcmp(line, "rx-dump") == 0)
        action = VOIP_RX_DUMP;
    else if (strcmp(line, "rx-record") == 0)
        action = VOIP_RX_RECORD;
    else if (strcmp(line, "retry") == 0)
        action = VOIP_RETRY;
    else if (strncmp(line, "call ", 5) == 0) {
        char *end;
        unsigned long value = strtoul(line + 5, &end, 10);
        if (end == line + 5 || *end || value == 0 || value > 8) {
            puts("Use call 1..8");
            return;
        }
        index = value;
        action = VOIP_CALL;
    } else {
        puts("Unknown command; enter help");
        return;
    }
    esp_err_t err = wechat_voip_command(action, index);
    if (err != ESP_OK)
        ESP_LOGE("app", "command not queued: %s", esp_err_to_name(err));
}
void app_main(void)
{
    ESP_ERROR_CHECK(heap_caps_register_failed_alloc_callback(allocation_failed));
    memory_probe_init();
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE("app", "NVS init failed: %s; identity not erased", esp_err_to_name(err));
        return;
    }
    ESP_ERROR_CHECK(wechat_voip_start(APP_WECHAT_VERSION));
    ESP_ERROR_CHECK(boot_button_start((gpio_num_t)APP_BOOT_BUTTON_GPIO, boot_button_pressed));
    ESP_LOGI("app", "BOOT button ready: gpio=%d idle=call-1 incoming=answer call=hangup",
             APP_BOOT_BUTTON_GPIO);
    char ssid[33] = APP_WIFI_SSID, password[65] = APP_WIFI_PASSWORD;
    if (!ssid[0])
        platform_load_wifi(ssid, sizeof(ssid), password, sizeof(password));
    help();
    if (ssid[0])
        start_network(ssid, password);
    memset(password, 0, sizeof(password));
    size_t used = 0;
    bool overflow = false;
    setvbuf(stdin, NULL, _IONBF, 0);
    for (;;) {
        memory_probe_sample();
        memory_probe_poll();
        report_allocation_failure();
        int ch = getchar();
        if (ch == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (overflow)
                puts("Command too long; discarded");
            else if (used) {
                s_line[used] = 0;
                command(s_line);
            }
            memset(s_line, 0, sizeof(s_line));
            used = 0;
            overflow = false;
        } else if (ch == 8 || ch == 127) {
            if (used)
                --used;
        } else if (ch >= 32 && !overflow) {
            if (used + 1 < sizeof(s_line))
                s_line[used++] = (char)ch;
            else
                overflow = true;
        }
    }
}
