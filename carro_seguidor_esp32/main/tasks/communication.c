#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "car.h"
#include "driver/i2c_slave.h"

//I2C
#define I2C_SLAVE_SCL_IO      22
#define I2C_SLAVE_SDA_IO      21
#define I2C_SLAVE_PORT        I2C_NUM_0
#define I2C_SLAVE_ADDR        0x28


//CODIGOS
#define ROJO 1
#define VERDE 1
#define AZUL 1
#define bifurcacion_derecha 1
#define bifurcacion_izquierda 1

static const char *TAG = "I2C_SLAVE";

void communication_control(void *pvParameters){
    uint32_t evento;

    while (1) {
        // Acceso directo a la variable global g_context
        if (xQueueReceive(context.event_queue, &evento, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Evento %lu recibido mediante g_context", evento);
            
            switch (evento) {
                case I2C_SLAVE_EVT_RX:
                    ESP_LOGI(TAG, "Evento de recepción de datos I2C recibido");

                    break;
            }
        }
    }
}

