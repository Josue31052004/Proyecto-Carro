# Ciclo principal con interfaz y camara en C++

Se ejecuta un solo programa: ciclo_main. main.cpp sirve directamente la pagina
incluida en vista_control.hpp, la API y los JPEG en el puerto 8000.
No ejecutar servidor.py. Los archivos anteriores se conservan en legacy/.
La pagina queda incorporada en el ejecutable; recompilar si cambia el .hpp.

## Actualizar y ejecutar en Raspberry

Detener con Ctrl+C el ciclo anterior Y el servidor Python (liberar puerto 8000).
Tras recibir los cambios de GitHub, desde ~/Proyecto-Carro/raspberry-pi:

```bash
g++ -std=c++17 -Wall -Wextra -pthread main.cpp -o ciclo_main $(pkg-config --cflags --libs opencv4 cpp-httplib) -lssl -lcrypto
./ciclo_main
```

Abrir http://192.168.86.28:8000 y recargar con Ctrl+F5. Usar la IP actual si cambia.
Tambien se puede reenviar el puerto 8000 por Remote SSH y abrir localhost:8000.
No requiere Flask, Python ni puerto 8081. No cambia las dependencias C++ de la demo.

## Flujo

main inicia HTTP en un hilo auxiliar y espera Inicio. El hilo principal abre
la camara, imprime 0x00 y ejecuta CicloEjecucion. El detector conserva los rangos
HSV, region central y minimo de 8%. LogicaBloqueo evita repetir colores
consecutivos y EnviarCodigo imprime la accion. Fin imprime 0xFF, libera la camara
y vuelve a esperar. Ctrl+C termina el programa completo.

La interfaz permite intercambiar las acciones RGB y elegir el ramal amarillo.
La configuracion solo cambia cuando el ciclo esta detenido. Las solicitudes
usan formularios HTTP; C++ las valida y mantiene el estado autoritativo.

La misma captura se dibuja y codifica hasta 10 veces por segundo despues de
la deteccion. JPEG y estado se comparten con mutex. La codificacion ocurre fuera
del mutex. La camara no se abre dos veces ni desde el hilo HTTP.
La imagen muestra color y porcentajes RGB/amarillo de la region central.
C++ rechaza imagenes de mas de dos segundos; el navegador oculta imagenes ante
fallos o timeout de tres segundos. El enlace antiguo /camara redirige a la vista
integrada. Antes de Inicio y despues de Fin no se muestra imagen.

## Codigos

- Inicio: 0x00. Fin: 0xFF.
- Frenar: 0x01. Retroceder: 0x02. Velocidad: 0x03.
- Amarillo: 0x04 si el ramal es derecha, 0x05 si es izquierda.
- NINGUNO=-1 e INDETERMINADO=-2 son internos, nunca ordenes.

Amarillo usa H=20..34, S>=80 y V>=50: calibrar con la pista.
Dos colores con al menos 8% producen INDETERMINADO, sin nueva orden.

## Pruebas en Raspberry

1. Arrancar solo ciclo_main. Control y camara deben cargar juntos.
2. Sin Inicio: sin imagen ni eventos. Cambiar rojo a retroceder y ramal a izquierda.
3. Inicio: imagen, region y porcentajes; rojo debe producir 0x02, amarillo 0x05.
4. Mantener el color: un solo conteo. Tras cinco segundos, retirar hasta NINGUNO y volver: otro evento.
5. Fin: desaparece la imagen, configuracion habilitada. Elegir derecha y volver a
   iniciar: amarillo produce 0x04. Contadores reiniciados.
6. Detener C++: mostrar desconexion y ocultar imagen; reiniciar y verificar retorno.
7. Una segunda instancia debe informar que el puerto 8000 esta ocupado.

## Limites

Salida real I2C habilitada. La confirmacion por varios frames queda pendiente. La configuracion no persiste al salir.
El estado activo indica el ciclo de software, no movimiento fisico confirmado.
La apertura/lectura de camara puede bloquearse en el controlador como en la demo;
Fin se procesa cuando retorna. No se modifico ese comportamiento en esta entrega.

## Bloqueo de envio

`tiempopausa{5}` define cinco segundos. El primer color valido genera una orden
inmediatamente. Luego deben cumplirse ambas condiciones en una observacion:
plazo vencido y color distinto del disparador. Captura, deteccion y vista continuan.
El contador solo aumenta cuando se acepta un evento, no por cada frame bloqueado.

