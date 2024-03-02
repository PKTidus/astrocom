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

#define I2C_MASTER_SCL_IO 10            // GPIO for sensor I2C clock
#define I2C_MASTER_SDA_IO 11            // GPIO for sensor I2C data
#define I2C_MASTER_NUM I2C_NUM_0
#define TCA9548_ADDR 0x71
//-----------------------------------------------------------------------------------------
#define CHANNEL_1_BIT 0x02
#define SERVO_MIN_PULSEWIDTH 500        // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2500       // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE     -135       // Minimum angle
#define SERVO_MAX_DEGREE     135        // Maximum angle >>>270o // rotate counter clockwise   
//#define I2C_MASTER_SCL_IO 22            // GPIO pin for servo I2C clock
//#define I2C_MASTER_SDA_IO 21            // GPIO pin for servo I2C data
//#define I2C_MASTER_NUM I2C_NUM_0      // I2C port number
#define PCA9685_ADDR 0x40               // Driver address
#define I2C_MASTER_FREQ_HZ 100000       // !MHz, 1us per tick
#define MODE1   0x00                    // Register mode 1 (standard)  
#define ADDR0   0x06                    // Call first servo register
#define PRESCALE 0xFE                   // prescale bit for servo frequency

//>>> switch to I2C
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

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


void pca9685_init() {
    i2c_config_t conf;                                          // set up configuartion for I2C
    conf.mode = I2C_MODE_MASTER;                                // Switch to master mode
    conf.sda_io_num = I2C_MASTER_SDA_IO;                        // set SDA pin
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;                    // pull up SDA
    conf.scl_io_num = I2C_MASTER_SCL_IO;                        // set SCL pin
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;                    // pull up SCL
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;                 // 100 kHz clock speed
    conf.clk_flags = 0;                                         // Choosing a source clock <optional>
    i2c_param_config(I2C_MASTER_NUM, &conf);                    // set other parameter to defautl value
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);     // install driver for master, port (0), the rest of param are default

    // set PCA9865 to normal MODE

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();                                                       // create new command
    i2c_master_start(cmd);                                                                              // start command
    i2c_master_write_byte(cmd, (PCA9685_ADDR << 1) | I2C_MASTER_WRITE, true);                           // write to PCA9685
    i2c_master_write_byte(cmd, PRESCALE, true);                                                         // Set to frequency to prescale of 50Hz
    i2c_master_stop(cmd);                                                                               // stop command
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);                                 // delay for 1000 ms
    i2c_cmd_link_delete(cmd);                                                                           // delete command

    // set PWM frequency to prescale value

    cmd = i2c_cmd_link_create();                                                       // create new command
    i2c_master_start(cmd);                                                                              // start command
    i2c_master_write_byte(cmd, (PCA9685_ADDR << 1) | I2C_MASTER_WRITE, true);                           // write to PCA9685
    i2c_master_write_byte(cmd, MODE1, true);                                                            // Set to normal mode
    i2c_master_stop(cmd);                                                                               // stop command
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);                                 // delay for 1000 ms
    i2c_cmd_link_delete(cmd);                                                                           // delete command

}

void set_pwm(uint8_t channel, uint16_t on, uint16_t off) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();                                                       // create new command
    i2c_master_start(cmd);                                                                              // start command
    i2c_master_write_byte(cmd, (PCA9685_ADDR << 1) | I2C_MASTER_WRITE, true);                    // write to PCA9685 
    i2c_master_write_byte(cmd, ADDR0 + 4 * channel, true);                                              // switching between servo wil call value
    i2c_master_write_byte(cmd, on & 0xFF, true);                                                        // turn on by masking
    i2c_master_write_byte(cmd, on>>8, true);                                                            // check bit 
    i2c_master_write_byte(cmd, off & 0xFF, true);                                                       // turn off by maskeing
    i2c_master_write_byte(cmd, off>>8, true);                                                           // check bit
    i2c_master_stop(cmd);                                                                               // stop command
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);                               // delay for 1000 ms
    i2c_cmd_link_delete(cmd);                                                                           // delete command
}

void set_servo_angle(uint8_t servo_num, double angle){
    uint16_t pulse_width = SERVO_MIN_PULSEWIDTH + (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH)* angle / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE);

    set_pwm(servo_num, 0, pulse_width);
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
    pca9685_init();
    printf("ESP32 Initialized for I2C.\n");

    VEML_Data veml_devices[] = {
        { .dev = NULL, .lux = 0, .fc = 0 },  // Placeholder for channel 1 (index 0)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 2 (index 1)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 3 (index 2)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 4 (index 3)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 5 (index 4)
        { .dev = NULL, .lux = 0, .fc = 0 }   // Data for channel 6 (index 5)
    };
    int num_veml_devices = sizeof(veml_devices) / sizeof(VEML_Data);

    for (int i = 1; i < num_veml_devices; i++) { // Start from index 1 as index 0 is placeholder
        select_channel(i + 1); // Channel index starts from 1, so add 1
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
    for (int i = 1; i < 6; i++) { // Start from index 1, as index 0 is placeholder
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

    set_servo_angle(0,0);                        // set angle of servo 0  to 0 degree
    printf("servo 0 to 0\n");
    vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
    set_servo_angle(0,10);                        // set angle of servo 0  to 0 degree
    printf("servo 0 to 10\n");
    vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
    set_servo_angle(0,0);                        // set angle of servo 0  to 0 degree
    printf("servo 0 to 0\n");
    vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
    set_servo_angle(0,10);                        // set angle of servo 0  to 0 degree
    printf("servo 0 to 10\n");
    vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms

    printf("\n\n");
}
