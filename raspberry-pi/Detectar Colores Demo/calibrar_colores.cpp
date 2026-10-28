// Demo para Raspberry Pi 3 con libcamera: ver DEMO_COLORES.md.
#include <opencv2/opencv.hpp> //Captura y Procesamiento de imagenes
    // Los cv:: pertenecen a esta libreria
#include <csignal> //Solicitar salida con control+c
#include <iomanip> //Formatea codigos en hexadecimal
#include <iostream> //Inputs-outputs en terminal
#include <stdexcept> //Permite lanzar excepciones al tener errores
#include "vista_calibracion.hpp" // Vista web para VS Code Remote SSH
#include <memory>
#include <algorithm>
#include <sstream>
#include <string> // Strings

// Flags que muestra la demo. No se envia nada por I2C en estas funciones.
const int NINGUNO = 0x00;   //No hay suficiente de ningun color buscado
const int ROJO = 0x01;
const int VERDE = 0x02;
const int AZUL = 0x03;
const int AMARILLO = 0x04;
const int INDETERMINADO = 0xFF; //Imagen no valida o varios colores

// AJUSTAR con la pista real. Estos valores aun no estan calibrados.

volatile std::sig_atomic_t terminar = 0; //Se puede usar la variable para mandar senal de salida
void solicitarSalida(int) { terminar = 1; } //Funcion para salir del ciclo

