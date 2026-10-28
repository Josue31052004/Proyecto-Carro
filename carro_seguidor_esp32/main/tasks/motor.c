#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "car.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#define base_speed 550
#define base_speed_lenta 275  // segunda velocidad: la mitad de base_speed
#define max_duty 1023
#define curve_slowdown 0.5f  // cuanto baja la velocidad base por cada unidad de |u| (0 = no frena en curvas)
#define min_base_speed 250   // velocidad base minima en curvas cerradas
#define motor_ldc_channel_1 LEDC_CHANNEL_0
#define motor_ldc_channel_2 LEDC_CHANNEL_1
#define motor_IN1 42
#define motor_IN2 41
#define motor_IN3 47
#define motor_IN4 21

static int velocidad_base = base_speed;
static motor_direccion_t direccion_actual = MOTOR_FRENO;


void motor_set_direccion(motor_direccion_t direccion){
    if(direccion == direccion_actual){
        return;
    }
    direccion_actual = direccion;

    switch(direccion){
        case MOTOR_ADELANTE:
            gpio_set_level(motor_IN1, 0);
            gpio_set_level(motor_IN2, 1);
            gpio_set_level(motor_IN3, 0);
            gpio_set_level(motor_IN4, 1);
            break;
        case MOTOR_ATRAS:
            gpio_set_level(motor_IN1, 1);
            gpio_set_level(motor_IN2, 0);
            gpio_set_level(motor_IN3, 1);
            gpio_set_level(motor_IN4, 0);
            break;
        case MOTOR_FRENO:
            gpio_set_level(motor_IN1, 1);
            gpio_set_level(motor_IN2, 1);
            gpio_set_level(motor_IN3, 1);
            gpio_set_level(motor_IN4, 1);
            break;
    }
}

void motor_init(void){
    // arranca frenado hasta recibir el codigo de Inicio
    direccion_actual = MOTOR_ADELANTE;  // fuerza a que motor_brake escriba los pines
    motor_brake();
}

void motor_brake(void){
    motor_set_direccion(MOTOR_FRENO);
    // PWM al maximo para que el freno sea activo (en un L298N con EN en bajo el motor queda suelto)
    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_1, max_duty);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_2, max_duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_1);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, motor_ldc_channel_2);
}

void motor_set_velocidad_rapida(bool rapida){
    velocidad_base = rapida ? base_speed : base_speed_lenta;
}

void motor_set(float u){
    // en reversa la correccion se invierte: para mover la parte de los sensores hacia un lado
    // hay que acelerar la rueda contraria a la que se acelera hacia adelante
    if(direccion_actual == MOTOR_ATRAS){
        u = -u;
    }

    // frenar en curvas: entre mas correccion, menor velocidad base
    float minimo = velocidad_base < min_base_speed ? velocidad_base : min_base_speed;
    float base = velocidad_base - curve_slowdown*fabsf(u);
    if(base < minimo){
        base = minimo;
    }

    // int con signo para que el limite inferior funcione
    int speed_left = (int)(base + u);
    int speed_right = (int)(base - u);

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
