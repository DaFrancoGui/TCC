# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(idroid)

# Reject an incorrect dependency/configuration before compiling graphics assets.
# LV_COLOR_16_SWAP is provided by LVGL 8, not by the ESP-IDF core Kconfig.
idf_component_get_property(idroid_lvgl_version lvgl__lvgl COMPONENT_VERSION)
if(NOT "${idroid_lvgl_version}" STREQUAL "8.3.11")
    message(FATAL_ERROR "iDroid requires LVGL 8.3.11; resolved '${idroid_lvgl_version}'. Install tests/requirements-build.txt and preserve dependencies.lock.")
endif()
if(NOT CONFIG_LV_CONF_SKIP OR NOT CONFIG_LV_COLOR_DEPTH_16 OR NOT CONFIG_LV_COLOR_16_SWAP)
    message(FATAL_ERROR "iDroid requires LVGL Kconfig with RGB565 and LV_COLOR_16_SWAP=y. Configure from sdkconfig.defaults with the pinned Python build tools.")
endif()
