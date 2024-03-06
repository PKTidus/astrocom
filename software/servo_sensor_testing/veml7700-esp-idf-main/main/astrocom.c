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
#include "freertos/task.h"
// commenting zigbee stuff
//#include "managed_components/espressif__esp-zigbee-lib/include/ha/esp_zigbee_ha_standard.h"
//#include "managed_components/espressif__esp-zigbee-lib/include/esp_zb_light.h"
//#include "nvs_flash.h"

#define I2C_MASTER_SCL_IO 10            // GPIO for sensor I2C clock
#define I2C_MASTER_SDA_IO 11            // GPIO for sensor I2C data
#define I2C_MASTER_NUM I2C_NUM_0
#define TCA9548_ADDR 0x71
//-----------------------------------------------------------------------------------------
#define USBC_PIN GPIO_NUM_12 // Define GPIO pin 12
#define USBA_PIN GPIO_NUM_1 // Define GPIO pin 1
#define STEP_PIN GPIO_NUM_22        // CLK pin 22
#define DIR_PIN GPIO_NUM_23         // DIR pin 23

// PCA9685 I2C address
#define PCA9685_ADDR    0x40

// Registers
#define PCA9685_MODE1   0x00
#define PCA9685_PRESCALE 0xFE
#define LED0_ON_L       0x06
#define LED0_ON_H       0x07
#define LED0_OFF_L      0x08
#define LED0_OFF_H      0x09

#define SERVO_NUM       3

// I2C Configuration
#define I2C_PORT        0
#define I2C_FREQ_HZ     1000000

// Configuration for servo motors
#define SERVO_FREQ_HZ   50
#define SERVO_MIN_US    500
#define SERVO_MAX_US    2500
#define SERVO_MIN_AN    90
#define SERVO_MAX_AN    360

float angle_spin = 0.0;


