#ifndef CAR_H
#define CAR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/i2c_slave.h"
#include "freertos/queue.h"



extern adc_oneshot_unit_handle_t adc1_handle;



//I2C
typedef enum {
    I2C_SLAVE_EVT_RX,
    I2C_SLAVE_EVT_TX
} i2c_slave_event_t;

typedef struct {
    i2c_slave_dev_handle_t handle;
    QueueHandle_t event_queue; // Handle de la cola
} app_context_t;

extern app_context_t context;



void pid_sensor_control(void *pvParameters);

void motor_init(void);
void motor_set(float u);

void communication_control(void *pvParameters);
static bool i2c_slave_receive_cb(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_rx_done_event_data_t *evt_data, void *arg);



#endif