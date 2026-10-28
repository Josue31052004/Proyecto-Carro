#pragma once
// Vista remota: HTTP local, accesible desde la laptop por el tunel SSH.
// Dependencia en Raspberry Pi OS: libcpp-httplib-dev.
#include <httplib.h>
#include <opencv2/opencv.hpp>
#include <atomic>
#include <chrono>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

class VistaDemo {
    httplib::Server servidor;
    std::thread hilo;
    std::mutex mutexImagen;
    std::string jpeg;
    std::chrono::steady_clock::time_point ultimaImagen{};
    std::atomic<bool> finalizado{false};

public:
    VistaDemo() {
        servidor.new_task_queue = [] { return new httplib::ThreadPool(2); };
        servidor.set_read_timeout(2, 0);
        servidor.set_write_timeout(2, 0);
        servidor.Get("/", [](const httplib::Request&, httplib::Response& respuesta) {
            respuesta.set_content(R"HTML(<!doctype html>
<html lang="es"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Demo de colores</title>
<style>body{font:18px system-ui;background:#18212c;color:#f3f6fa;margin:30px auto;max-width:800px;padding:0 16px}img{width:100%;max-width:640px;border:2px solid #738399}p{line-height:1.5}#estado{color:#ffd76b}</style>
<h1>Deteccion de colores</h1>
<p>Camara y procesamiento en la Raspberry Pi. El rectangulo amarillo indica la zona analizada.</p>
<p id="estado" role="status">Conectando...</p>
<img id="camara" alt="Imagen de la camara con color y codigo detectados" hidden>
<p>Rojo: 0x01 · Verde: 0x02 · Azul: 0x03<br>Ninguno: 0x00 · Indeterminado: 0xFF</p>
<script>
const foto=document.getElementById('camara'), estado=document.getElementById('estado');
let anterior;
async function actualizar(){
  const control=new AbortController(), limite=setTimeout(()=>control.abort(),3000);
  try {
    const r=await fetch('/frame.jpg?t='+Date.now(),{cache:'no-store',signal:control.signal});
    if(!r.ok) throw new Error('Sin imagen reciente');
    const siguiente=URL.createObjectURL(await r.blob());
    foto.src=siguiente; foto.hidden=false;
    if(anterior) URL.revokeObjectURL(anterior);
    anterior=siguiente;
    estado.textContent='Recibiendo imagenes · '+new Date().toLocaleTimeString();
  } catch(e) {
    foto.hidden=true;
    estado.textContent='Sin imagen reciente. Revisar demo, camara y tunel SSH.';
  } finally {clearTimeout(limite);setTimeout(actualizar,150);}
}
actualizar();
</script></html>)HTML", "text/html; charset=utf-8");
        });
        servidor.Get("/frame.jpg", [this](const httplib::Request&, httplib::Response& respuesta) {
            std::lock_guard<std::mutex> acceso(mutexImagen);
            respuesta.set_header("Cache-Control", "no-store");
            if (jpeg.empty() || std::chrono::steady_clock::now() - ultimaImagen > std::chrono::seconds(2)) {
                respuesta.status = 503;
                respuesta.set_content("Sin imagen reciente", "text/plain");
                return;
            }
            respuesta.set_content(jpeg, "image/jpeg");
        });
        if (!servidor.bind_to_port("127.0.0.1", 8080))
            throw std::runtime_error("No se pudo abrir puerto 8080. Revisar si otra demo esta ejecutandose.");
        hilo = std::thread([this] { servidor.listen_after_bind(); finalizado = true; });
        while (!servidor.is_running() && !finalizado)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (finalizado) {
            hilo.join();
            throw std::runtime_error("El servidor web no pudo iniciar.");
        }
    }

    void publicar(const cv::Mat& imagen) {
        if (finalizado) throw std::runtime_error("El servidor web se detuvo.");
        std::vector<uchar> bytes;
        if (!cv::imencode(".jpg", imagen, bytes, {cv::IMWRITE_JPEG_QUALITY, 75}))
            throw std::runtime_error("No se pudo preparar la imagen para el navegador.");
        std::lock_guard<std::mutex> acceso(mutexImagen);
        jpeg.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        ultimaImagen = std::chrono::steady_clock::now();
    }

    ~VistaDemo() {
        servidor.stop();
        if (hilo.joinable()) hilo.join();
    }
};
