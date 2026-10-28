// Demo para Raspberry Pi 3 con libcamera: ver DEMO_COLORES.md.
#include <opencv2/opencv.hpp> //Captura y Procesamiento de imagenes
    // Los cv:: pertenecen a esta libreria
#include <csignal> //Solicitar salida con control+c
#include <iomanip> //Formatea codigos en hexadecimal
#include <iostream> //Inputs-outputs en terminal
#include <stdexcept> //Permite lanzar excepciones al tener errores
#include "vista_demo.hpp" // Vista web para VS Code Remote SSH
#include <memory>
#include <sstream>
#include <string> // Strings 

// Flags que muestra la demo. No se envia nada por I2C en estas funciones.
const int NINGUNO = 0x00;   //No hay suficiente de ningun color buscado
const int ROJO = 0x01;
const int VERDE = 0x02;
const int AZUL = 0x03;
const int INDETERMINADO = 0xFF; //Imagen no valida o varios colores

// AJUSTAR con la pista real. Estos valores aun no estan calibrados.
const double FRACCION_MINIMA = 0.08; // Cuanto de un color debe ocupar de la pantalla (8%)
const int SATURACION_MINIMA = 80; //Ajustar para descartar colores blancos y grises
const int BRILLO_MINIMO = 50; //Ajustar para descartar pixeles oscuros

volatile std::sig_atomic_t terminar = 0; //Se puede usar la variable para mandar senal de salida
void solicitarSalida(int) { terminar = 1; } //Funcion para salir del ciclo

// Analiza una imagen BGR. No cuenta, no bloquea y no envia I2C.
int detectarColor(const cv::Mat& imagen) {
    if (imagen.empty() || imagen.type() != CV_8UC3)
        return INDETERMINADO; //Condicion de error

    cv::Mat hsv, rojo1, rojo2, rojo, verde, azul; //Crear imagenes auxiliares
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



    const double minimo = imagen.total() * FRACCION_MINIMA; 
    bool hayRojo = cv::countNonZero(rojo) >= minimo; //Definir si hay suficiente rojo
    bool hayVerde = cv::countNonZero(verde) >= minimo; //Definir si hay suficiente verde
    bool hayAzul = cv::countNonZero(azul) >= minimo; //Definir si hay suficiente azul

    if (int(hayRojo) + int(hayVerde) + int(hayAzul) > 1)
        return INDETERMINADO; //Se detecto mas de 1 color
    if (hayRojo) return ROJO; //Regresar codigo correspondiente
    if (hayVerde) return VERDE; //Regresar codigo correspondiente
    if (hayAzul) return AZUL; //Regresar codigo correspondiente
    return NINGUNO;// No detecto ninguno de los 3 colores
}

const char* nombreColor(int color) { //Convertir el codigo hex a un string. (Eventualmente realizaraccion() recibira el string y mandara la accion correspondiente)
    switch (color) {
        case ROJO: return "ROJO";
        case VERDE: return "VERDE";
        case AZUL: return "AZUL";
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
    const std::string pipeline = //Recorrido de la imagen
        "libcamerasrc ! video/x-raw,format=NV12,width=640,height=480,"
        "framerate=30/1,colorimetry=bt601 ! videoconvert ! "
        "video/x-raw,format=BGR ! appsink max-buffers=1 drop=true sync=false";
        //Accede a la camara por libcamera
        //Solicita tamaño de la imagen
        //Solicita framerate/ 30 frames por seg , solicitar 30 no garantiza 30, revisar al probar codigo
        //formatnv12 y colorimetry especifica formato de entrada y como interpretar color
        //appsink permite que opencv reciba las imagenes
        //maxbuffers evita acumular imagenes viejas que no hayan sido procesadas
        //Sync evita esperar una sincronizacion de presentacion
    cv::VideoCapture camara(pipeline, cv::CAP_GSTREAMER); //Abre la captura con gstreamer (organiza recorrido y convierte imagenes)
    if (!camara.isOpened()) {
        std::cerr << "No se pudo abrir la camara. Revisar libcamera y GStreamer.\n"; //Msg de error
        return 1;
    }

    cv::Mat imagen; //Donde se guarda la imagen
    int ultimoColor = -1; //Donde se almacena el ultimo color detectado
    // POSIBLE MEJORA: mover captura y deteccion a un thread de vision.
    // Esta demo usa un ciclo simple; no ejecuta acciones ni bloqueos.
    while (!terminar) { 
        if (!camara.read(imagen) || imagen.empty()) { //Obtener frame con camara.read y almacenarla. Ademas compara a ver si hay error
            std::cerr << "Error de captura: flag invalida (0xFF).\n"; //msg de error
            return 1;
        }
        if (imagen.cols < 4 || imagen.rows < 4) //Comprobar tamaño suficiente de la imagen capturada
            throw std::runtime_error("Tamano de imagen invalido.");

        // Ajustar esta region segun la posicion de la pista en la imagen.
        cv::Rect region(imagen.cols / 4, imagen.rows / 4,
                        imagen.cols / 2, imagen.rows / 2);
        int flagColor = detectarColor(imagen(region)); //Analizar imagen obtenida

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
            // Publicar hasta 10 imagenes/s para reducir carga en la Raspberry Pi 3.
            auto ahora = std::chrono::steady_clock::now();
            if (vista && ahora - ultimaPublicacion >= std::chrono::milliseconds(100)) {
                vista->publicar(imagen);
                ultimaPublicacion = ahora;
            }
        }
        if (mostrarVentana) {
            cv::imshow("Demo colores - Q para salir", imagen);
            int tecla = cv::waitKey(1);
            if (tecla == 'q' || tecla == 27) break;
        }
    }
    return 0; //Finalizar
}

int main(int argc, char** argv) {
    // Por defecto: vista en navegador para Remote SSH.
    std::string modo = argc == 2 ? argv[1] : "--web";
    if (argc > 2 || (modo != "--web" && modo != "--ventana" && modo != "--sin-ventana")) {
        std::cerr << "Uso: ./demo_colores [--web|--ventana|--sin-ventana]\n";
        return 1;
    }
    std::signal(SIGINT, solicitarSalida);
    std::signal(SIGTERM, solicitarSalida);
    try {
        int resultado = detectarColorDemo(modo == "--ventana", modo == "--web");
        if (modo == "--ventana") cv::destroyAllWindows();
        return resultado;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
