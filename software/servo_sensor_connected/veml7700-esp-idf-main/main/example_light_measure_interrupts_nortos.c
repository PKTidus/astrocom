// YOU NEED TO DISABLE WATCHDOG TIMER IN MENUCONFIG TO RUN THIS CODE
// Component config -> ESP System settings -> Disable Task Watchdog Timer

#include <stdio.h>
#include "driver/i2c.h"
#include "veml7700.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_timer.h"
//----------------------------------test
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"              // task call library
#include "freertos/semphr.h"            // Semaphore API
#include "driver/mcpwm_prelude.h" 
#include "managed_components/espressif__esp-zigbee-lib/include/ha/esp_zigbee_ha_standard.h"
#include "managed_components/espressif__esp-zigbee-lib/include/esp_zb_light.h"
#include "nvs_flash.h"

#define I2C_MASTER_SCL_IO 22
#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_NUM I2C_NUM_0
#define TCA9548_ADDR 0x70
#define CHANNEL_1_BIT 0x02

void timer_isr(void* arg);

typedef struct {
    veml7700_handle_t dev;
    double lux;
    double fc;
} VEML_Data;

void i2c_master_init() {
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000; // 100 kHz clock speed
    conf.clk_flags = 0;
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

void select_channel(uint8_t channel) {
    uint8_t cmd = 1 << channel;
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (TCA9548_ADDR << 1) | I2C_MASTER_WRITE, true); // why does this shift TCA9548 left one?
    i2c_master_write_byte(cmd_handle, cmd, true);
    i2c_master_stop(cmd_handle);
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd_handle, 1000 / portTICK_PERIOD_MS);
}

void initialize_veml_device(VEML_Data *data, int channel) {
    esp_err_t init_result = veml7700_initialize(&(data->dev), I2C_MASTER_NUM);
    if (init_result != ESP_OK) {
        ESP_LOGE("VEML7700", "Failed to initialize VEML7700 (Channel %d). Result: %d\n", channel, init_result);
    }
}

void app_main() {
    i2c_master_init();
    printf("ESP32 Initialized for I2C.\n");

    VEML_Data veml_devices[] = {
        { .dev = NULL, .lux = 0, .fc = 0 },  // Placeholder for channel 1 (index 0)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 2 (index 1)
        { .dev = NULL, .lux = 0, .fc = 0 }   // Data for channel 3 (index 2)
    };
    int num_veml_devices = sizeof(veml_devices) / sizeof(VEML_Data);

    for (int i = 1; i < num_veml_devices; i++) { // Start from index 1 as index 0 is placeholder
        select_channel(i + 1); // Channel index starts from 1, so add 1
        //printf("Channel %d on the TCA9548 is now open.\n", i + 1);
        initialize_veml_device(&veml_devices[i], i + 1); // Initialize VEML device for this channel
    }

    // Initialize timer
    esp_timer_create_args_t timer_args = {
        .callback = timer_isr,
        .name = "software_timer",
        .arg = &veml_devices
    };
    esp_timer_handle_t software_timer;
    esp_timer_create(&timer_args, &software_timer);

    // Start timer
    esp_timer_start_periodic(software_timer, 5000000); // 5s timer

    while (1) {
        
    }
}

void timer_isr(void* arg) {
    printf("Software interrupt timer ISR\n");
    VEML_Data* veml_devices = (VEML_Data*)arg;
    for (int i = 1; i < 3; i++) { // Start from index 1, as index 0 is placeholder
        select_channel(i + 1); // Channel index starts from 1, so add 1
        printf("Channel %d on the TCA9548 is now open.\n", i + 1);

        // Read VEML data
        if (veml_devices[i].dev != NULL) {
            double lux = 0, fc = 0;
            esp_err_t read_result = veml7700_read_als_lux_auto(veml_devices[i].dev, &lux);
            if (read_result == ESP_OK) {
                fc = lux * LUX_FC_COEFFICIENT;
                veml_devices[i].lux = lux;
                veml_devices[i].fc = fc;
                printf("Sensor %d (Channel %d) - ALS: %0.4f lux, %0.4f fc\n", i + 1, i + 1, lux, fc);
            } else {
                ESP_LOGE("VEML7700", "Failed to read data from VEML7700 (Channel %d). Result: %d\n", i + 1, read_result);
            }
        } else {
            ESP_LOGE("VEML7700", "VEML7700 device not initialized for Channel %d\n", i + 1);
        }
    }
    printf("\n\n");
}
