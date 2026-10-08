#include "boot_button.h"

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define BUTTON_POLL_MS 20
#define BUTTON_DEBOUNCE_SAMPLES 3

static gpio_num_t s_gpio;
static boot_button_pressed_cb_t s_pressed_cb;
static StaticTimer_t s_timer_storage;
static TimerHandle_t s_timer;
static int s_stable_level;
static int s_candidate_level;
static uint8_t s_candidate_samples;

static void poll_button(TimerHandle_t timer)
{
    (void)timer;
    int level = gpio_get_level(s_gpio);
    if (level == s_stable_level) {
        s_candidate_level = level;
        s_candidate_samples = 0;
        return;
    }
    if (level != s_candidate_level) {
        s_candidate_level = level;
        s_candidate_samples = 1;
        return;
    }
    if (++s_candidate_samples < BUTTON_DEBOUNCE_SAMPLES)
        return;

    s_stable_level = level;
    s_candidate_samples = 0;
    if (level == 0 && s_pressed_cb)
        s_pressed_cb();
}

esp_err_t boot_button_start(gpio_num_t gpio, boot_button_pressed_cb_t pressed_cb)
{
    if (s_timer || !pressed_cb || gpio < 0 || gpio >= GPIO_NUM_MAX)
        return ESP_ERR_INVALID_ARG;

    gpio_config_t config = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK)
        return err;

    s_gpio = gpio;
    s_pressed_cb = pressed_cb;
    s_stable_level = gpio_get_level(gpio);
    s_candidate_level = s_stable_level;
    s_timer = xTimerCreateStatic("boot_btn", pdMS_TO_TICKS(BUTTON_POLL_MS), pdTRUE, NULL,
                                 poll_button, &s_timer_storage);
    if (!s_timer)
        return ESP_ERR_NO_MEM;
    if (xTimerStart(s_timer, 0) != pdPASS) {
        s_timer = NULL;
        s_pressed_cb = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
