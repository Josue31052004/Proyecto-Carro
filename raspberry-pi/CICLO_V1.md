# Ciclo principal V1 (pendiente de validar en Raspberry)

`main.cpp` mantiene todo el estado y ejecuta captura, deteccion, filtro de eventos
consecutivos y salida por terminal. Python solo sirve `index.html` y reenvia
solicitudes al puerto local 8081. No usa smbus2 ni I2C.

## Compilar y ejecutar

En la Raspberry, desde la carpeta `raspberry-pi` del repositorio:

```bash
g++ -std=c++17 -Wall -Wextra -pthread main.cpp -o ciclo_main $(pkg-config --cflags --libs opencv4 cpp-httplib) -lssl -lcrypto
./ciclo_main
```

Necesita las mismas dependencias C++ de la demo funcional. En otra terminal,
en la misma carpeta, con Flask instalado:

```bash
python3 servidor.py
```

Si falta Flask, instalar el paquete del sistema con `sudo apt install python3-flask`.
Abrir http://192.168.86.28:8000 desde la laptop en la misma red, o reenviar el
puerto 8000 en VS Code y abrir http://localhost:8000. La IP puede cambiar.
El programa C++ no abre un navegador ni lanza Python automaticamente.

## Flujo

1. C++ inicia su API local y espera una solicitud de Inicio.
2. Inicio abre la camara. Si abre correctamente, imprime 0x00 y entra al ciclo.
3. Cada frame usa la misma region, rangos HSV y umbral de la demo.
4. LogicaBloqueo acepta un color nuevo; el ciclo consulta su accion, cuenta una
   deteccion y llama EnviarCodigo. Esta funcion solo imprime en V1.
5. Fin sale del ciclo, imprime 0xFF, libera la camara y vuelve a esperar Inicio.
   Ctrl+C termina C++; detener tambien Python con Ctrl+C en su terminal.

## Verificacion manual

- Sin Inicio: no debe abrir la camara ni generar eventos de color.
- Cambiar rojo a retroceder: el selector intercambia la accion del otro color.
- Pulsar Inicio: debe aparecer Ciclo activo y 0x00 en terminal.
- Presentar rojo: debe imprimir ROJO y codigo de accion 0x02; contar una vez.
- Mantener rojo: no debe repetir. Retirarlo hasta NINGUNO y presentarlo de nuevo:
  debe contar otra vez. Una observacion ambigua no rearma el filtro.
- Durante el ciclo: configuracion bloqueada. Fin debe volver a estado detenido.
- Volver a Inicio: contadores a cero y camara abierta nuevamente.
- Si falla la camara: mostrar error y regresar a espera; no mantener ciclo activo.
- Si C++ no esta disponible: el puente devuelve 503, no simula exito.

## Limites de esta entrega

No implementa aun el bloqueo temporal de seis segundos ni confirmacion por
varios frames. Una sola imagen NINGUNO rearma el filtro: calibrar en V2.
No transmite I2C, no detecta bifurcaciones y no muestra video. El ramal se guarda
como configuracion para etapas posteriores. Los cambios se pierden al reiniciar
C++. El estado activo indica el ciclo de software, no confirma movimiento fisico.
La apertura y lectura de camara pueden bloquearse en el controlador, como en la
demo original; Fin se procesa al retornar esas operaciones. Resolver ese limite
antes de usar esta arquitectura como parada de hardware.
