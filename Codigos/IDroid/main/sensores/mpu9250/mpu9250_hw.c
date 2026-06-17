/**
 * @file mpu9250_hw.c
 * @brief Driver I2C do MPU-9250 + AK8963 (API nova, barramento compartilhado).
 */

#include "mpu9250_hw.h"
#include "i2c_recover.h"
#include "esp_log.h"
#include "freertos/task.h"

static const char *TAG = "MPU9250_HW";

/* ── Registradores MPU-9250 ── */
#define MPU_WHO_AM_I        0x75
#define MPU_PWR_MGMT_1      0x6B
#define MPU_INT_PIN_CFG     0x37
#define MPU_USER_CTRL       0x6A

/* ── Registradores AK8963 ── */
#define AK_WHO_AM_I         0x00
#define AK_ST1              0x02
#define AK_HXL              0x03
#define AK_ST2              0x09
#define AK_CNTL1            0x0A
#define AK_CNTL2            0x0B
#define AK_ASAX             0x10

#define AK_MODE_POWERDOWN   0x00
#define AK_MODE_CONT2       0x06   /* 100 Hz */
#define AK_MODE_FUSE_ROM    0x0F
#define AK_BIT_16           0x10
#define AK_TIMEOUT_MS       100

static i2c_master_dev_handle_t s_mpu = NULL;
static i2c_master_dev_handle_t s_ak  = NULL;
static SemaphoreHandle_t       s_mutex = NULL;

static float s_asa_x = 1.0f, s_asa_y = 1.0f, s_asa_z = 1.0f;

/* ── Helpers I2C (genericos por device, com mutex) ── */
static esp_err_t reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit(dev, buf, 2, AK_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

static esp_err_t reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len)
{
    esp_err_t ret;
    if (s_mutex) xSemaphoreTake(s_mutex, portMAX_DELAY);
    ret = i2c_master_transmit_receive(dev, &reg, 1, data, len, AK_TIMEOUT_MS);
    if (ret != ESP_OK) i2c_recover_bus();
    if (s_mutex) xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t mpu9250_hw_init(i2c_master_bus_handle_t bus, SemaphoreHandle_t mutex)
{
    esp_err_t ret;
    s_mutex = mutex;

    /* Adiciona o MPU-9250 (0x68) */
    i2c_device_config_t mpu_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MPU9250_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    ret = i2c_master_bus_add_device(bus, &mpu_cfg, &s_mpu);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "add MPU falhou"); return ret; }

    /* WHO_AM_I do MPU */
    uint8_t who = 0;
    if (reg_read(s_mpu, MPU_WHO_AM_I, &who, 1) != ESP_OK) {
        ESP_LOGE(TAG, "MPU nao responde em 0x68");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_LOGI(TAG, "MPU WHO_AM_I=0x%02X", who);
    if (who == 0x70 || who == 0x68) {
        ESP_LOGE(TAG, "Modulo e MPU-6500 (sem magnetometro)");
        return ESP_ERR_NOT_SUPPORTED;
    }

    /* Reset, wake, desabilita I2C master, habilita bypass (expoe AK8963) */
    reg_write(s_mpu, MPU_PWR_MGMT_1, 0x80);  vTaskDelay(pdMS_TO_TICKS(100));
    reg_write(s_mpu, MPU_PWR_MGMT_1, 0x01);  vTaskDelay(pdMS_TO_TICKS(50));
    reg_write(s_mpu, MPU_USER_CTRL, 0x00);   vTaskDelay(pdMS_TO_TICKS(10));
    reg_write(s_mpu, MPU_INT_PIN_CFG, 0x22); vTaskDelay(pdMS_TO_TICKS(50));

    /* Adiciona o AK8963 (0x0C), agora visivel via bypass */
    i2c_device_config_t ak_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = AK8963_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    ret = i2c_master_bus_add_device(bus, &ak_cfg, &s_ak);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "add AK8963 falhou"); return ret; }

    uint8_t ak_who = 0;
    if (reg_read(s_ak, AK_WHO_AM_I, &ak_who, 1) != ESP_OK || ak_who != 0x48) {
        ESP_LOGE(TAG, "AK8963 nao detectado (WHO=0x%02X)", ak_who);
        return ESP_ERR_NOT_FOUND;
    }

    /* AK8963: reset, le ASA no Fuse ROM, depois modo continuo 100 Hz 16-bit */
    reg_write(s_ak, AK_CNTL2, 0x01);                vTaskDelay(pdMS_TO_TICKS(100));
    reg_write(s_ak, AK_CNTL1, AK_MODE_FUSE_ROM);    vTaskDelay(pdMS_TO_TICKS(10));
    uint8_t asa[3] = {128, 128, 128};
    reg_read(s_ak, AK_ASAX, asa, 3);
    s_asa_x = ((float)asa[0] - 128.0f) / 256.0f + 1.0f;
    s_asa_y = ((float)asa[1] - 128.0f) / 256.0f + 1.0f;
    s_asa_z = ((float)asa[2] - 128.0f) / 256.0f + 1.0f;
    reg_write(s_ak, AK_CNTL1, AK_MODE_POWERDOWN);   vTaskDelay(pdMS_TO_TICKS(10));
    reg_write(s_ak, AK_CNTL1, AK_MODE_CONT2 | AK_BIT_16);
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "AK8963 ok (ASA: %.3f %.3f %.3f)", s_asa_x, s_asa_y, s_asa_z);
    return ESP_OK;
}

void mpu9250_hw_get_asa(float *ax, float *ay, float *az)
{
    *ax = s_asa_x; *ay = s_asa_y; *az = s_asa_z;
}

esp_err_t mpu9250_hw_read_mag(int16_t *mx, int16_t *my, int16_t *mz)
{
    uint8_t st1 = 0;
    esp_err_t ret = reg_read(s_ak, AK_ST1, &st1, 1);
    if (ret != ESP_OK) return ret;
    if (!(st1 & 0x01)) return ESP_ERR_NOT_FOUND;   /* DRDY ainda nao */

    uint8_t d[7];
    ret = reg_read(s_ak, AK_HXL, d, 7);             /* 6 bytes + ST2 */
    if (ret != ESP_OK) return ret;
    if (d[6] & 0x08) return ESP_ERR_INVALID_RESPONSE; /* overflow */

    *mx = (int16_t)((d[1] << 8) | d[0]);
    *my = (int16_t)((d[3] << 8) | d[2]);
    *mz = (int16_t)((d[5] << 8) | d[4]);
    return ESP_OK;
}
