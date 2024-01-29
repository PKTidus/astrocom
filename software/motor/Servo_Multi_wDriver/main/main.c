#include "freertos/FreeRTOS.h"          //RTOs library>> from package
#include "freertos/task.h"              //RTOs tasks
#include "esp_log.h"                    //esp library
#include "driver/mcpwm_prelude.h"       //mcpwm for servo driver
#include "driver/i2c_master.h"          //i2c library

#define SERVO_MIN_PULSEWIDTH 500        // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH 2500       // Maximum pulse width in microsecond
#define SERVO_NEUTRAL        1500       // Servo Neutral position pulse 
#define SERVO_MIN_DEGREE     -135        // Minimum angle
#define SERVO_MAX_DEGREE     135         // Maximum angle >>>270o // rotate counter clockwise   

//>>> switch to I2C
// #define SERVO_PULSE_GPIO             0        // GPIO connects to the PWM signal line
// #define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
// #define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms


// Angle check function>>> return acceptable range of servo angle
static inline uint32_t example_angle_to_compare (int angle){
    return (angle-SERVO_MIN_DEGREE)*(SERVO_MAX_PULSEWIDTH-SERVO_MIN_PULSEWIDTH)/(SERVO_MAX_DEGREE-SERVO_MIN_DEGREE)+SERVO_MIN_PULSEWIDTH;
}

// Main function

void main(void){

    // Main rotation operation
    int angle = 0;      // starting angle
    int step = 2;       // smallest step between each rotation

    while(1){           //infinite loops for rotation =>> need to implement interrupt to input from sensor


        // when angle reach maximum or minimum degree change direction
        if((angle+step)>15 || (angle + step) < -15){
            step *= -1;
        }

    }

}