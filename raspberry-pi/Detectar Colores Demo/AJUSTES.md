# Demo ajustable de cuatro colores (sin I2C)

Detener ciclo_main y cualquier otra captura antes de iniciar esta demo.

```bash
cd ~/Proyecto-Carro/raspberry-pi/"Detectar Colores Demo"
g++ -std=c++17 -Wall -Wextra -pthread calibrar_colores.cpp -o calibrar_colores $(pkg-config --cflags --libs opencv4 cpp-httplib) -lssl -lcrypto
./calibrar_colores --web
```

Abrir http://192.168.86.28:8080 con la IP actual, o reenviar 8080 en VS Code.
No usa Python. La interfaz viene incluida en vista_calibracion.hpp.

- Resolucion solicitada: 320x240, 640x480 (inicial), 1280x720. La camara debe
  soportar la combinacion con 30 FPS solicitados. Cambiarla libera y reabre captura.
- Ocupacion minima: 1..100% de la REGION analizada, inicial 8%.
- Tamano del cuadro: 10..100% del ancho Y alto, inicial 50%. A 50% ocupa el 25%
  del area total. Siempre centrado; no modifica posicion ni relacion de aspecto.

Pulsar Aplicar. Los dos porcentajes cambian al siguiente frame; no reinician
la camara. La imagen muestra resolucion recibida y porcentajes aplicados.
Si no abre, elegir otra resolucion y Aplicar (tambien permite reintentar la misma).
Consultar terminal si sigue sin imagen. La lectura/apertura puede esperar al
controlador; la peticion HTTP confirma recepcion, no apertura exitosa.
Los ajustes no se guardan al cerrar. Los modos --ventana y --sin-ventana conservan
los valores iniciales, sin controles web.

Amarillo inicia en H=20..34, S>=80 y V>=50. Todos los rangos HSV son ajustables.
Los FPS siguen fijos. No se han agregado mascaras en esta entrega.
Captura solicitada: 30 FPS. JPEG hasta 10/s; navegador espera 150 ms entre consultas.
Flags: rojo 01, verde 02, azul 03, amarillo 04, ninguno 00, ambiguo FF.
Son resultados internos de esta demo; no se envian al carrito.

Prueba: verificar los cuatro colores con valores iniciales; cambiar cuadro a
25% y luego 100%, comprobar que sigue centrado; variar minimo 1%, 8%, 50% y
observar el cambio de sensibilidad. Probar cada resolucion, incluyendo regreso
a 640x480. Dos colores suficientes deben producir INDETERMINADO. Blanco y negro
no deben disparar colores. Para comparar cambiar un solo parametro cada vez.

Pendiente de compilar y probar en Raspberry con la camara real.

La demo original detectar_color.cpp + vista_demo.hpp fue restaurada sin cambios.
La nueva demo es calibrar_colores.cpp + vista_calibracion.hpp. Ejecutar solo una
captura a la vez; ambas usan puerto 8080 y la misma camara.

## Ajustes de color

Cada rango tiene H minimo/maximo (0..179), S minimo/maximo y V minimo/maximo
(0..255). Se puede ajustar verde, azul, amarillo y los dos rangos de rojo.
El rojo resulta de unir sus dos mascaras. Los limites son inclusivos; un minimo
mayor al maximo se rechaza sin modificar la configuracion. H usa la escala de
OpenCV, no grados 0..360. Los cambios HSV se aplican al siguiente frame y no
reinician captura. Los valores no se guardan al salir.

Probar: variar H del amarillo con material real; comprobar que Aplicar cambia
la deteccion sin perder la captura. Enviar minimo mayor al maximo debe mostrar
error y conservar ajustes anteriores. Recargar pagina debe recuperar los ajustes
vigentes. Rangos de colores solapados pueden producir INDETERMINADO.

## Finalizar prueba

Aplicar los ajustes, comprobar el resultado en la imagen y pulsar
"Finalizar prueba: guardar e imprimir valores". El servidor solo acepta guardar
si tiene un frame reciente correspondiente a la configuracion vigente.

Se crea un informe nuevo calibraciones/calibracion_<identificador>.txt en la
carpeta desde la que se ejecuto el programa. La ruta absoluta aparece en la
interfaz y en la terminal, junto con resolucion solicitada/recibida, FPS
solicitados (no medidos), frecuencia web, tamano de region, porcentaje minimo y
los limites H/S/V de cada color (incluidos ambos rangos rojos).

Los informes permanecen al cerrar; no se cargan automaticamente al reiniciar.
Finalizar guarda el resultado, pero deja la vista abierta para consultarlo.
Ctrl+C termina el programa. Si se cambia un campo, Aplicar antes de guardar.
Un fallo de escritura muestra error, sin anunciar guardado exitoso.
