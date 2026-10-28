# -*- coding: utf-8 -*-
"""
Interfaz de control del carro seguidor de pista.
MT-7003 Microprocesadores y Microcontroladores - II Semestre 2026

Corre en la Raspberry Pi. Sirve la pagina de control y traduce cada accion
del usuario en un codigo de un byte que se envia al ESP32 por el bus I2C.
El bus I2C es el unico medio de comunicacion con el microcontrolador.

Uso:
    pip3 install flask smbus2
    python3 servidor.py
    Abrir http://<ip-de-la-raspberry>:8000 desde el celular o la laptop.
"""

import os
from threading import Lock

from flask import Flask, jsonify, request, send_from_directory

# ---------------------------------------------------------------------------
# Configuracion del bus I2C
# ---------------------------------------------------------------------------

DIRECCION_ESP32 = 0x08   # direccion del ESP32 como esclavo I2C
NUMERO_DE_BUS = 1        # bus 1 en Raspberry Pi 2/3/4 (GPIO2 = SDA, GPIO3 = SCL)
PUERTO_WEB = 8000

# Codigos de un byte enviados al ESP32.
# Las tres acciones tienen codigo propio en lugar de un codigo por color:
# asi la asignacion color-accion se cambia desde la interfaz sin tocar
# el firmware del microcontrolador.
CODIGO_INICIO = 0x00
CODIGO_FRENAR = 0x01
CODIGO_RETROCEDER = 0x02
CODIGO_VELOCIDAD = 0x03
CODIGO_BIF_DERECHA = 0x04
CODIGO_BIF_IZQUIERDA = 0x05
CODIGO_FIN = 0xFF

CODIGO_DE_ACCION = {
    "frenar": CODIGO_FRENAR,
    "retroceder": CODIGO_RETROCEDER,
    "velocidad": CODIGO_VELOCIDAD,
}

CODIGO_DE_RAMAL = {
    "derecha": CODIGO_BIF_DERECHA,
    "izquierda": CODIGO_BIF_IZQUIERDA,
}

COLORES = ("rojo", "verde", "azul")
ACCIONES = ("frenar", "retroceder", "velocidad")

# ---------------------------------------------------------------------------
# Acceso al bus I2C
# ---------------------------------------------------------------------------

try:
    from smbus2 import SMBus
    _bus = SMBus(NUMERO_DE_BUS)
    print("[OK] Bus I2C %d abierto." % NUMERO_DE_BUS)
except Exception as error:
    _bus = None
    print("[AVISO] No se pudo abrir el bus I2C (%s)." % error)
    print("[AVISO] El servidor corre en modo simulacion: los codigos solo se imprimen.")

_candado_i2c = Lock()


def enviar_codigo(codigo):
    """Escribe un byte en el ESP32. Unica salida hacia el microcontrolador."""
    with _candado_i2c:
        if _bus is None:
            print("[SIM] I2C -> 0x%02X" % codigo)
            return True
        try:
            _bus.write_byte(DIRECCION_ESP32, codigo)
            print("[I2C] -> 0x%02X" % codigo)
            return True
        except OSError as error:
            print("[ERROR] Fallo al escribir en I2C: %s" % error)
            return False


# ---------------------------------------------------------------------------
# Estado del sistema
# ---------------------------------------------------------------------------

estado = {
    "en_movimiento": False,
    "conteo": {"rojo": 0, "verde": 0, "azul": 0},
    "asignacion": {"rojo": "frenar", "verde": "retroceder", "azul": "velocidad"},
    "bifurcacion": "derecha",
}

_candado_estado = Lock()


# ---------------------------------------------------------------------------
# Funciones que debe llamar el codigo de vision por computadora
# ---------------------------------------------------------------------------

