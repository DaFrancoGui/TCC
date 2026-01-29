idf_component_register(SRCS "main_official.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl)
