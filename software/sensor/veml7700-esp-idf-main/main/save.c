/**
 * @file veml7700_light_sensor.c
 * 
 * @author Kristijan Grozdanovski (kgrozdanovski7@gmail.com)
 * 
 * @brief Vishay VEML7700 Light Sensor driver for integration with ESP-IDF framework.
 * 
 * @version 2
 * 
 * @date 2021-12-11
 * 
 * @copyright Copyright (c) 2022, Kristijan Grozdanovski
 *  Copyright (c) 2020 Ruslan V. Uss <unclerus@gmail.com>
 * All rights reserved.
 * 
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. 
 */
#include "esp_event.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "tca9548.h" // Include the TCA9548 driver


#include "veml7700.h"

#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_MASTER_NUM I2C_NUM_0

/**
 * Function prototypes
 */
void i2c_master_setup(void);
void task_veml7700_read(void *ignore);

/**
 * @brief Standard application entry point.
 */
void app_main()
{
    // Must call before wifi/http functions
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize I2C
    i2c_master_setup();

    xTaskCreate(&task_veml7700_read, "task_veml7700_read",  4096, NULL, 6, NULL);
}

/**
 * @brief Configure the ESP host as an I2C master device.
 */
void i2c_master_setup(void)
{
    i2c_config_t conf;

    // Configure the first I2C bus (I2C_NUM_0)
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = SDA_PIN;
    conf.scl_io_num = SCL_PIN;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = 0;


    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

/**
 * @brief FreeRTOS-compatible task to periodically read and print sensor data.
 * 
 * @param ignore Ignore
 */
void task_veml7700_read(void *ignore)
{
    // Setup the TCA9548 device descriptor
    i2c_dev_t tca9548_dev;
    printf("I made it to this part of the code.");

    esp_err_t tca9548_init_result = tca9548_init_desc(&tca9548_dev, 0x70, I2C_MASTER_NUM, SDA_PIN, SCL_PIN);

    if (tca9548_init_result != ESP_OK) {
        ESP_LOGE("TCA9548", "Failed to initialize TCA9548. Result: %d\n", tca9548_init_result);
        vTaskDelete(NULL);
        return;
    }

    // CODE IS FAILING
    // Initialize the VEML7700 sensor for channel 2
    veml7700_handle_t veml7700_dev_2;
    esp_err_t veml7700_init_result_2 = veml7700_initialize(&veml7700_dev_2, I2C_MASTER_NUM);

    if (veml7700_init_result_2 != ESP_OK) {
        ESP_LOGE("VEML7700_2", "Failed to initialize VEML7700 (Channel 2). Result: %d\n", veml7700_init_result_2);
        tca9548_free_desc(&tca9548_dev); // Clean up TCA9548 resources
        vTaskDelete(NULL);
        return;
    }


    // Initialize the VEML7700 sensor for channel 3
    veml7700_handle_t veml7700_dev_3;
    esp_err_t veml7700_init_result_3 = veml7700_initialize(&veml7700_dev_3, I2C_MASTER_NUM);

    if (veml7700_init_result_3 != ESP_OK) {
        ESP_LOGE("VEML7700_3", "Failed to initialize VEML7700 (Channel 3). Result: %d\n", veml7700_init_result_3);
        tca9548_free_desc(&tca9548_dev); // Clean up TCA9548 resources
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI("VEML7700", "Reading data...\r\n");

    //while (true) {
        double lux_als_2, lux_white_2, fc_als_2, fc_white_2;
        double lux_als_3, lux_white_3, fc_als_3, fc_white_3;

        // Select the TCA9548 channels for the sensors
        uint8_t channel_mask = (1 << 2) | (1 << 3); // Channels 2 and 3
        tca9548_set_channels(&tca9548_dev, channel_mask);

        // Read data from the first sensor (Channel 2)
        ESP_ERROR_CHECK(veml7700_read_als_lux_auto(veml7700_dev_2, &lux_als_2));
        fc_als_2 = lux_als_2 * LUX_FC_COEFFICIENT;

        ESP_ERROR_CHECK(veml7700_read_white_lux_auto(veml7700_dev_2, &lux_white_2));
        fc_white_2 = lux_white_2 * LUX_FC_COEFFICIENT;

        // Read data from the second sensor (Channel 3)
        ESP_ERROR_CHECK(veml7700_read_als_lux_auto(veml7700_dev_3, &lux_als_3));
        fc_als_3 = lux_als_3 * LUX_FC_COEFFICIENT;

        ESP_ERROR_CHECK(veml7700_read_white_lux_auto(veml7700_dev_3, &lux_white_3));
        fc_white_3 = lux_white_3 * LUX_FC_COEFFICIENT;

        printf("Sensor 2 (Channel 2) - ALS: %0.4f lux, %0.4f fc\n", lux_als_2, fc_als_2);
        printf("Sensor 2 (Channel 2) - White: %0.4f lux, %0.4f fc\n", lux_white_2, fc_white_2);

        printf("Sensor 3 (Channel 3) - ALS: %0.4f lux, %0.4f fc\n", lux_als_3, fc_als_3);
        printf("Sensor 3 (Channel 3) - White: %0.4f lux, %0.4f fc\n\n", lux_white_3, fc_white_3);

        vTaskDelay(2000 / portTICK_PERIOD_MS);
        
    //}

    // Clean up resources
    tca9548_free_desc(&tca9548_dev);
    vTaskDelete(NULL);
}
