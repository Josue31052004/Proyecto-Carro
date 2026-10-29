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

#define TIEMPO_FRENADO_MS 10000     // accion frenar: 10 s detenido
#define TIEMPO_RETROCESO_MS 3000    // accion retroceder: 3 s en reversa
#define TIEMPO_BIFURCACION_MS 1500  // tiempo que se sigue el ramal elegido despues del aviso de bifurcacion

typedef enum {
    ESTADO_DETENIDO,
    ESTADO_SIGUIENDO,
    ESTADO_FRENANDO,
    ESTADO_RETROCEDIENDO
} estado_carro_t;

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

static estado_carro_t estado = ESTADO_DETENIDO;
static TickType_t accion_inicio = 0;      // tick en que empezo el frenado o el retroceso
static bool velocidad_rapida = true;
static int bifurcacion_lado = 0;          // 1 derecha, -1 izquierda, 0 sin bifurcacion activa
static TickType_t bifurcacion_inicio = 0;

static float last_position = 0.0f;  // ultima posicion donde un sensor vio la linea
static float trend = 0.0f;          // hacia donde se movia la linea (-1 izquierda, 1 derecha)
static float error_hist[D_WINDOW] = {0};  // historial de errores para la derivada
static int hist_idx = 0;


static void reset_pid(void){
    error = 0.0f;
    last_error = 0.0f;
    suma_error = 0.0f;
    last_position = 0.0f;
    trend = 0.0f;
    memset(error_hist, 0, sizeof(error_hist));
    hist_idx = 0;
}

// posicion del grupo de sensores activos mas cercano al lado elegido, para tomar un ramal de la Y
// d[] va de izquierda a derecha: L2, L1, C, R1, R2 (pesos -2 a 2)
static float posicion_lado(const int *d, int lado){
    int i = lado > 0 ? 4 : 0;
    int paso = lado > 0 ? -1 : 1;
    float suma = 0.0f;
    int n = 0;

    for(; i >= 0 && i < 5; i += paso){
        if(d[i]){
            suma += (float)(i - 2);
            n++;
        }
        else if(n > 0){
            break;  // termino el primer grupo, lo demas es el otro ramal
        }
    }
    return suma / (float)n;
}

static const char *nombre_estado(estado_carro_t e){
    switch(e){
        case ESTADO_DETENIDO:      return "DETENIDO";
        case ESTADO_SIGUIENDO:     return "SIGUIENDO";
        case ESTADO_FRENANDO:      return "FRENANDO";
        case ESTADO_RETROCEDIENDO: return "RETROCEDIENDO";
    }
    return "?";
}

static const char *nombre_codigo(uint8_t codigo){
    switch(codigo){
        case CODIGO_INICIO:        return "INICIO";
        case CODIGO_FIN:           return "FIN";
        case CODIGO_FRENAR:        return "FRENAR";
        case CODIGO_RETROCEDER:    return "RETROCEDER";
        case CODIGO_VELOCIDAD:     return "VELOCIDAD";
        case CODIGO_BIF_DERECHA:   return "BIF_DERECHA";
        case CODIGO_BIF_IZQUIERDA: return "BIF_IZQUIERDA";
    }
    return "DESCONOCIDO";
}

