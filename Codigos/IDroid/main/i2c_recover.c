/**
 * @file i2c_recover.c
 * @brief Recuperacao do barramento I2C compartilhado.
 */

#include "i2c_recover.h"

static i2c_master_bus_handle_t s_bus = NULL;

void i2c_recover_set_bus(i2c_master_bus_handle_t bus)
{
    s_bus = bus;
}

void i2c_recover_bus(void)
{
    if (s_bus) i2c_master_bus_reset(s_bus);
}
