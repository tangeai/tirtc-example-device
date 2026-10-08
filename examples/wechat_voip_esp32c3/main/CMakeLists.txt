idf_component_register(SRCS "app_main.c" "boot_button.c" INCLUDE_DIRS "."
                       REQUIRES demo_platform wechat_voip nvs_flash memory_probe esp_driver_gpio)
