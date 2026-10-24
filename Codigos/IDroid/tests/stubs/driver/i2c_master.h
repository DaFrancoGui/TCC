#pragma once
#include "esp_err.h"
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
typedef struct { int dev_addr_length, device_address, scl_speed_hz; } i2c_device_config_t;
#define I2C_ADDR_BIT_LEN_7 7
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,const i2c_device_config_t *cfg,i2c_master_dev_handle_t *dev);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev);
