# Carro con seguimiento de pista

Proyecto final del curso **MT-7003 Microprocesadores y Microcontroladores**
Instituto Tecnológico de Costa Rica — Escuela de Ingeniería Mecatrónica
II Semestre 2026 · Prof. Dr.-Ing. Carlos Adrián Salazar García

## Equipo

- Sergio Andrés García Acuña
- Pablo Jacquinod González
- Fernando Josué Rodríguez Camacho
- Alejandro Zamora Llach

## Descripción

Vehículo autónomo que sigue una pista marcada con tape negro, identifica segmentos
de color rojo, verde y azul para ejecutar acciones configurables, cuenta los
segmentos encontrados y elige el ramal en una bifurcación en forma de Y.

El sistema se reparte entre dos unidades de cómputo:

| Unidad | Responsabilidad |
|---|---|
| Raspberry Pi 3 | Interfaz web, visión por computadora, conteo de segmentos |
| ESP32-WROOM-32 | Lazo de control PID, lectura de sensores, manejo de motores |

La comunicación entre ambas es **únicamente por I2C**, con la Raspberry Pi como
maestro y el ESP32 como esclavo en la dirección `0x08`.

## Protocolo I2C

Códigos de un byte enviados desde la Raspberry Pi hacia el ESP32:

| Código | Significado |
|---|---|
| `0x00` | Inicio del movimiento |
| `0x01` | Frenar |
| `0x02` | Retroceder |
| `0x03` | Cambio de velocidad |
| `0x04` | Bifurcación: tomar el ramal derecho |
| `0x05` | Bifurcación: tomar el ramal izquierdo |
| `0xFF` | Fin del movimiento |

Los códigos `0x01` a `0x03` corresponden a **acciones**, no a colores. La
asociación entre cada color y cada acción se define desde la interfaz, y la
Raspberry Pi hace la traducción. Así, cambiar la asignación no requiere
reprogramar el microcontrolador.

## Estructura del repositorio

```
documentos/
  enunciado/        Enunciado y rúbrica del curso
  avances/          Avances I a IV entregados
  informe-final/    Informe final en formato IEEE
raspberry-pi/
  servidor.py       Servidor web e interfaz de control
  index.html        Página que se abre desde el celular
  vision/           Código de identificación de colores y figuras
esp32/              Firmware del microcontrolador (ESP-IDF, lenguaje C)
bitacora/           Bitácora del grupo (requisito de la rúbrica)
recursos/
  diagramas/        Diagramas de flujo y de bloques
  fotos/            Fotografías del prototipo y de las pruebas
```

## Cómo correr la interfaz

En la Raspberry Pi, con el I2C ya habilitado:

```bash
cd raspberry-pi
pip3 install flask smbus2
python3 servidor.py
```

Luego se abre `http://<ip-de-la-raspberry>:8000` desde el celular o la laptop
conectados a la misma red. Para conocer la dirección:

```bash
hostname -I
```

Si el bus I2C no está disponible, el servidor arranca igual en modo simulación e
imprime los códigos en la terminal, lo que permite probar la interfaz sin el
hardware conectado.

## Entregas

| Entrega | Semana | Estado |
|---|---|---|
| Avance I — Propuesta de algoritmos | 4 | Entregado |
| Avance II — Selección del microcontrolador | 6 | Entregado |
| Avance III — Diagramas y conteo de componentes | 10 | Entregado |
| Avance IV — Prototipo y algoritmos en operación | 14 | Pendiente |
| Entrega final e informe | 18 | Pendiente |

## Convenciones de trabajo

- Los documentos de Word los edita **una sola persona a la vez**. Git no sabe
  combinar archivos `.docx`, y si dos personas modifican el mismo documento se
  pierde el trabajo de una de las dos. Avisen en el grupo antes de abrir uno.
- Los mensajes de los commits describen qué cambió, en español y en presente:
  `Agrega conteo de segmentos a la interfaz`, no `cambios` ni `asdf`.
- El código se trabaja en ramas y se integra con pull request. Los documentos se
  pueden subir directo a `main`.
- Antes de empezar a trabajar, siempre `git pull`.
