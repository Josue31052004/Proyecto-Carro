#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "car.h"
#include "driver/i2c_slave.h"

//I2C
#define I2C_SLAVE_SCL_IO      18
#define I2C_SLAVE_SDA_IO      17
#define I2C_SLAVE_PORT        I2C_NUM_0
#define I2C_SLAVE_ADDR        0x08
#define I2C_COLA_LEN          16

static const char *TAG = "I2C_SLAVE";

app_context_t context = {
    .handle = NULL,
    .event_queue = NULL
};


// Interrupcion: el driver la llama cuando el maestro termina una escritura.
// Solo mete los bytes en la cola; el lazo de control los procesa.
static bool i2c_slave_receive_cb(i2c_slave_dev_handle_t i2c_slave, const i2c_slave_rx_done_event_data_t *evt_data, void *arg)
{
    app_context_t *ctx = (app_context_t *)arg;
    BaseType_t xTaskWoken = pdFALSE;

    for (uint32_t i = 0; i < evt_data->length; i++) {
        xQueueSendFromISR(ctx->event_queue, &evt_data->buffer[i], &xTaskWoken);
    }

    return (xTaskWoken == pdTRUE);
}


void communication_init(void){
    context.event_queue = xQueueCreate(I2C_COLA_LEN, sizeof(uint8_t));
    if (context.event_queue == NULL) {
        ESP_LOGE(TAG, "Error: No se pudo crear la cola de FreeRTOS");
        return;
    }

    i2c_slave_config_t i2c_slv_config = {
        .i2c_port = I2C_SLAVE_PORT,
        .sda_io_num = I2C_SLAVE_SDA_IO,
        .scl_io_num = I2C_SLAVE_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .slave_addr = I2C_SLAVE_ADDR,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7,
        .send_buf_depth = 32,
        .receive_buf_depth = 32,
    };
    ESP_ERROR_CHECK(i2c_new_slave_device(&i2c_slv_config, &context.handle));

    i2c_slave_event_callbacks_t cbs = {
        .on_receive = i2c_slave_receive_cb,
    };
    ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(context.handle, &cbs, &context));

    ESP_LOGI(TAG, "Esclavo I2C listo en 0x%02X (SDA %d, SCL %d)", I2C_SLAVE_ADDR, I2C_SLAVE_SDA_IO, I2C_SLAVE_SCL_IO);
}


// Saca un codigo de la cola sin bloquear. Devuelve false si no hay nada pendiente.
bool communication_recibir_codigo(uint8_t *codigo){
    if (context.event_queue == NULL) {
        return false;
    }
    return xQueueReceive(context.event_queue, codigo, 0) == pdTRUE;
}
