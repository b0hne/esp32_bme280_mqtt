/**
 * @file bme280_app.c
 * @brief Read BME280 via I2C (Bosch driver) and publish JSON to MQTT.
 *
 * Publishes to CONFIG_MQTT_STATE_TOPIC as:
 *  {"temperature":21.49,"pressure":1013.25,"humidity":45.00}
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "esp_rom_sys.h"

#include "bme280.h"
#include "net_mqtt.h"

/* ================= I2C / BME280 CONFIG ================= */

#define I2C_MASTER_SCL_IO    6
#define I2C_MASTER_SDA_IO    5
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000
#define BME280_ADDR          0x76

/* ================= I2C INIT ================= */

static void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/* ================= BME280 CALLBACKS ================= */

static int8_t user_i2c_read(uint8_t reg_addr,
                            uint8_t *reg_data,
                            uint32_t len,
                            void *intf_ptr)
{
    uint8_t addr = *(uint8_t*)intf_ptr;

    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        addr,
        &reg_addr, 1,
        reg_data, len,
        pdMS_TO_TICKS(100)
    ) == ESP_OK ? BME280_OK : BME280_E_COMM_FAIL;
}

static int8_t user_i2c_write(uint8_t reg_addr,
                             const uint8_t *reg_data,
                             uint32_t len,
                             void *intf_ptr)
{
    uint8_t addr = *(uint8_t*)intf_ptr;

    uint8_t buffer[len + 1];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], reg_data, len);

    return i2c_master_write_to_device(
        I2C_MASTER_NUM,
        addr,
        buffer, len + 1,
        pdMS_TO_TICKS(100)
    ) == ESP_OK ? BME280_OK : BME280_E_COMM_FAIL;
}

static void user_delay_us(uint32_t period, void *intf_ptr)
{
    (void)intf_ptr;
    esp_rom_delay_us(period);
}

/* ================= MAIN ================= */

void app_main(void)
{
    // Network stack lives in component now
    net_wifi_init_sta_blocking();
    net_mqtt_start_blocking();

    i2c_master_init();

    struct bme280_dev dev;
    struct bme280_data data;
    struct bme280_settings settings = {0};

    uint8_t addr = BME280_ADDR;

    dev.intf = BME280_I2C_INTF;
    dev.read = user_i2c_read;
    dev.write = user_i2c_write;
    dev.delay_us = user_delay_us;
    dev.intf_ptr = &addr;

    if (bme280_init(&dev) != BME280_OK) {
        printf("BME280 init failed\n");
        return;
    }

    // Important: configure oversampling so pressure is compensated correctly
    settings.osr_t = BME280_OVERSAMPLING_1X;
    settings.osr_p = BME280_OVERSAMPLING_1X;
    settings.osr_h = BME280_OVERSAMPLING_1X;
    settings.filter = BME280_FILTER_COEFF_OFF;
    settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

    uint8_t sel = BME280_SEL_OSR_TEMP |
                  BME280_SEL_OSR_PRESS |
                  BME280_SEL_OSR_HUM |
                  BME280_SEL_FILTER |
                  BME280_SEL_STANDBY;

    int8_t rs = bme280_set_sensor_settings(sel, &settings, &dev);
    printf("set_sensor_settings=%d\n", rs);

    while (1)
    {
        // Trigger one measurement
        bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev);

        // Wait until measurement finished (status reg 0xF3, bit3 = measuring)
        uint8_t status = 0;
        do {
            user_i2c_read(0xF3, &status, 1, &addr);
            vTaskDelay(pdMS_TO_TICKS(2));
        } while (status & 0x08);

        if (bme280_get_sensor_data(BME280_ALL, &data, &dev) == BME280_OK)
        {
            char payload[160];

            // pressure from Bosch driver is Pa; convert to hPa
            float pressure_hpa = data.pressure / 100.0f;

            snprintf(payload, sizeof(payload),
                     "{\"temperature\":%.2f,\"pressure\":%.2f,\"humidity\":%.2f}",
                     data.temperature,
                     pressure_hpa,
                     data.humidity);

            // printf("Publishing: %s\n", payload);

            net_mqtt_publish_blocking(CONFIG_MQTT_STATE_TOPIC, payload, 1, false);
        }
        else
        {
            printf("Read failed\n");
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}