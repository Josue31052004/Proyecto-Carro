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

#define umbralr2 300
#define umbralr1 300
#define umbralc 300
#define umbral1 300
#define umbral2 300

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
    float last_side = 0.0f;  // ultimo lado donde se vio la linea (-1 izquierda, 1 derecha)
    int log_count = 0;

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
            if(error != 0.0f){
                last_side = error > 0.0f ? 1.0f : -1.0f;
            }
        }
        else{
            // linea perdida: girar fuerte hacia el lado donde se vio por ultima vez
            error = error_perdido * last_side;
        }

        p_term = error*kp;
        suma_error = suma_error + error;
        i_term = suma_error*ki;
        d_term = (error - last_error)*kd;

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
