idf_component_register(
    SRCS "lvgl_port_disp.c"
         "lvgl_port_indev.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl gc9a01 driver
)
