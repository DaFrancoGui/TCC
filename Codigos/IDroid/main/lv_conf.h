/* The ESP-IDF build uses sdkconfig via CONFIG_LV_CONF_SKIP=y.
 * Do not define competing LVGL options here. */
#pragma once
#include "sdkconfig.h"
#if !CONFIG_LV_CONF_SKIP
#error "Configure LVGL through menuconfig (LV_CONF_SKIP=y)"
#endif
