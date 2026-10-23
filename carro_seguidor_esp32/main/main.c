#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/i2c_slave.h"
#include "freertos/queue.h"
#include "car.h"

//LEDC
#define LEDC_FRECUENCY 5000
#define motor_pin_left 1
#define motor_pin_right 2
#define motor_IN1 42
#define motor_IN2 41
#define motor_IN3 47 
#define motor_IN4 21
#define motor_ldc_channel_1 LEDC_CHANNEL_0
#define motor_ldc_channel_2 LEDC_CHANNEL_1

//I2C
#define I2C_SLAVE_SCL_IO      18 //22
#define I2C_SLAVE_SDA_IO      17 //21
#define I2C_SLAVE_PORT        I2C_NUM_0
#define I2C_SLAVE_ADDR        0x28

app_context_t context = {
    .handle = NULL,
    .event_queue = NULL
};

adc_oneshot_unit_handle_t adc1_handle = NULL;

void app_main(void)
{

    /*------------------CONFIGURE ADC-------------------*/
    adc_oneshot_unit_init_cfg_t adc1_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc1_config, &adc1_handle));

    adc_oneshot_chan_cfg_t adc1_chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &adc1_chan_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_4, &adc1_chan_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_2, &adc1_chan_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_8, &adc1_chan_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_9, &adc1_chan_config));

    /*------------------END CONFIGURE ADC-------------------*/


    /*------------------CONFIGURE LEDC-------------------*/

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = LEDC_FRECUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = motor_ldc_channel_1,
        .duty = 0,
        .gpio_num = motor_pin_left,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel_config_t ledc_channel_2 = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = motor_ldc_channel_2,
        .duty = 0,
        .gpio_num = motor_pin_right,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_2));

    gpio_config_t motor_in1_config = {
        .pin_bit_mask = (1ULL << motor_IN1),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&motor_in1_config));

    gpio_config_t motor_in2_config = {
        .pin_bit_mask = (1ULL << motor_IN2),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&motor_in2_config));

    gpio_config_t motor_in3_config = {
        .pin_bit_mask = (1ULL << motor_IN3),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&motor_in3_config));

    gpio_config_t motor_in4_config = {
        .pin_bit_mask = (1ULL << motor_IN4),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&motor_in4_config));

    /*------------------END CONFIGURE LEDC-------------------*/

    /*------------------CONFIGURE I2C-------------------*/

    context.event_queue = xQueueCreate(10, sizeof(i2c_slave_event_t));
    if (context.event_queue == NULL) {
        ESP_LOGE("MAIN", "Error: No se pudo crear la cola de FreeRTOS");
        return;
    }
    
    i2c_slave_config_t i2c_slv_config = {
        .i2c_port = I2C_SLAVE_PORT,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .slave_addr = I2C_SLAVE_ADDR,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7,
        .send_buf_depth = 100,
    };

    ESP_ERROR_CHECK(i2c_new_slave_device(&i2c_slv_config, &context.handle));


    /*------------------END CONFIGURE I2C-------------------*/

    i2c_slave_event_callbacks_t cbs = {
        .on_recv_done = i2c_slave_receive_cb,
    };
    ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(context.handle, &cbs, &context));


    motor_init();

    xTaskCreate(&pid_sensor_control, "pid_sensor_control", 4096, &context, 5, NULL);

}


static bool i2c_slave_receive_cb(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_rx_done_event_data_t *evt_data, void *arg)
{
    app_context_t *ctx = (app_context_t *)arg;
    i2c_slave_event_t evt = I2C_SLAVE_EVT_RX;
    BaseType_t xTaskWoken = pdFALSE;

    if (ctx && ctx->event_queue) {
        xQueueSendFromISR(ctx->event_queue, &evt, &xTaskWoken);
    }

    return (xTaskWoken == pdTRUE);
}
