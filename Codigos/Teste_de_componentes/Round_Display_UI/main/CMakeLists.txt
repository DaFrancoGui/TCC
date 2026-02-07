# Incluir arquivos da UI do SquareLine Studio
file(GLOB_RECURSE UI_SOURCES "../ui/*.c")

idf_component_register(
    SRCS "main.c" ${UI_SOURCES}
    INCLUDE_DIRS "." "../ui"
    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl
)