- Rojo en t=0: orden. Verde en t=2: bloqueado. Verde en t=5 o despues: orden.
- Rojo sigue presente despues de t=5: sigue bloqueado.
- NINGUNO despues del plazo: rearma sin enviar; rojo posterior puede generar orden.
- NINGUNO antes del plazo seguido de rojo al vencer: sigue bloqueado.
- INDETERMINADO no confirma cambio y no rearma.
- Inicio y Fin no estan sujetos al bloqueo de colores. Reiniciar el ciclo reinicia
  este filtro. Un cambio ocurrido solo durante el plazo no se guarda como evento.

La terminal dice `[I2C] Codigo enviado 0xXX, esperando nueva deteccion.`
El mensaje se imprime solo tras una escritura I2C correcta.

## Integracion I2C real

main abre /dev/i2c-1 una sola vez y selecciona 0x08, como en demoI2C. Si falla
la apertura/configuracion, termina con error. El descriptor se cierra al salir.
EnviarCodigo escribe exactamente un byte. Inicio=0x00, Fin=0xFF; RGB usa la
asignacion de la interfaz y amarillo usa derecha=0x04/izquierda=0x05.
No ejecutar demoI2C simultaneamente con ciclo_main.

No se necesitan nuevas librerias de enlace: usar el mismo comando de compilacion.
Mantener el receptor ESP32 y cableado ya probados. Verificar en su monitor serial:
Inicio, cada color con separacion de cinco segundos y cambio de color, y Fin.
Probar ambas direcciones para amarillo y cambiar una asignacion RGB con el ciclo
detenido. Ctrl+C durante el ciclo intenta transmitir Fin antes de cerrar el bus.

Si falla Inicio no arranca la deteccion; se intenta Fin como cierre del ciclo.
Si falla una orden de color, conserva contador y bloqueo, no reintenta, y sigue
capturando. El contador representa eventos, no ordenes recibidas. La interfaz
muestra error del ultimo envio y el ultimo codigo escrito correctamente. Una
transferencia posterior correcta limpia ese indicador. El fallo de Inicio queda
ademas en el error del ciclo hasta una nueva solicitud de Inicio.
Fin no confirma parada fisica: comprobar recepcion en la ESP32. write correcto
solo confirma transferencia del bus, no ejecucion del firmware.

El bloqueo temporal sigue usando reloj monotono sin sleep. write es sincrono y
el controlador determina cuanto tarda (especialmente con fallos); durante esa
llamada se retrasa brevemente el ciclo y el acceso al estado. No se garantiza un
plazo maximo de parada. Integracion pendiente de compilar y validar en Raspberry.

## Ajuste de rendimiento (29 octubre 2026)

640x480, cuadro centrado del 70% de ancho y alto: 448x336 (49% del area).
Conserva rangos HSV y umbral de ocupacion 8% de la region.
Solicita 60 FPS; si la apertura falla, intenta 30 y lo informa en terminal.
No garantiza 60 FPS ni determina el maximo del sensor: validar en hardware.
Si abre pero falla la lectura, se detiene el ciclo como antes, sin fallback.
La terminal informa FPS realmente procesados cada cinco segundos; esta medida
incluye el efecto de procesamiento, vista e I2C, no mide todos los frames del sensor.
La vista se codifica como maximo cada 333 ms (~3/s), para reducir carga JPEG.
Se evita contar dos veces los mismos pixeles en cada mascara. Appsink mantiene
solo el frame mas reciente, sin acumular imagenes atrasadas.
La demo de calibracion permanece independiente, sin cambiar sus valores iniciales.

## Bloqueo vigente: sin tiempo (sustituye la regla anterior de cinco segundos)

Se compara el codigo de accion con el ultimo evento aceptado. NINGUNO no borra
ese recuerdo: rojo -> ninguno -> rojo genera solo una orden. Rojo -> verde ->
rojo genera tres, sin espera. Amarillo es la excepcion: amarillo -> ninguno ->
amarillo genera dos bifurcaciones, pero mantener amarillo solo genera una.
INDETERMINADO no rearma amarillo ni borra el ultimo codigo. Una bifurcacion
aceptada tambien actualiza el ultimo codigo: rojo -> amarillo -> rojo se permite.
Inicio/Fin no pasan por este filtro. Cada nuevo ciclo reinicia la memoria.
Se registra el evento antes del envio: un fallo I2C conserva el bloqueo para no
reintentar automaticamente una orden cuya recepcion no se conoce.
