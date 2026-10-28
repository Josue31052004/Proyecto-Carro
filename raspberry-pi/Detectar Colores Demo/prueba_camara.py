
from picamera2 import Picamera2
import cv2
import time

print("Iniciando Arducam IMX219...")

camera = Picamera2()

config = camera.create_preview_configuration(
    main={"size": (640, 480), "format": "RGB888"}
)
camera.configure(config)

try:
    camera.start()
    time.sleep(2)

    # Capturar imagen desde la Raspberry Pi 5
    frame = camera.capture_array()

    print("Imagen capturada:", frame.shape)

    # Picamera2 entrega el buffer RGB888 en orden BGR
    # compatible con OpenCV para este formato.
    cv2.imwrite("foto_original.jpg", frame)

    # Procesar imagen con OpenCV
    gris = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    cv2.imwrite("foto_grises.jpg", gris)

    print("¡Prueba completada!")
    print("Archivos: foto_original.jpg y foto_grises.jpg")

finally:
    camera.stop()
    camera.close()
