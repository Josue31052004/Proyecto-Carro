#ifndef CAR_H
#define CAR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/i2c_slave.h"
#include "freertos/queue.h"



extern adc_oneshot_unit_handle_t adc1_handle;



//I2C
// codigos de un byte que envia el maestro I2C; la funcion de cada uno esta en procesar_codigo() de sensor.c
#define CODIGO_INICIO        0x00
#define CODIGO_FIN           0xFF
#define CODIGO_FRENAR        0x01
#define CODIGO_RETROCEDER    0x02
#define CODIGO_VELOCIDAD     0x03
#define CODIGO_BIF_DERECHA   0x04
#define CODIGO_BIF_IZQUIERDA 0x05

typedef struct {
    i2c_slave_dev_handle_t handle;
    QueueHandle_t event_queue; // cola de codigos recibidos (uint8_t)
} app_context_t;

extern app_context_t context;



//MOTORES
typedef enum {
    MOTOR_ADELANTE,
    MOTOR_ATRAS,
    MOTOR_FRENO
} motor_direccion_t;



void pid_sensor_control(void *pvParameters);

void motor_init(void);
void motor_set(float u);
void motor_set_direccion(motor_direccion_t direccion);
void motor_brake(void);
void motor_set_velocidad_rapida(bool rapida);

void communication_init(void);
bool communication_recibir_codigo(uint8_t *codigo);



#endif
