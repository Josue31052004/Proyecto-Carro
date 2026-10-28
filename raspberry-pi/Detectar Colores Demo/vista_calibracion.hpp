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
#include <sstream>
#include <array>
#include <fstream>
#include <iostream>
#include <filesystem>

struct RangoHSV {
    int hmin, hmax, smin, smax, vmin, vmax;
};

struct ConfigDemo {
    int ancho = 640, alto = 480;
    int porcentaje = 8; // Porcentaje de la REGION, no del frame entero.
    int cuadro = 50; // Porcentaje del ancho y alto; region centrada.
    // Rojo bajo, rojo alto, verde, azul, amarillo. H en escala OpenCV 0..179.
    std::array<RangoHSV, 5> hsv{{{0,10,80,255,50,255}, {170,179,80,255,50,255},
        {35,85,80,255,50,255}, {95,130,80,255,50,255}, {20,34,80,255,50,255}}};
};

class VistaDemo {
    httplib::Server servidor;
    std::thread hilo;
    std::mutex mutexImagen;
    std::string jpeg;
    std::chrono::steady_clock::time_point ultimaImagen{};
    std::atomic<bool> finalizado{false};
    ConfigDemo config;
    unsigned revision = 0;
    unsigned revisionImagen = 0;
    int anchoReal = 0, altoReal = 0;

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
<h1>Calibracion de colores</h1>
<form id="ajustes">
<p><label>Resolucion <select name="resolucion"><option>320x240</option><option selected>640x480</option><option>1280x720</option></select></label></p>
<p><label>Ocupacion minima del color (% de la region) <input name="porcentaje" type="number" min="1" max="100" value="8" required></label></p>
<p><label>Tamano del cuadro (% del ancho y alto) <input name="cuadro" type="number" min="10" max="100" value="50" required></label></p>
<p>H: tono (0..179), S: saturacion (0..255), V: brillo (0..255). Rojo combina dos rangos.</p>
<div id="rangos"></div>
<button id="aplicar">Aplicar ajustes</button><p id="mensaje" role="status"></p>
</form>
<button id="guardar" type="button" disabled>Finalizar prueba: guardar e imprimir valores</button>
<p>Primero aplica los ajustes y comprueba la imagen. Se guarda un informe en la Raspberry; la vista sigue abierta.</p>
<pre id="resumen" style="white-space:pre-wrap" role="status"></pre>
<p>Camara y procesamiento en la Raspberry Pi. El rectangulo amarillo indica la zona analizada.</p>
<p id="estado" role="status">Conectando...</p>
<img id="camara" alt="Imagen de la camara con color y codigo detectados" hidden>
<p>Rojo: 0x01 · Verde: 0x02 · Azul: 0x03 · Amarillo: 0x04<br>Ninguno: 0x00 · Indeterminado: 0xFF. Demo sin I2C.</p>
<script>
const foto=document.getElementById('camara'), estado=document.getElementById('estado');
let anterior;
const formulario=document.getElementById('ajustes'), mensaje=document.getElementById('mensaje'), boton=document.getElementById('aplicar');
boton.disabled=true;
const guardar=document.getElementById('guardar'), resumen=document.getElementById('resumen');
formulario.addEventListener('input',()=>{guardar.disabled=true;});
guardar.addEventListener('click',async()=>{
 guardar.disabled=true;
 try {
  const r=await fetch('/finalizar',{method:'POST'});
  const texto=await r.text();if(!r.ok)throw Error(texto);
  resumen.textContent=texto;
 }catch(e){resumen.textContent='No se guardo: '+e.message;}
 finally{guardar.disabled=false;}
});
const campos=['hmin','hmax','smin','smax','vmin','vmax'];
['Rojo bajo','Rojo alto','Verde','Azul','Amarillo'].forEach((color,i)=>{
 const grupo=document.createElement('fieldset');
 const titulo=document.createElement('legend');titulo.textContent=color;grupo.appendChild(titulo);
 campos.forEach(campo=>{
  const etiqueta=document.createElement('label');etiqueta.textContent=campo+' ';
  const entrada=document.createElement('input');entrada.type='number';entrada.required=true;
  entrada.min=0;entrada.max=campo.startsWith('h')?179:255;entrada.name='r'+i+'_'+campo;
  entrada.style.width='70px';etiqueta.appendChild(entrada);grupo.appendChild(etiqueta);
 });
 document.getElementById('rangos').appendChild(grupo);
});
fetch('/config',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error();return r.json();}).then(c=>{
  for(const nombre of ['resolucion','porcentaje','cuadro']) formulario.elements[nombre].value=c[nombre];
  c.hsv.forEach((rango,i)=>campos.forEach((campo,j)=>{formulario.elements['r'+i+'_'+campo].value=rango[j];}));
  boton.disabled=false;
  guardar.disabled=false;
}).catch(()=>{mensaje.textContent='No se pudo cargar configuracion. Recarga la pagina.';});
formulario.addEventListener('submit',async e=>{
 e.preventDefault();boton.disabled=true;guardar.disabled=true;
 try{
  const r=await fetch('/config',{method:'POST',body:new URLSearchParams(new FormData(formulario))});
  const texto=await r.text();if(!r.ok)throw Error(texto);
  foto.hidden=true;mensaje.textContent='Ajustes recibidos. La imagen mostrara los valores aplicados; cambiar resolucion reinicia la camara.';
  guardar.disabled=false;
 }catch(e){mensaje.textContent='No se pudo aplicar: '+e.message;}
 finally{boton.disabled=false;}
});
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
        servidor.Get("/config", [this](const httplib::Request&, httplib::Response& r) {
            std::lock_guard<std::mutex> acceso(mutexImagen);
            std::ostringstream json;
            json << "{\"resolucion\":\"" << config.ancho << 'x' << config.alto
                 << "\",\"porcentaje\":" << config.porcentaje << ",\"cuadro\":" << config.cuadro << ",\"hsv\":[";
            for (size_t i=0; i<config.hsv.size(); ++i) {
                const auto& v=config.hsv[i];
                if (i) json << ',';
                json << '[' << v.hmin << ',' << v.hmax << ',' << v.smin << ',' << v.smax
                     << ',' << v.vmin << ',' << v.vmax << ']';
            }
            json << "]}";
            r.set_header("Cache-Control", "no-store");
            r.set_content(json.str(), "application/json");
        });
        servidor.Post("/config", [this](const httplib::Request& q, httplib::Response& r) {
            ConfigDemo nueva;
            auto entero = [&](const char* nombre, int minimo, int maximo) {
                const auto texto = q.get_param_value(nombre);
                if (texto.empty() || texto.find_first_not_of("0123456789") != std::string::npos)
                    throw std::invalid_argument("Valor invalido");
                int valor = std::stoi(texto);
                if (valor < minimo || valor > maximo) throw std::invalid_argument("Fuera de rango");
                return valor;
            };
            try {
                const auto resolucion = q.get_param_value("resolucion");
                if (resolucion == "320x240") { nueva.ancho=320; nueva.alto=240; }
                else if (resolucion == "640x480") { nueva.ancho=640; nueva.alto=480; }
                else if (resolucion == "1280x720") { nueva.ancho=1280; nueva.alto=720; }
                else throw std::invalid_argument("Resolucion invalida");
                nueva.porcentaje = entero("porcentaje", 1, 100);
                nueva.cuadro = entero("cuadro", 10, 100);
                for (size_t i=0; i<nueva.hsv.size(); ++i) {
                    auto leer = [&](const char* campo, int maximo) {
                        return entero(("r"+std::to_string(i)+"_"+campo).c_str(),0,maximo);
                    };
                    auto& v=nueva.hsv[i];
                    v={leer("hmin",179),leer("hmax",179),leer("smin",255),
                       leer("smax",255),leer("vmin",255),leer("vmax",255)};
                    if(v.hmin>v.hmax || v.smin>v.smax || v.vmin>v.vmax)
                        throw std::invalid_argument("Minimo mayor que maximo");
                }
            } catch (const std::exception&) {
                r.status=400;r.set_content("Valores invalidos: revisar limites y minimo <= maximo en cada rango HSV", "text/plain");return;
            }
            std::lock_guard<std::mutex> acceso(mutexImagen);
            config=nueva;++revision;jpeg.clear();
            r.set_content("OK", "text/plain");
        });
        servidor.Post("/finalizar", [this](const httplib::Request&, httplib::Response& r) {
            std::lock_guard<std::mutex> acceso(mutexImagen);
            if (jpeg.empty() || revisionImagen != revision ||
                std::chrono::steady_clock::now()-ultimaImagen > std::chrono::seconds(2)) {
                r.status=409;
                r.set_content("Espere una imagen reciente con los ajustes aplicados antes de guardar.", "text/plain");
                return;
            }
            std::ostringstream informe;
            informe << "VALORES FINALES DE CALIBRACION\nRevision: " << revision
                    << "\nResolucion solicitada: " << config.ancho << 'x' << config.alto
                    << "\nResolucion recibida: " << anchoReal << 'x' << altoReal
                    << "\nFPS solicitados: 30 (no medidos)\nPublicacion: maximo 10 imagenes/s"
                    << "\nConsulta web: pausa de 150 ms entre peticiones"
                    << "\nRegion centrada: " << config.cuadro << "% del ancho y alto"
                    << "\nOcupacion minima: " << config.porcentaje << "% de la region\n";
            const char* nombres[] = {"Rojo bajo", "Rojo alto", "Verde", "Azul", "Amarillo"};
            for (size_t i=0; i<config.hsv.size(); ++i) {
                const auto& v=config.hsv[i];
                informe << nombres[i] << ": H=[" << v.hmin << ',' << v.hmax
                        << "] S=[" << v.smin << ',' << v.smax << "] V=["
                        << v.vmin << ',' << v.vmax << "]\n";
            }
            informe << "H usa escala OpenCV 0..179; S y V 0..255. Rojo une sus dos rangos.\n";
            try {
                const auto carpeta=std::filesystem::current_path()/"calibraciones";
                std::filesystem::create_directories(carpeta);
                const auto id=std::chrono::system_clock::now().time_since_epoch().count();
                const auto ruta=carpeta/("calibracion_"+std::to_string(id)+".txt");
                std::ofstream archivo;
                archivo.exceptions(std::ios::failbit | std::ios::badbit);
                archivo.open(ruta);
                archivo << informe.str();
                archivo.close();
                const std::string salida="Guardado en: "+ruta.string()+"\n\n"+informe.str();
                std::cout << '\n' << salida << std::flush;
                r.set_content(salida, "text/plain; charset=utf-8");
            } catch (const std::exception& e) {
                std::cerr << "Error guardando calibracion: " << e.what() << std::endl;
                r.status=500;
                r.set_content("No se pudo guardar el archivo. Revise permisos y espacio en disco.", "text/plain");
            }
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
        if (!servidor.bind_to_port("0.0.0.0", 8080))
            throw std::runtime_error("No se pudo abrir puerto 8080. Revisar si otra demo esta ejecutandose.");
        hilo = std::thread([this] { servidor.listen_after_bind(); finalizado = true; });
        while (!servidor.is_running() && !finalizado)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (finalizado) {
            hilo.join();
            throw std::runtime_error("El servidor web no pudo iniciar.");
        }
    }

    ConfigDemo configuracion(unsigned& version) {
        std::lock_guard<std::mutex> acceso(mutexImagen);
        version=revision;
        return config;
    }

    void invalidar() {
        std::lock_guard<std::mutex> acceso(mutexImagen);
        jpeg.clear();
    }

    void publicar(const cv::Mat& imagen, unsigned version) {
        if (finalizado) throw std::runtime_error("El servidor web se detuvo.");
        std::vector<uchar> bytes;
        if (!cv::imencode(".jpg", imagen, bytes, {cv::IMWRITE_JPEG_QUALITY, 75}))
            throw std::runtime_error("No se pudo preparar la imagen para el navegador.");
        std::lock_guard<std::mutex> acceso(mutexImagen);
        if (version != revision) return; // No publicar un frame con ajustes anteriores.
        revisionImagen=version;anchoReal=imagen.cols;altoReal=imagen.rows;
        jpeg.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        ultimaImagen = std::chrono::steady_clock::now();
    }

    ~VistaDemo() {
        servidor.stop();
        if (hilo.joinable()) hilo.join();
    }
};
