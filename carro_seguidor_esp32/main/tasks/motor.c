#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "car.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define base_speed 400
#define max_duty 1023
#define motor_ldc_channel_1 LEDC_CHANNEL_0
#define motor_ldc_channel_2 LEDC_CHANNEL_1
#define motor_IN1 42
#define motor_IN2 41
#define motor_IN3 47
#define motor_IN4 21


void motor_init(void){
    gpio_set_level(motor_IN1, 0);
    gpio_set_level(motor_IN3, 0);
    gpio_set_level(motor_IN2, 1);
    gpio_set_level(motor_IN4, 1);
}

void motor_set(float u){
    // int con signo para que el limite inferior funcione
    int speed_left = (int)(base_speed + u);
    int speed_right = (int)(base_speed - u);

    if(speed_left > max_duty){
        speed_left = max_duty;
    }
    if(speed_right > max_duty){
        speed_right = max_duty;
    }
    if(speed_left < 0){
        speed_left = 0;
    }
    if(speed_right < 0){
        speed_right = 0;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_1, speed_left);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_2, speed_right);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_1);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_2);
}
