# iDroid — firmware principal
#
# Organizacao modular por funcionalidade:
#   main.c            → nucleo (init de HW, navegacao, loop)
#   relogio/          → watchface + RTC + fonte
#   sensores/<nome>/  → cada sensor (hardware + processamento + tela)
#   ui/               → base gerada no SquareLine Studio

file(GLOB_RECURSE MODULE_SRCS
    "relogio/*.c"
    "sensores/*.c"
    "ui/*.c"
)

idf_component_register(
    SRCS "main.c" ${MODULE_SRCS}
    INCLUDE_DIRS
        "."
        "ui"
        "ui/screens"
        "relogio"
        "sensores/max30102"
    REQUIRES driver esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl
)
