#Para usar main_clock.c (relógio original), substitua por:
#idf_component_register(SRCS "main_clock.c"
#                    INCLUDE_DIRS "."
#                    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl)

# UI test com SquareLine Studio
file(GLOB UI_SRCS "ui/*.c" "ui/screens/*.c" "ui/images/*.c")
idf_component_register(SRCS "main_ui_test.c" "font_sharetechmono_32.c" ${UI_SRCS}
                    INCLUDE_DIRS "." "ui" "ui/screens"
                    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl)

# Para usar main_touch_oficial.c, substitua por:
#idf_component_register(SRCS "main_touch_oficial.c"
#                    INCLUDE_DIRS "."
#                    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch)
