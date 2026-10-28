// Coordinador Raspberry. V1: salida por terminal; sin transmisiones I2C.
#include <opencv2/opencv.hpp>
#include <httplib.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <utility>

// Resultados internos: los valores negativos nunca se envian al ESP32.
constexpr int NINGUNO = -1, INDETERMINADO = -2;
constexpr int ROJO = 1, VERDE = 2, AZUL = 3, AMARILLO = 4;
constexpr double FRACCION_MINIMA = 0.08;
constexpr int SATURACION_MINIMA = 80, BRILLO_MINIMO = 50;
// Estos son codigos de ACCION, no resultados del detector.
constexpr int INICIO = 0x00, FIN = 0xFF;
constexpr int BIF_DERECHA = 0x04, BIF_IZQUIERDA = 0x05;
volatile std::sig_atomic_t terminar = 0;
void solicitarSalida(int) { terminar = 1; }

// Analiza una imagen BGR. No cuenta, no bloquea y no envia I2C.
int detectarColor(const cv::Mat& imagen, std::array<double, 4>& porcentajes) {
    porcentajes.fill(0);
    if (imagen.empty() || imagen.type() != CV_8UC3)
        return INDETERMINADO; //Condicion de error

    cv::Mat hsv, rojo1, rojo2, rojo, verde, azul, amarillo;
    cv::cvtColor(imagen, hsv, cv::COLOR_BGR2HSV); //Convierte BGR a HSV
         // HSV: tono H de 0 a 179; saturacion S y brillo V de 0 a 255.
    // El rojo aparece en los dos extremos de H.

    //Crear las mascaras de cada color, inrange es para revisar cada pixel
    cv::inRange(hsv, cv::Scalar(0, SATURACION_MINIMA, BRILLO_MINIMO),
                cv::Scalar(10, 255, 255), rojo1);
    cv::inRange(hsv, cv::Scalar(170, SATURACION_MINIMA, BRILLO_MINIMO),
                cv::Scalar(179, 255, 255), rojo2);
    cv::bitwise_or(rojo1, rojo2, rojo); //Or de los 2 rojos para unirlos en una sola mascara

    cv::inRange(hsv, cv::Scalar(35, SATURACION_MINIMA, BRILLO_MINIMO),
                cv::Scalar(85, 255, 255), verde);
    cv::inRange(hsv, cv::Scalar(95, SATURACION_MINIMA, BRILLO_MINIMO),
                cv::Scalar(130, 255, 255), azul);
    // Rango inicial de amarillo; calibrar con el material y luz de la pista.
    cv::inRange(hsv, cv::Scalar(20, SATURACION_MINIMA, BRILLO_MINIMO),
                cv::Scalar(34, 255, 255), amarillo);



    const double minimo = imagen.total() * FRACCION_MINIMA;
    bool hayRojo = cv::countNonZero(rojo) >= minimo; //Definir si hay suficiente rojo
    bool hayVerde = cv::countNonZero(verde) >= minimo; //Definir si hay suficiente verde
    bool hayAzul = cv::countNonZero(azul) >= minimo; //Definir si hay suficiente azul
    bool hayAmarillo = cv::countNonZero(amarillo) >= minimo;
    porcentajes = {100.0 * cv::countNonZero(rojo) / imagen.total(),
                   100.0 * cv::countNonZero(verde) / imagen.total(),
                   100.0 * cv::countNonZero(azul) / imagen.total(),
                   100.0 * cv::countNonZero(amarillo) / imagen.total()};

    if (int(hayRojo) + int(hayVerde) + int(hayAzul) + int(hayAmarillo) > 1)
        return INDETERMINADO; //Se detecto mas de 1 color
    if (hayRojo) return ROJO; //Regresar codigo correspondiente
    if (hayVerde) return VERDE; //Regresar codigo correspondiente
    if (hayAzul) return AZUL; //Regresar codigo correspondiente
    if (hayAmarillo) return AMARILLO;
    return NINGUNO; // No detecto ninguno de los cuatro colores.
}

