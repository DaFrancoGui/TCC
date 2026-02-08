#Para usar main_oficial.c, substitua por:
idf_component_register(SRCS "main_clock.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl)

# Para usar main_touch_oficial.c, substitua por:
#idf_component_register(SRCS "main_touch_oficial.c"
#                    INCLUDE_DIRS "."
#                    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch)
