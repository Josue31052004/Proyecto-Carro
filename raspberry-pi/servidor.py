# -*- coding: utf-8 -*-
"""Puente web V1. C++ controla configuracion, vision, conteo y salida.
Ejecutar ./ciclo_main y luego python3 servidor.py. Requiere Flask.
No accede a I2C ni mantiene un estado independiente del carrito.
"""
import os
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

from flask import Flask, Response, jsonify, request, send_from_directory

app = Flask(__name__, static_folder=None)
CARPETA = os.path.dirname(os.path.abspath(__file__))
API_CPP = "http://127.0.0.1:8081"


def reenviar(ruta, datos=None):
    contenido = urlencode(datos).encode("utf-8") if datos is not None else None
    consulta = Request(API_CPP + ruta, data=contenido, method=request.method)
    if contenido is not None:
        consulta.add_header("Content-Type", "application/x-www-form-urlencoded")
    try:
        with urlopen(consulta, timeout=3) as respuesta:
            return Response(respuesta.read(), status=respuesta.status,
                            content_type="application/json")
    except HTTPError as error:
        return Response(error.read(), status=error.code, content_type="application/json")
    except (URLError, TimeoutError, OSError):
        return jsonify(ok=False, error="Sin conexion con ciclo_main de C++"), 503


@app.get("/")
def pagina():
    return send_from_directory(CARPETA, "index.html")


@app.get("/api/estado")
def api_estado():
    return reenviar("/api/estado")


@app.post("/api/inicio")
def api_inicio():
    return reenviar("/api/inicio", {})


@app.post("/api/fin")
def api_fin():
    return reenviar("/api/fin", {})


@app.post("/api/config")
def api_config():
    # Solo valida la forma del mensaje; C++ valida y aplica su contenido.
    datos = request.get_json(silent=True)
    if not isinstance(datos, dict) or not isinstance(datos.get("asignacion"), dict):
        return jsonify(ok=False, error="Configuracion mal formada"), 400
    asignacion = datos["asignacion"]
    if set(asignacion) != {"rojo", "verde", "azul"} or not all(
        isinstance(valor, str) for valor in asignacion.values()
    ) or not isinstance(datos.get("bifurcacion"), str):
        return jsonify(ok=False, error="Configuracion incompleta"), 400
    return reenviar("/api/config", {**asignacion, "bifurcacion": datos["bifurcacion"]})


if __name__ == "__main__":
    print("Interfaz: http://<ip-raspberry>:8000; requiere ciclo_main en puerto 8081")
    app.run(host="0.0.0.0", port=8000, threaded=True)
