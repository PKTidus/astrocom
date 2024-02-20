#include "stdio.h"
#include "freertos/FreeRTOS.h"          // Operating system library
#include "freertos/task.h"              // task call library
#include "freertos/semphr.h"            // Semaphore API
#include "driver/mcpwm_prelude.h"               // PwM to run servos
#include "driver/i2c.h"                 // i2c library
#include "esp_log.h"                    // esp library

#define SERVO_MIN_PULSEWIDTH 500        // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2500       // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE     -135       // Minimum angle
#define SERVO_MAX_DEGREE     135        // Maximum angle >>>270o // rotate counter clockwise   
#define I2C_MASTER_SCL_IO 22            // GPIO pin for I2C clock
#define I2C_MASTER_SDA_IO 21            // GPIO pin for I2C data
#define I2C_MASTER_NUM I2C_NUM_0        // I2C port number
#define PCA9685_ADDR 0x40               // Driver address
#define I2C_MASTER_FREQ_HZ 100000       // !MHz, 1us per tick
#define MODE1   0x00                    // Register mode 1 (standard)  
#define ADDR0   0x06                    // Call first servo register
#define PRESCALE 0xFE                   // prescale bit for servo frequency

//>>> switch to I2C
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

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
    i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS)                                // delay for 1000 ms
    i2c_cmd_link_delete(cmd);                                                                           // delete command
}

void set_servo_angle(uint8_t servo_num, double angle){
    uint16_t pulse_width = SERVO_MIN_PULSEWIDTH + (SERVO_MAX_PULSEWIDTH - SERVO_MIN_PULSEWIDTH)* angle / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE);

    set_pwm(servo_num, 0, pulse_width);
}



// Main function
int main(void)
{
    pca9685_init();

    while(1){

        // Set angles for testing

        set_servo_angle(0,0);                        // set angle of setvo 0  to 0 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
        set_servo_angle(0,90);                        // set angle of setvo 0  to 90 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
        set_servo_angle(0,180);                        // set angle of setvo 0  to 180 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms

        set_servo_angle(1,0);                        // set angle of setvo 1  to 0 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
        set_servo_angle(1,90);                        // set angle of setvo 1  to 90 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
        set_servo_angle(1,180);                        // set angle of setvo 1  to 180 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms

        set_servo_angle(2,0);                        // set angle of setvo 2  to 0 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
        set_servo_angle(2,90);                        // set angle of setvo 2  to 90 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
        set_servo_angle(2,180);                        // set angle of setvo 2  to 180 degree
        vTaskDelay(pdMS_TO_TICKS(100));               // delay 100ms
    }




    // ESP_LOGI(TAG, "Create timer and operator");
    // mcpwm_timer_handle_t timer = NULL;
    // mcpwm_timer_config_t timer_config = {
    //     .group_id = 0,
    //     .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
    //     .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
    //     .period_ticks = SERVO_TIMEBASE_PERIOD,
    //     .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    // };
    // ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    // mcpwm_oper_handle_t oper = NULL;
    // mcpwm_operator_config_t operator_config = {
    //     .group_id = 0, // operator must be in the same group to the timer
    // };
    // ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    // ESP_LOGI(TAG, "Connect timer and operator");
    // ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    // ESP_LOGI(TAG, "Create comparator and generator from the operator");
    // mcpwm_cmpr_handle_t comparator = NULL;
    // mcpwm_comparator_config_t comparator_config = {
    //     .flags.update_cmp_on_tez = true,
    // };
    // ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    // mcpwm_gen_handle_t generator = NULL;
    // mcpwm_generator_config_t generator_config = {
    //     .gen_gpio_num = SERVO_PULSE_GPIO,
    // };
    // ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // // set the initial compare value, so that the servo will spin to the center position
    // ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(0)));

    // ESP_LOGI(TAG, "Set generator action on timer and compare event");
    // // go high on counter empty
    // ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
    //                 MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // // go low on compare threshold
    // ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
    //                 MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    // ESP_LOGI(TAG, "Enable and start timer");
    // ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    // ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    // int angle = 0;
    // int step = 2;
    // while (1) {
    //     ESP_LOGI(TAG, "Angle of rotation: %d", angle);
    //     ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(angle)));
    //     //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
    //     vTaskDelay(pdMS_TO_TICKS(50));
    //     if ((angle + step) > 15 || (angle + step) < -15) {
    //         step *= -1;
    //     }
    //     angle += step;
    // }
}