// Analiza una imagen BGR. No cuenta, no bloquea y no envia I2C.
int detectarColor(const cv::Mat& imagen, const ConfigDemo& config) {
    if (imagen.empty() || imagen.type() != CV_8UC3)
        return INDETERMINADO; //Condicion de error

    cv::Mat hsv, rojo1, rojo2, rojo, verde, azul, amarillo;
    cv::cvtColor(imagen, hsv, cv::COLOR_BGR2HSV); //Convierte BGR a HSV
         // HSV: tono H de 0 a 179; saturacion S y brillo V de 0 a 255.
    // El rojo aparece en los dos extremos de H.

    auto mascara = [&](int indice, cv::Mat& salida) {
        const auto& rango = config.hsv[indice];
        cv::inRange(hsv, cv::Scalar(rango.hmin, rango.smin, rango.vmin),
                    cv::Scalar(rango.hmax, rango.smax, rango.vmax), salida);
    };
    mascara(0, rojo1);
    mascara(1, rojo2);
    cv::bitwise_or(rojo1, rojo2, rojo);
    mascara(2, verde);
    mascara(3, azul);
    mascara(4, amarillo);
    const double minimo = imagen.total() * config.porcentaje / 100.0;
    bool hayRojo = cv::countNonZero(rojo) >= minimo; //Definir si hay suficiente rojo
    bool hayVerde = cv::countNonZero(verde) >= minimo; //Definir si hay suficiente verde
    bool hayAzul = cv::countNonZero(azul) >= minimo; //Definir si hay suficiente azul
    bool hayAmarillo = cv::countNonZero(amarillo) >= minimo;

    if (int(hayRojo) + int(hayVerde) + int(hayAzul) + int(hayAmarillo) > 1)
        return INDETERMINADO; //Se detecto mas de 1 color
    if (hayRojo) return ROJO; //Regresar codigo correspondiente
    if (hayVerde) return VERDE; //Regresar codigo correspondiente
    if (hayAzul) return AZUL; //Regresar codigo correspondiente
    if (hayAmarillo) return AMARILLO;
    return NINGUNO;
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


//Para probar la funcion de detectarcolor()
// Abre la camara y publica la flag en pantalla y terminal.
// detectarColor() sigue siendo la funcion que analiza una sola imagen.
int detectarColorDemo(bool mostrarVentana, bool mostrarWeb) {
    std::unique_ptr<VistaDemo> vista;
    if (mostrarWeb) {
        vista = std::make_unique<VistaDemo>();
        std::cout << "Vista web: reenviar puerto 8080 en VS Code y abrir http://localhost:8080\n";
    }
    auto ultimaPublicacion = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    ConfigDemo config;
    unsigned version = 0;
    int anchoAbierto = 0, altoAbierto = 0;
    unsigned intento = 0;
    bool fallo = false;
    cv::VideoCapture camara;
    cv::Mat imagen; //Donde se guarda la imagen
    int ultimoColor = -1; //Donde se almacena el ultimo color detectado
    // POSIBLE MEJORA: mover captura y deteccion a un thread de vision.
    // Esta demo usa un ciclo simple; no ejecuta acciones ni bloqueos.
    while (!terminar) {
        if (vista) config = vista->configuracion(version);
        if (fallo && version == intento) {
            // Mantener la interfaz disponible para elegir otra resolucion/reintentar.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        if (!camara.isOpened() || config.ancho != anchoAbierto || config.alto != altoAbierto) {
            if (vista) vista->invalidar();
            camara.release();
            std::ostringstream pipeline;
            pipeline << "libcamerasrc ! video/x-raw,format=NV12,width=" << config.ancho
                     << ",height=" << config.alto
                     << ",framerate=30/1,colorimetry=bt601 ! videoconvert ! "
                     << "video/x-raw,format=BGR ! appsink max-buffers=1 drop=true sync=false";
            bool abierta = false;
            try { abierta = camara.open(pipeline.str(), cv::CAP_GSTREAMER); }
            catch (const cv::Exception& e) { std::cerr << e.what() << std::endl; }
            if (terminar) break;
            if (!abierta) {
                std::cerr << "No se pudo abrir la camara con " << config.ancho << 'x'
                          << config.alto << ". Seleccione otra resolucion y pulse Aplicar.\n";
                if (!vista) return 1;
                fallo=true;intento=version;
                continue;
            }
            fallo=false;anchoAbierto=config.ancho;altoAbierto=config.alto;
        }
        const bool capturada = camara.read(imagen);
        if (terminar) break; // Ctrl+C: no procesar ni publicar otra imagen.
        if (!capturada || imagen.empty()) {
            std::cerr << "Error de captura: flag invalida (0xFF).\n"; //msg de error
            if (!vista) return 1;
            vista->invalidar();camara.release();fallo=true;intento=version;continue;
        }
        if (imagen.cols < 4 || imagen.rows < 4) //Comprobar tamaño suficiente de la imagen capturada
            throw std::runtime_error("Tamano de imagen invalido.");

        // Ajustar esta region segun la posicion de la pista en la imagen.
        const int anchoRegion = std::max(1, imagen.cols * config.cuadro / 100);
        const int altoRegion = std::max(1, imagen.rows * config.cuadro / 100);
        cv::Rect region((imagen.cols-anchoRegion)/2, (imagen.rows-altoRegion)/2,
                        anchoRegion, altoRegion);
        int flagColor = detectarColor(imagen(region), config); //Analizar imagen obtenida

        // La variable se actualiza en CADA frame. Solo imprimimos al cambiar
        // para no llenar la terminal con cientos de lineas repetidas.
        if (flagColor != ultimoColor) {
            std::cout << nombreColor(flagColor) << " -> 0x"
                      << std::hex << std::uppercase << std::setw(2)
                      << std::setfill('0') << flagColor << std::dec << std::endl;
            ultimoColor = flagColor;
        }

        if (mostrarVentana || mostrarWeb) { // Dibujar despues de detectar.
            std::ostringstream etiqueta;
            etiqueta << nombreColor(flagColor) << " -> 0x" << std::hex
                     << std::uppercase << std::setw(2) << std::setfill('0') << flagColor;
            cv::rectangle(imagen, region, cv::Scalar(0, 255, 255), 2);
            cv::putText(imagen, etiqueta.str(), cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
            std::ostringstream ajustes;
            ajustes << imagen.cols << 'x' << imagen.rows << " ROI " << config.cuadro
                    << "% min " << config.porcentaje << '%';
            cv::putText(imagen, ajustes.str(), cv::Point(10, 55),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255,255,255), 1);
            // Publicar hasta 10 imagenes/s para reducir carga en la Raspberry Pi 3.
            auto ahora = std::chrono::steady_clock::now();
            if (vista && ahora - ultimaPublicacion >= std::chrono::milliseconds(100)) {
                vista->publicar(imagen, version);
                ultimaPublicacion = ahora;
            }
        }
        if (mostrarVentana) {
            cv::imshow("Demo colores - Q para salir", imagen);
            int tecla = cv::waitKey(1);
            if (tecla == 'q' || tecla == 27) break;
        }
    }
    std::cout << "Cerrando calibracion: liberando camara y servidor web..." << std::endl;
    camara.release();
    vista.reset(); // Detiene HTTP y espera a que termine su hilo.
    return 0;
}

int main(int argc, char** argv) {
    // Por defecto: vista en navegador para Remote SSH.
    std::string modo = argc == 2 ? argv[1] : "--web";
    if (argc > 2 || (modo != "--web" && modo != "--ventana" && modo != "--sin-ventana")) {
        std::cerr << "Uso: ./calibrar_colores [--web|--ventana|--sin-ventana]\n";
        return 1;
    }
    std::signal(SIGINT, solicitarSalida);
    std::signal(SIGTERM, solicitarSalida);
    std::cout << "Para cerrar: Ctrl+C en esta terminal." << std::endl;
    try {
        int resultado = detectarColorDemo(modo == "--ventana", modo == "--web");
        if (modo == "--ventana") cv::destroyAllWindows();
        return resultado;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
