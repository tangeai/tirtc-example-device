#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

typedef void (*boot_button_pressed_cb_t)(void);

esp_err_t boot_button_start(gpio_num_t gpio, boot_button_pressed_cb_t pressed_cb);
