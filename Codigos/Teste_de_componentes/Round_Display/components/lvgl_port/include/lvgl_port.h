/**
 * @file lvgl_port.h
 * @brief LVGL port interface for Round Display
 */

#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize LVGL display driver
     * @return ESP_OK on success
     */
    esp_err_t lvgl_port_display_init(void);

    /**
     * @brief Initialize LVGL input device (touchscreen)
     * @return ESP_OK on success
     */
    esp_err_t lvgl_port_indev_init(void);

    /**
     * @brief Complete LVGL port initialization (display + touch)
     * @return ESP_OK on success
     */
    esp_err_t lvgl_port_init(void);

#ifdef __cplusplus
}
#endif

#endif // LVGL_PORT_H
