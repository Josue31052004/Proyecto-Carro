# Demo I2C: menu Raspberry y receptor ESP32

Prueba independiente del main, interfaz y camara. Transmite un byte real a la
direccion de 7 bits 0x08. No hay registro, texto hexadecimal ni byte extra.
Pendiente de compilar y validar en las placas reales.

## Archivos

- demoI2C.cpp: menu C++ Linux para la Raspberry, maestro.
- esp32_receptor/esp32_receptor.ino: receptor Arduino-ESP32, solo monitor serial.
  La subcarpeta coincide con el nombre del sketch para abrirlo en Arduino IDE.

## Conexion (ESP32 clasico/WROOM, verificar modelo antes de cablear)

Conectar con ambas placas apagadas:

| Raspberry, conector de 40 pines | ESP32 del ejemplo |
|---|---|
| GPIO2 / SDA, pin fisico 3 | GPIO21 / SDA |
| GPIO3 / SCL, pin fisico 5 | GPIO22 / SCL |
| GND, por ejemplo pin fisico 6 | GND |

Senales de 3.3 V; no conectar SDA/SCL a 5 V. Alimentar cada placa por su USB
adecuado y compartir GND; no unir las salidas de alimentacion de ambas placas.
I2C necesita pull-ups a 3.3 V; revisar las ya presentes en el bus antes de agregar
resistencias. Usar cables cortos. Otras variantes ESP32 pueden necesitar otros
pines y soporte de esclavo diferente; este sketch supone ESP32 clasico.

## ESP32

Abrir esp32_receptor.ino en Arduino IDE con el paquete Arduino-ESP32 instalado.
Seleccionar la placa y puerto correctos, cargar y abrir Monitor Serial a 115200.
Debe indicar receptor listo en 0x08. No conecta ni acciona motores.
La callback encola; loop imprime. Mensajes de longitud distinta de uno o cola
llena se descartan con aviso. El maestro no recibe una confirmacion de aplicacion.

## Raspberry

Habilitar I2C con `sudo raspi-config`, Interface Options, I2C. Reiniciar si se indica.
Comprobar que existe `/dev/i2c-1`; si usa otro adaptador, identificarlo con
`i2cdetect -l` (requiere i2c-tools). Este comando lista buses, no escanea dispositivos.

Desde ~/Proyecto-Carro/raspberry-pi/demoI2C:

```bash
g++ -std=c++17 -Wall -Wextra -O2 demoI2C.cpp -o demoI2C
./demoI2C
```

No requiere OpenCV, Flask ni cpp-httplib. Por defecto abre /dev/i2c-1; para otro:
`./demoI2C /dev/i2c-N` reemplazando N por el adaptador identificado.
Si falta permiso, revisar `ls -l /dev/i2c-1` y `groups`. En Raspberry Pi OS, si
el dispositivo pertenece al grupo i2c, agregar el usuario con
`sudo usermod -aG i2c "$USER"` y cerrar/reabrir la sesion SSH.

| Opcion del menu | Byte | Interpretacion |
|---|---|---|
| 0 | 0x00 | Inicio |
| 1 | 0x01 | Frenar |
| 2 | 0x02 | Retroceder |
| 3 | 0x03 | Cambiar velocidad |
| 4 | 0x04 | Bifurcacion derecha |
| 5 | 0x05 | Bifurcacion izquierda |
| 6 | 0xFF | Fin |
| q | Ninguno | Salir del programa |

Se escribe el numero de opcion y Enter. Salir o EOF no envia Fin automaticamente.
Entradas vacias, letras distintas de q, numeros fuera del menu o tokens extra
no transmiten. Cada seleccion valida envia exactamente un byte, sin reintento.
Abrir bus y seleccionar direccion no confirma que la ESP32 este conectada.
El exito de write confirma la transferencia del bus, no la ejecucion de una accion.
La transferencia es sincrona; ante fallos el tiempo de retorno depende del driver.

## Verificacion

Enviar las opciones 0 a 6, una por una, y cotejar byte y nombre en ambas terminales.
En particular, 6 debe dar 0xFF. Repetir 4 dos veces: deben llegar dos mensajes.
Probar entrada invalida: no debe aparecer nada en el monitor serial.
Salir con q: no debe enviarse un codigo adicional. Para probar desconexion,
apagar antes de cambiar cableado; una ESP ausente debe producir error de envio.
No ejecutar simultaneamente otro programa que envie ordenes por este bus.

Referencias:
- https://www.kernel.org/doc/html/latest/i2c/dev-interface.html
- https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2c.html
