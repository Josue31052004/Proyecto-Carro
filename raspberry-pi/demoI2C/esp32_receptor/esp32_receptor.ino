// Arduino-ESP32, ESP32 clasico. Solo recibe e imprime; no controla motores.
#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>

constexpr uint8_t DIRECCION = 0x08;
constexpr int SDA_PIN = 21, SCL_PIN = 22;
QueueHandle_t cola;
std::atomic<unsigned> descartados{0};

// Arduino-ESP32 ejecuta onReceive desde su tarea de eventos.
void recibir(int cantidad) {
    if (cantidad != 1 || !Wire.available()) {
        while (Wire.available()) Wire.read();
        ++descartados;
        return;
    }
    uint8_t codigo = static_cast<uint8_t>(Wire.read());
    if (xQueueSend(cola, &codigo, 0) != pdTRUE) ++descartados;
}

const char* nombre(uint8_t codigo) {
    switch (codigo) {
        case 0x00: return "INICIO";
        case 0x01: return "FRENAR";
        case 0x02: return "RETROCEDER";
        case 0x03: return "CAMBIAR VELOCIDAD";
        case 0x04: return "BIFURCACION DERECHA";
        case 0x05: return "BIFURCACION IZQUIERDA";
        case 0xFF: return "FIN";
        default: return "CODIGO DESCONOCIDO";
    }
}

void setup() {
    Serial.begin(115200);
    cola = xQueueCreate(16, sizeof(uint8_t));
    if (!cola) {
        Serial.println("ERROR: no se pudo crear la cola");
        while (true) delay(1000);
    }
    Wire.onReceive(recibir);
    if (!Wire.begin(DIRECCION, SDA_PIN, SCL_PIN, 100000)) {
        Serial.println("ERROR: no se pudo iniciar I2C esclavo");
        while (true) delay(1000);
    }
    Serial.println("Receptor 0x08 listo. SDA=21, SCL=22. Sin motores.");
}

void loop() {
    uint8_t codigo;
    if (xQueueReceive(cola, &codigo, pdMS_TO_TICKS(20)) == pdTRUE)
        Serial.printf("Codigo recibido 0x%02X: %s\n", unsigned(codigo), nombre(codigo));
    const unsigned perdidos = descartados.exchange(0);
    if (perdidos)
        Serial.printf("AVISO: %u mensajes descartados (longitud invalida o cola llena)\n", perdidos);
}
