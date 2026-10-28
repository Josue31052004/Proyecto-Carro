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
4. Mantener el color: un solo conteo. Retirar hasta NINGUNO y volver: otro evento.
5. Fin: desaparece la imagen, configuracion habilitada. Elegir derecha y volver a
   iniciar: amarillo produce 0x04. Contadores reiniciados.
6. Detener C++: mostrar desconexion y ocultar imagen; reiniciar y verificar retorno.
7. Una segunda instancia debe informar que el puerto 8000 esta ocupado.

## Limites

Salida solo por terminal: I2C pendiente. Bloqueo temporal de seis segundos y
confirmacion por varios frames pendientes. La configuracion no persiste al salir.
El estado activo indica el ciclo de software, no movimiento fisico confirmado.
La apertura/lectura de camara puede bloquearse en el controlador como en la demo;
Fin se procesa cuando retorna. No se modifico ese comportamiento en esta entrega.