void pca9685_write_byte(i2c_port_t i2c_num, uint8_t reg_addr, uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (PCA9685_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(i2c_num, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
}

void pca9685_set_pwm(i2c_port_t i2c_num, uint8_t channel, uint16_t on_time, uint16_t off_time) {
    uint8_t reg_addr = LED0_ON_L + 4 * channel;
    uint8_t on_l = on_time & 0xFF;
    uint8_t on_h = (on_time >> 8) & 0x0F;
    uint8_t off_l = off_time & 0xFF;
    uint8_t off_h = (off_time >> 8) & 0x0F;

    pca9685_write_byte(i2c_num, reg_addr, on_l);
    pca9685_write_byte(i2c_num, reg_addr + 1, on_h);
    pca9685_write_byte(i2c_num, reg_addr + 2, off_l);   
    pca9685_write_byte(i2c_num, reg_addr + 3, off_h);

}

void pca9685_init(i2c_port_t i2c_num) {
    pca9685_write_byte(i2c_num, PCA9685_MODE1, 0x00); // Reset PCA9685

    // Set PWM frequency
    uint8_t prescale_val = (uint8_t)(25000000 / (4096 * SERVO_FREQ_HZ) - 1);
    pca9685_write_byte(i2c_num, PCA9685_PRESCALE, prescale_val);

    // Restart PCA9685
    pca9685_write_byte(i2c_num, PCA9685_MODE1, 0xA1);
    vTaskDelay(pdMS_TO_TICKS(5)); // Delay at least 500us after restart

    // Set all channels off initially
    for (int i = 0; i < SERVO_NUM; i++) {
        pca9685_set_pwm(i2c_num, i, 0, 0);
    }
}

void set_servo_angle(i2c_port_t i2c_num, uint8_t channel, double angle) {
    // Calculate pulse length
    uint16_t pulse = (uint16_t)(SERVO_MIN_US + (angle / 180.0) * (SERVO_MAX_US - SERVO_MIN_US));
    uint16_t pwm_val = (uint16_t)(pulse * 4096 / 20000); // 20ms (50Hz) period for servos

    // Set PWM for the servo channel
    pca9685_set_pwm(i2c_num, channel, 0, pwm_val);
}

//-------------------------------------------------------------------------------
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
    conf.master.clk_speed = I2C_FREQ_HZ; // 100 kHz clock speed
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
    else{
        ESP_LOGI("VEML7700", "Successfully initialized VEML7700 (Channel %d). Result: %d\n", channel, init_result);
    }
}
//-----------------------------------------------------------------------
//Set stepper task
void stepper_task() {  
    esp_rom_gpio_pad_select_gpio(STEP_PIN);         //selecting CLK pin
    esp_rom_gpio_pad_select_gpio(DIR_PIN);          //selecting DIR pin
    gpio_set_direction(STEP_PIN, GPIO_MODE_OUTPUT); //set CLK pin to output
    gpio_set_direction(DIR_PIN, GPIO_MODE_OUTPUT);  //set DIR pin to output
    //Pull USBC pin up to switch on power to Servos
    esp_rom_gpio_pad_select_gpio(USBC_PIN);
    gpio_set_direction(USBC_PIN, GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(USBC_PIN, GPIO_PULLUP_ONLY);
    gpio_set_level(DIR_PIN, 1); // Set direction
    for (int i = 0; i < 150; i++) { // 200 steps for one revolution
        gpio_set_level(STEP_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10)); // Adjust delay
        gpio_set_level(STEP_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(10)); // Adjust delay
    }
    vTaskDelay(pdMS_TO_TICKS(200)); // Delay between rotations

    esp_rom_gpio_pad_select_gpio(USBA_PIN);
    gpio_set_direction(USBA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(USBA_PIN, 1);

    gpio_reset_pin(USBC_PIN);

    return;
}
void app_main() {
    esp_rom_gpio_pad_select_gpio(USBA_PIN);
    gpio_set_direction(USBA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(USBA_PIN, 1);

    gpio_reset_pin(USBC_PIN);

    i2c_master_init();
    pca9685_init(I2C_PORT);
    printf("ESP32 Initialized for I2C.\n");

    VEML_Data veml_devices[] = {
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 0 (index 0)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 1 (index 1)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 2 (index 2)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 3 (index 3)
        { .dev = NULL, .lux = 0, .fc = 0 },  // Data for channel 4 (index 4)
    };
    int num_veml_devices = sizeof(veml_devices) / sizeof(VEML_Data);

    for (int i = 0; i < num_veml_devices; i++) { // Start from index 0
        select_channel(i); // Channel index starts from 0
        initialize_veml_device(&veml_devices[i], i); // Initialize VEML device for this channel
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
    esp_timer_start_periodic(software_timer, 1000000); // 5s timer


    while (1) {
        
    }
}

void timer_isr(void* arg) {
    printf("Software interrupt timer ISR\n");
    VEML_Data* veml_devices = (VEML_Data*)arg;
    for (int i = 0; i < 5; i++) { // Start from index p
        select_channel(i); // Channel index starts from 0
        printf("Channel %d on the TCA9548 is now open.\n", i);

        // Read VEML data
        if (veml_devices[i].dev != NULL) {
            double lux = 0, fc = 0;
            esp_err_t read_result = veml7700_read_als_lux_auto(veml_devices[i].dev, &lux);
            if (read_result == ESP_OK) {
                fc = lux * LUX_FC_COEFFICIENT;
                veml_devices[i].lux = lux;
                veml_devices[i].fc = fc;
                printf("Sensor %d (Channel %d) - ALS: %0.4f lux, %0.4f fc\n", i, i, lux, fc);
            } else {
                ESP_LOGE("VEML7700", "Failed to read data from VEML7700 (Channel %d). Result: %d\n", i, read_result);
            }
        } else {
            ESP_LOGE("VEML7700", "VEML7700 device not initialized for Channel %d\n", i);
        }
    }

    float servo_spin_right = (((veml_devices[0].lux + veml_devices[3].lux)/2) / ((veml_devices[1].lux + veml_devices[2].lux)/2));
    float servo_spin_left = (((veml_devices[1].lux + veml_devices[2].lux)/2) / ((veml_devices[0].lux + veml_devices[3].lux)/2));
    float stepper_spin = (((veml_devices[0].lux + veml_devices[1].lux)/2) / ((veml_devices[2].lux + veml_devices[3].lux)/2));
    

    //printf("Servo value is %.4f\n", servo_spin_right);
    printf("Stepper value is %.4f\n", stepper_spin);

    //set_servo_angle(I2C_PORT, 0, 90);
    //vTaskDelay(pdMS_TO_TICKS(1000));

    // set_servo_angle(I2C_PORT, 0, 765);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // set_servo_angle(I2C_PORT, 0, 495);
    // vTaskDelay(pdMS_TO_TICKS(1000));

    if(servo_spin_right < 0.9)
    {
        printf("Servo value is %.4f\n", servo_spin_right);
        angle_spin = 495 + (270*servo_spin_right);
        printf("setting servo step to %.4f\n\n", angle_spin);
        set_servo_angle(I2C_PORT, 0, angle_spin);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    else if(servo_spin_left < 0.9)
    {
        printf("Servo value is %.4f\n", servo_spin_left);
        angle_spin = 765 - (270*servo_spin_left);
        printf("setting servo step to %.4f\n\n", angle_spin);  
        set_servo_angle(I2C_PORT, 0, angle_spin);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if(stepper_spin < 1)
    {

    }
    else if(stepper_spin > 1)
    {

    }

    set_servo_angle(I2C_PORT, 0, angle_spin);
    vTaskDelay(pdMS_TO_TICKS(1000));
    

    /*printf("\n\nBegin Servo spinning\n\n");


    for(int j=SERVO_MIN_AN; j<=SERVO_MAX_AN; j++){
        for(int i=0; i<3; i++){
            set_servo_angle(I2C_PORT, i, j);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    for(int k=SERVO_MAX_AN; k>=SERVO_MIN_AN; k--){
        for(int i=0; i<3; i++){
            set_servo_angle(I2C_PORT, i, k);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    printf("Servo done spinning\n\n");

    //--------------
    //xTaskCreate(stepper_task, "stepper_task", 2048, NULL, 5, NULL);

    printf("\n\nBegin Stepper spinning\n\n");
    stepper_task();
    printf("Stepper done spinning\n\n");*/

}