const char* nombreColor(int color) { //Convertir el codigo hex a un string. (Eventualmente realizaraccion() recibira el string y mandara la accion correspondiente)
    switch (color) {
        case ROJO: return "ROJO";
        case VERDE: return "VERDE";
        case AZUL: return "AZUL";
        case AMARILLO: return "AMARILLO";
        case NINGUNO: return "NINGUNO";
        default: return "INDETERMINADO";
    }
}




struct Estado {
    std::mutex mutex;
    bool activo = false, iniciar = false, detener = false;
    std::array<int, 3> acciones{1, 2, 3};
    std::array<unsigned, 4> conteo{0, 0, 0, 0};
    std::string ramal = "derecha";
    std::string error;
    int ultimoCodigo = -1;
    std::string jpeg;
    std::chrono::steady_clock::time_point ultimaImagen{};
};
struct Bloqueo {
    int ultimoColor = NINGUNO;
};

// V1: evita repetir la misma lectura consecutiva. El bloqueo temporal de seis
// segundos y la confirmacion de salida se incorporaran en V2.
// No enviar NINGUNO ni INDETERMINADO como comandos de movimiento.
bool LogicaBloqueo(int color, Bloqueo& bloqueo) {
    if (color == INDETERMINADO) return false;
    if (color == NINGUNO) {
        bloqueo.ultimoColor = NINGUNO;
        return false;
    }
    if (color < ROJO || color > AMARILLO || color == bloqueo.ultimoColor) return false;
    bloqueo.ultimoColor = color;
    return true;
}

// Unico punto de salida. Sustituir el cuerpo por el transporte I2C en V3.
// Por ahora NO abre /dev/i2c ni escribe en GPIO.
bool EnviarCodigo(int codigo, Estado& estado) {
    if (!((codigo >= INICIO && codigo <= BIF_IZQUIERDA) || codigo == FIN)) {
        std::cerr << "Codigo de salida invalido: " << codigo << std::endl;
        return false;
    }
    estado.ultimoCodigo = codigo; // Llamar con el mutex de estado adquirido.
    std::cout << "[TERMINAL; sin I2C] Codigo de accion -> 0x"
              << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
              << codigo << std::dec << std::endl;
    return true;
}

const char* nombreAccion(int codigo) {
    switch (codigo) {
        case 1: return "frenar";
        case 2: return "retroceder";
        case 3: return "velocidad";
        default: return "invalida";
    }
}
int codigoAccion(const std::string& nombre) {
    for (int codigo = 1; codigo <= 3; ++codigo)
        if (nombre == nombreAccion(codigo)) return codigo;
    return -1;
}

int accionParaColor(int color, const Estado& estado) {
    if (color == AMARILLO)
        return estado.ramal == "derecha" ? BIF_DERECHA : BIF_IZQUIERDA;
    if (color >= ROJO && color <= AZUL) return estado.acciones[color - ROJO];
    return -1; // No convertir ausencia o ambiguedad en orden.
}

