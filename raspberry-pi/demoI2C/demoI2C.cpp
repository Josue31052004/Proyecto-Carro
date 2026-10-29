// Demo independiente Linux/Raspberry: una seleccion envia exactamente un byte.
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

constexpr int DIRECCION_ESP32 = 0x08;
const char* nombres[] = {"INICIO", "FRENAR", "RETROCEDER", "CAMBIAR VELOCIDAD",
                         "BIFURCACION DERECHA", "BIFURCACION IZQUIERDA", "FIN"};
constexpr uint8_t codigos[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF};

int main(int argc, char** argv) {
    if (argc > 2) {
        std::cerr << "Uso: ./demoI2C [/dev/i2c-1]\n";
        return 1;
    }
    const char* dispositivo = argc == 2 ? argv[1] : "/dev/i2c-1";
    const int bus = open(dispositivo, O_RDWR);
    if (bus < 0) {
        perror("No se pudo abrir el bus I2C");
        return 1;
    }
    if (ioctl(bus, I2C_SLAVE, DIRECCION_ESP32) < 0) {
        perror("No se pudo seleccionar la direccion 0x08");
        close(bus);
        return 1;
    }
    std::cout << "Bus: " << dispositivo << "; destino: 0x08. Esto NO confirma conexion.\n"
              << "Esta demo transmite I2C real. Receptor de prueba sin motores.\n";
    std::string linea;
    while (true) {
        std::cout << "\n--- Enviar codigo ---\n";
        for (int i = 0; i < 7; ++i)
            std::cout << i << ": " << nombres[i] << " (0x" << std::hex
                      << std::uppercase << std::setw(2) << std::setfill('0')
                      << unsigned(codigos[i]) << std::dec << ")\n";
        std::cout << "q: salir sin enviar nada\nOpcion: " << std::flush;
        if (!std::getline(std::cin, linea)) break;
        std::istringstream entrada(linea);
        std::string opcion, extra;
        if (!(entrada >> opcion) || (entrada >> extra)) {
            std::cout << "Entrada invalida; no se envio nada.\n";
            continue;
        }
        if (opcion == "q" || opcion == "Q") break;
        if (opcion.size() != 1 || opcion[0] < '0' || opcion[0] > '6') {
            std::cout << "Use 0..6 o q; no se envio nada.\n";
            continue;
        }
        const int indice = opcion[0] - '0';
        const uint8_t codigo = codigos[indice];
        const ssize_t enviados = write(bus, &codigo, 1);
        if (enviados != 1) {
            if (enviados < 0) perror("Fallo al escribir I2C");
            else std::cerr << "Transferencia incompleta\n";
            std::cerr << "Sin confirmacion de entrega. No se reintenta automaticamente.\n";
            continue;
        }
        std::cout << "Codigo enviado 0x" << std::hex << std::uppercase
                  << std::setw(2) << std::setfill('0') << unsigned(codigo) << std::dec
                  << " (" << nombres[indice] << "). Verifique el monitor serial de la ESP32.\n";
    }
    close(bus);
    return 0;
}