def registrar_color(color):
    """
    Llamar cuando la camara confirma un segmento de color.
    Incrementa el contador y envia al ESP32 el codigo de la accion que el
    usuario asigno a ese color en la interfaz.
    """
    if color not in COLORES:
        return False

    with _candado_estado:
        if not estado["en_movimiento"]:
            return False
        estado["conteo"][color] += 1
        accion = estado["asignacion"][color]

    enviar_codigo(CODIGO_DE_ACCION[accion])
    return True


def registrar_bifurcacion():
    """
    Llamar cuando la camara identifica la figura que marca la bifurcacion.
    El ramal que se toma lo define la interfaz, no la figura.
    """
    with _candado_estado:
        if not estado["en_movimiento"]:
            return False
        ramal = estado["bifurcacion"]

    enviar_codigo(CODIGO_DE_RAMAL[ramal])
    return True


# ---------------------------------------------------------------------------
# Servidor web
# ---------------------------------------------------------------------------

app = Flask(__name__, static_folder=None)
CARPETA = os.path.dirname(os.path.abspath(__file__))


@app.route("/")
def pagina():
    return send_from_directory(CARPETA, "index.html")


@app.route("/api/estado")
def api_estado():
    with _candado_estado:
        return jsonify(estado)


@app.route("/api/inicio", methods=["POST"])
def api_inicio():
    with _candado_estado:
        estado["en_movimiento"] = True
        estado["conteo"] = {"rojo": 0, "verde": 0, "azul": 0}
    enviar_codigo(CODIGO_INICIO)
    return jsonify({"ok": True})


@app.route("/api/fin", methods=["POST"])
def api_fin():
    with _candado_estado:
        estado["en_movimiento"] = False
    enviar_codigo(CODIGO_FIN)
    return jsonify({"ok": True})


@app.route("/api/config", methods=["POST"])
def api_config():
    """
    Cambia la asignacion color-accion y el ramal de la bifurcacion.
    Solo se permite con el carro detenido; de lo contrario responde con
    un error que la pagina despliega en pantalla.
    """
    with _candado_estado:
        if estado["en_movimiento"]:
            return jsonify({
                "ok": False,
                "error": "No se puede cambiar la configuracion con el carro en movimiento. "
                         "Presione Fin para detenerlo."
            }), 409

        datos = request.get_json(silent=True) or {}
        asignacion = datos.get("asignacion")
        bifurcacion = datos.get("bifurcacion")

        if asignacion is not None:
            if sorted(asignacion.keys()) != sorted(COLORES):
                return jsonify({"ok": False, "error": "Faltan colores en la configuracion."}), 400
            if sorted(asignacion.values()) != sorted(ACCIONES):
                return jsonify({
                    "ok": False,
                    "error": "Cada accion debe asignarse a un unico color."
                }), 400
            estado["asignacion"] = dict(asignacion)

        if bifurcacion is not None:
            if bifurcacion not in CODIGO_DE_RAMAL:
                return jsonify({"ok": False, "error": "Ramal invalido."}), 400
            estado["bifurcacion"] = bifurcacion

        copia = dict(estado)

    return jsonify({"ok": True, "estado": copia})


# --- Endpoints de prueba -----------------------------------------------------
# Permiten verificar la interfaz antes de que la camara este lista.
# Pueden eliminarse para la entrega final.

@app.route("/api/simular/color/<color>", methods=["POST"])
def api_simular_color(color):
    if not registrar_color(color):
        return jsonify({"ok": False, "error": "El carro debe estar en movimiento."}), 409
    return jsonify({"ok": True})


@app.route("/api/simular/bifurcacion", methods=["POST"])
def api_simular_bifurcacion():
    if not registrar_bifurcacion():
        return jsonify({"ok": False, "error": "El carro debe estar en movimiento."}), 409
    return jsonify({"ok": True})


if __name__ == "__main__":
    print("Interfaz disponible en el puerto %d" % PUERTO_WEB)
    app.run(host="0.0.0.0", port=PUERTO_WEB, threaded=True)