void CicloEjecucion(Estado& estado) {
    // Misma cadena de captura que la demo funcional.
    const std::string pipeline =
        "libcamerasrc ! video/x-raw,format=NV12,width=640,height=480,"
        "framerate=30/1,colorimetry=bt601 ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink max-buffers=1 drop=true sync=false";
    cv::VideoCapture camara(pipeline, cv::CAP_GSTREAMER);
    if (!camara.isOpened()) throw std::runtime_error("No se pudo abrir la camara");
    Bloqueo bloqueo;
    {
        std::lock_guard<std::mutex> acceso(estado.mutex);
        if (estado.detener || terminar) return;
        // Arrancar solo despues de abrir correctamente la camara.
        estado.activo = true;
        estado.iniciar = false;
        estado.conteo.fill(0);
        EnviarCodigo(INICIO, estado);
    }
    cv::Mat imagen;
    int ultimoColor = -3; // Distinto de todos los resultados del detector.
    auto ultimaPublicacion = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    while (!terminar) {
        {
            std::lock_guard<std::mutex> acceso(estado.mutex);
            if (estado.detener) break;
        }
        // Igual que en la demo: read puede esperar al controlador de camara.
        if (!camara.read(imagen) || imagen.empty())
            throw std::runtime_error("Error de captura");
        if (imagen.cols < 4 || imagen.rows < 4)
            throw std::runtime_error("Tamano de imagen invalido");
        cv::Rect region(imagen.cols / 4, imagen.rows / 4,
                        imagen.cols / 2, imagen.rows / 2);
        std::array<double, 4> porcentajes{};
        const int color = detectarColor(imagen(region), porcentajes);
        // Dibujar DESPUES de detectar; no altera las mascaras ni el resultado.
        // Codificar fuera del mutex para no bloquear las solicitudes de control.
        std::string nuevoJpeg;
        const auto captura = std::chrono::steady_clock::now();
        if (captura - ultimaPublicacion >= std::chrono::milliseconds(100)) {
            cv::rectangle(imagen, region, cv::Scalar(0, 255, 255), 2);
            cv::rectangle(imagen, cv::Rect(0, 0, imagen.cols, std::min(90, imagen.rows)),
                          cv::Scalar(0, 0, 0), cv::FILLED);
            cv::putText(imagen, nombreColor(color), cv::Point(10, 25),
                        cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(255, 255, 255), 1);
            std::ostringstream texto;
            texto << std::fixed << std::setprecision(1)
                  << "R " << porcentajes[0] << "%  V " << porcentajes[1]
                  << "%  A " << porcentajes[2] << "%  Amarillo " << porcentajes[3] << "%";
            cv::putText(imagen, texto.str(), cv::Point(10, 50),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            cv::putText(imagen, "Umbral: 8% de la region central", cv::Point(10, 75),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            std::vector<uchar> bytes;
            if (!cv::imencode(".jpg", imagen, bytes, {cv::IMWRITE_JPEG_QUALITY, 75}))
                throw std::runtime_error("No se pudo codificar la vista de camara");
            nuevoJpeg.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
            ultimaPublicacion = captura;
        }
        std::lock_guard<std::mutex> acceso(estado.mutex);
        if (estado.detener || terminar) break;
        if (!nuevoJpeg.empty()) {
            estado.jpeg = std::move(nuevoJpeg);
            estado.ultimaImagen = captura;
        }
        if (color != ultimoColor) {
            std::cout << "[VISION] " << nombreColor(color) << std::endl;
            ultimoColor = color;
        }
        if (LogicaBloqueo(color, bloqueo)) {
            ++estado.conteo[color - 1];
            EnviarCodigo(accionParaColor(color, estado), estado);
        }
    }
}

// HTTP solo local: Python es un puente y C++ mantiene el estado autoritativo.
// Los handlers solicitan inicio/fin; solo el hilo principal controla la camara
// y llama EnviarCodigo(). La configuracion se protege con el mismo mutex.
void configurarAPI(httplib::Server& api, Estado& estado) {
    api.Get("/api/camara.jpg", [&](const httplib::Request&, httplib::Response& r) {
        r.set_header("Cache-Control", "no-store");
        std::lock_guard<std::mutex> acceso(estado.mutex);
        if (!estado.activo || estado.detener || estado.jpeg.empty() ||
            std::chrono::steady_clock::now() - estado.ultimaImagen > std::chrono::seconds(2)) {
            r.status = 503;
            r.set_content("{\"error\":\"Camara detenida o sin imagen reciente\"}", "application/json");
            return;
        }
        r.set_content(estado.jpeg, "image/jpeg");
    });
    api.Get("/api/estado", [&](const httplib::Request&, httplib::Response& r) {
        std::lock_guard<std::mutex> acceso(estado.mutex);
        std::ostringstream json;
        json << "{\"en_movimiento\":" << (estado.activo ? "true" : "false")
             << ",\"iniciando\":" << (estado.iniciar ? "true" : "false")
             << ",\"modo\":\"terminal\",\"error\":\"" << estado.error << "\""
             << ",\"ultimo_codigo\":" << estado.ultimoCodigo
             << ",\"conteo\":{\"rojo\":" << estado.conteo[0]
             << ",\"verde\":" << estado.conteo[1] << ",\"azul\":" << estado.conteo[2]
             << ",\"amarillo\":" << estado.conteo[3]
             << "},\"asignacion\":{\"rojo\":\"" << nombreAccion(estado.acciones[0])
             << "\",\"verde\":\"" << nombreAccion(estado.acciones[1])
             << "\",\"azul\":\"" << nombreAccion(estado.acciones[2])
             << "\"},\"bifurcacion\":\"" << estado.ramal << "\"}";
        r.set_header("Cache-Control", "no-store");
        r.set_content(json.str(), "application/json");
    });
    api.Post("/api/inicio", [&](const httplib::Request&, httplib::Response& r) {
        std::lock_guard<std::mutex> acceso(estado.mutex);
        if (estado.detener) {
            r.status = 409;
            r.set_content("{\"error\":\"Esperar a que termine la parada\"}", "application/json");
            return;
        }
        if (!estado.activo && !estado.iniciar) {
            estado.iniciar = true;
            estado.error.clear();
        }
        r.status = 202;
        r.set_content("{\"ok\":true}", "application/json");
    });
    api.Post("/api/fin", [&](const httplib::Request&, httplib::Response& r) {
        std::lock_guard<std::mutex> acceso(estado.mutex);
        estado.detener = true;
        r.status = 202;
        r.set_content("{\"ok\":true}", "application/json");
    });
    api.Post("/api/config", [&](const httplib::Request& q, httplib::Response& r) {
        std::lock_guard<std::mutex> acceso(estado.mutex);
        if (estado.activo || estado.iniciar || estado.detener) {
            r.status = 409;
            r.set_content("{\"error\":\"Detenga el ciclo antes de configurar\"}", "application/json");
            return;
        }
        std::array<int, 3> acciones{codigoAccion(q.get_param_value("rojo")),
            codigoAccion(q.get_param_value("verde")), codigoAccion(q.get_param_value("azul"))};
        const auto ramal = q.get_param_value("bifurcacion");
        if (acciones[0] < 0 || acciones[1] < 0 || acciones[2] < 0 ||
            acciones[0] == acciones[1] || acciones[0] == acciones[2] || acciones[1] == acciones[2] ||
            (ramal != "derecha" && ramal != "izquierda")) {
            r.status = 400;
            r.set_content("{\"error\":\"Asignacion o ramal invalido\"}", "application/json");
            return;
        }
        estado.acciones = acciones;
        estado.ramal = ramal; // Amarillo dispara la direccion elegida.
        r.set_content("{\"ok\":true}", "application/json");
    });
}

int main() {
    std::signal(SIGINT, solicitarSalida);
    std::signal(SIGTERM, solicitarSalida);
    Estado estado;
    httplib::Server api;
    api.new_task_queue = [] { return new httplib::ThreadPool(2); };
    api.set_read_timeout(2, 0);
    api.set_write_timeout(2, 0);
    configurarAPI(api, estado);
    if (!api.bind_to_port("127.0.0.1", 8081)) {
        std::cerr << "No se pudo abrir el puerto local 8081\n";
        return 1;
    }
    std::atomic<bool> servidorFinalizado{false};
    std::thread servidor([&] { api.listen_after_bind(); servidorFinalizado = true; });
    std::cout << "API local: 8081. Abra la interfaz del servidor Python en el puerto 8000.\n"
              << "Esperando Inicio. Salida por terminal, I2C desactivado.\n";
    while (!terminar && !servidorFinalizado) {
        bool comenzar;
        {
            std::lock_guard<std::mutex> acceso(estado.mutex);
            if (estado.detener) {
                estado.iniciar = false;
                estado.detener = false;
                EnviarCodigo(FIN, estado);
            }
            comenzar = estado.iniciar;
        }
        if (!comenzar) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }
        try {
            CicloEjecucion(estado);
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            std::lock_guard<std::mutex> acceso(estado.mutex);
            estado.error = "Fallo de camara o procesamiento; revisar terminal";
        }
        {
            std::lock_guard<std::mutex> acceso(estado.mutex);
            EnviarCodigo(FIN, estado);
            estado.activo = estado.iniciar = estado.detener = false;
            estado.jpeg.clear();
        }
        std::cout << "Ciclo detenido. Esperando Inicio.\n";
    }
    api.stop();
    servidor.join();
    return 0;
}
