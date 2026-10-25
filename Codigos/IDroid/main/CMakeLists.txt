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
    SRCS "main.c" "i2c_recover.c" "screenshot.c" ${MODULE_SRCS}
    INCLUDE_DIRS
        "."
        "ui"
        "ui/screens"
        "relogio"
        "sensores/max30102"
        "sensores/ds18b20"
        "sensores/ltr390"
        "sensores/mpu9250"
    REQUIRES driver esp_adc esp_lcd_gc9a01 chsc6x_touch esp_lvgl_port lvgl ds18b20 onewire_bus nvs_flash
)
