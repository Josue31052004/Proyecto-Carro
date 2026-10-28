#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "car.h"

#define kp 100
#define ki 0.0
#define kd 150

#define LOOP_MS 2       // periodo del lazo de control
#define LOG_EVERY 100   // imprimir cada N ciclos (100 * 2ms = 200ms)
#define error_perdido 3.0f  // error cuando ningun sensor ve la linea
#define D_WINDOW 10     // la derivada compara contra el error de hace N ciclos (10 * 2ms = 20ms)

#define umbralr2 600
#define umbralr1 600
#define umbralc 600
#define umbral1 600
#define umbral2 600

int sensor_r2 = 0;
int sensor_r1 = 0;
int sensor_c = 0;
int sensor_l1 = 0;
int sensor_l2 = 0;

float error = 0.0f;
float last_error = 0.0f;
float suma_error = 0.0f;

float position = 0.0f;

float p_term = 0.0f;
float i_term = 0.0f;
float d_term = 0.0f;

float u = 0.0f;

void pid_sensor_control(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    float last_position = 0.0f;  // ultima posicion donde un sensor vio la linea
    float trend = 0.0f;          // hacia donde se movia la linea (-1 izquierda, 1 derecha)
    int log_count = 0;
    float error_hist[D_WINDOW] = {0};  // historial de errores para la derivada
    int hist_idx = 0;

    while (1)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_4, &sensor_r2));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &sensor_r1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_2, &sensor_c));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_8, &sensor_l1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_9, &sensor_l2));

        int digital_r2 = sensor_r2 > umbralr2 ? 1 : 0;
        int digital_r1 = sensor_r1 > umbralr1 ? 1 : 0;
        int digital_c = sensor_c > umbralc ? 1 : 0;
        int digital_l1 = sensor_l1 > umbral1 ? 1 : 0;
        int digital_l2 = sensor_l2 > umbral2 ? 1 : 0;

        int suma_sensores = digital_l2 + digital_l1 + digital_c + digital_r1 + digital_r2;

        if(suma_sensores > 0){
            position = (digital_l2*-2.0f + digital_l1*-1.0f + digital_c*0.0f + digital_r1*1.0f + digital_r2*2.0f) / (float)suma_sensores;
            error = position - 0.0f;
            if(position != last_position){
                trend = position > last_position ? 1.0f : -1.0f;
            }
            last_position = position;
        }
        else{
            if(last_position >= 2.0f && trend >= 0.0f){
                // se vio por ultima vez en R2 y se iba hacia afuera: linea perdida por la derecha
                error = error_perdido;
                last_position = error_perdido;  // asi al volver a R2 la tendencia apunta hacia adentro
            }
            else if(last_position <= -2.0f && trend <= 0.0f){
                // se vio por ultima vez en L2 y se iba hacia afuera: linea perdida por la izquierda
                error = -error_perdido;
                last_position = -error_perdido;
            }
            else{
                // la linea esta en el hueco entre dos sensores, medio paso desde la ultima posicion
                error = last_position + 0.5f*trend;
            }
        }

        p_term = error*kp;
        suma_error = suma_error + error;
        i_term = suma_error*ki;
        // derivada sobre una ventana de D_WINDOW ciclos: con sensores digitales el error cambia
        // en escalones y la diferencia de un solo ciclo (2ms) es un pico que el motor no alcanza a sentir
        d_term = (error - error_hist[hist_idx])*kd;
        error_hist[hist_idx] = error;
        hist_idx = (hist_idx + 1) % D_WINDOW;

        u = p_term + i_term + d_term;
        last_error = error;

        motor_set(u);

        if(++log_count >= LOG_EVERY){
            log_count = 0;
            ESP_LOGI("SENSORES", "R2: %d, R1: %d, C: %d, L1: %d, L2: %d", sensor_r2, sensor_r1, sensor_c, sensor_l1, sensor_l2);
            ESP_LOGI("PID", "Error: %.2f, P: %.2f, D: %.2f, U: %.2f", error, p_term, d_term, u);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LOOP_MS));
    }
}