static void procesar_codigo(uint8_t codigo, TickType_t ahora){
    // Fin se obedece siempre; Inicio solo si esta detenido; las acciones solo mientras va siguiendo la linea
    estado_carro_t estado_previo = estado;
    bool aceptado = true;

    ESP_LOGI("I2C", "Recibido 0x%02X (%s) en estado %s", codigo, nombre_codigo(codigo), nombre_estado(estado));

    switch(codigo){
        case CODIGO_INICIO:
            if(estado == ESTADO_DETENIDO){
                reset_pid();
                bifurcacion_lado = 0;
                estado = ESTADO_SIGUIENDO;
            }
            else{
                aceptado = false;
            }
            break;
        case CODIGO_FIN:
            estado = ESTADO_DETENIDO;
            break;
        case CODIGO_FRENAR:
            if(estado == ESTADO_SIGUIENDO){
                estado = ESTADO_FRENANDO;
                accion_inicio = ahora;
            }
            else{
                aceptado = false;
            }
            break;
        case CODIGO_RETROCEDER:
            if(estado == ESTADO_SIGUIENDO){
                estado = ESTADO_RETROCEDIENDO;
                accion_inicio = ahora;
            }
            else{
                aceptado = false;
            }
            break;
        case CODIGO_VELOCIDAD:
            if(estado != ESTADO_DETENIDO){
                velocidad_rapida = !velocidad_rapida;
                motor_set_velocidad_rapida(velocidad_rapida);
                ESP_LOGI("I2C", "Velocidad: %s", velocidad_rapida ? "RAPIDA" : "LENTA");
            }
            else{
                aceptado = false;
            }
            break;
        case CODIGO_BIF_DERECHA:
        case CODIGO_BIF_IZQUIERDA:
            if(estado == ESTADO_SIGUIENDO){
                bifurcacion_lado = codigo == CODIGO_BIF_DERECHA ? 1 : -1;
                bifurcacion_inicio = ahora;
            }
            else{
                aceptado = false;
            }
            break;
        default:
            ESP_LOGW("I2C", "Codigo desconocido 0x%02X, se ignora", codigo);
            return;
    }

    if(!aceptado){
        ESP_LOGW("I2C", "%s ignorado: no aplica en estado %s", nombre_codigo(codigo), nombre_estado(estado));
    }
    else if(estado != estado_previo){
        ESP_LOGI("I2C", "Estado: %s -> %s", nombre_estado(estado_previo), nombre_estado(estado));
    }
}

void pid_sensor_control(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();
    int log_count = 0;

    while (1)
    {
        TickType_t ahora = xTaskGetTickCount();

        uint8_t codigo;
        while(communication_recibir_codigo(&codigo)){
            procesar_codigo(codigo, ahora);
        }

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

        if(bifurcacion_lado != 0 && (ahora - bifurcacion_inicio) >= pdMS_TO_TICKS(TIEMPO_BIFURCACION_MS)){
            bifurcacion_lado = 0;
        }

        if(suma_sensores > 0){
            if(bifurcacion_lado != 0){
                // en la Y se ven los dos ramales: seguir solo el del lado elegido
                int d[5] = {digital_l2, digital_l1, digital_c, digital_r1, digital_r2};
                position = posicion_lado(d, bifurcacion_lado);
            }
            else{
                position = (digital_l2*-2.0f + digital_l1*-1.0f + digital_c*0.0f + digital_r1*1.0f + digital_r2*2.0f) / (float)suma_sensores;
            }
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

        switch(estado){
            case ESTADO_DETENIDO:
                motor_brake();
                break;
            case ESTADO_FRENANDO:
                motor_brake();
                if((ahora - accion_inicio) >= pdMS_TO_TICKS(TIEMPO_FRENADO_MS)){
                    estado = ESTADO_SIGUIENDO;
                    ESP_LOGI("I2C", "Fin del frenado -> SIGUIENDO");
                }
                break;
            case ESTADO_RETROCEDIENDO:
                motor_set_direccion(MOTOR_ATRAS);
                motor_set(u);
                if((ahora - accion_inicio) >= pdMS_TO_TICKS(TIEMPO_RETROCESO_MS)){
                    estado = ESTADO_SIGUIENDO;
                    ESP_LOGI("I2C", "Fin del retroceso -> SIGUIENDO");
                }
                break;
            case ESTADO_SIGUIENDO:
                motor_set_direccion(MOTOR_ADELANTE);
                motor_set(u);
                break;
        }

        if(++log_count >= LOG_EVERY){
            log_count = 0;
            ESP_LOGI("SENSORES", "R2: %d, R1: %d, C: %d, L1: %d, L2: %d", sensor_r2, sensor_r1, sensor_c, sensor_l1, sensor_l2);
            ESP_LOGI("PID", "Estado: %s, Error: %.2f, P: %.2f, D: %.2f, U: %.2f", nombre_estado(estado), error, p_term, d_term, u);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LOOP_MS));
    }
}
