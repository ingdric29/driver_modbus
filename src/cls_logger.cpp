/*
CLASE LOGGER:
Permite hacer un registro de los eventos que sucedan en la ejecucion del programa.
    Codigo realizado por: Ing. Javier Flores
    Ultima modificacion: 15/11/2024

*/
#include "include/cls_logger.h"
#include "include/monitor_state.h"
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp> // Es necesario instalar: sudo apt install libboost-filesystem-dev
#include <ctime>
#include <iomanip>
#include <sstream>
#include <sys/stat.h> // Para stat y mkdir
#include <cstdio>     // Para rename y remove
#include <cstring>    // Para manejar cadenas en C
#include <cstdint>
#include <vector>
#include <mutex>
namespace {
std::mutex &log_mutex() {
    static std::mutex m;
    return m;
}
} // namespace

std::string obtenerDirectorio(std::string nombreCarpeta) {
    boost::filesystem::path directorioPrograma = boost::filesystem::current_path();
    boost::filesystem::path directorioCarpeta = directorioPrograma.parent_path();

    //std::string directorioCarpetaLog = directorioCarpeta.string() + "/" + nombreCarpeta;
    //return directorioCarpetaLog;

    boost::filesystem::path rutaFinal =
        directorioCarpeta / nombreCarpeta; // Usa '/' nativo del sistema
    return rutaFinal.string();
}

Logger::Logger(size_t limiteBytes)
    : limiteBytes(limiteBytes), nombreArchivo("devlog.log"),
      directorioLog(obtenerDirectorio("log")) {

    crearDirectorioLog();
}

void Logger::registrarError(const std::string &mensajeError, bool mostrarConsola) {
    monitor::State::instance().event(mensajeError);
    std::lock_guard<std::mutex> lock(log_mutex());
    verificarTamañoArchivo();

    std::ofstream archivoLog(directorioLog + "/" + nombreArchivo, std::ios::app);
    if (archivoLog.is_open()) {
        archivoLog << "[" << obtenerFechaHoraActual() << "] " << mensajeError << std::endl;
        archivoLog.close();

        if (mostrarConsola)
            std::cerr << "[" << obtenerFechaHoraActual() << "] " << mensajeError << std::endl;
    } else {
        std::cerr << "No se pudo abrir el archivo de log: " << directorioLog + "/" + nombreArchivo
                  << std::endl;
    }
}

void Logger::crearDirectorioLog() {
    struct stat info;
    if (stat(directorioLog.c_str(), &info) != 0) {
// Si el directorio no existe, intenta crearlo
#ifdef _WIN32
        _mkdir(directorioLog.c_str()); // En Windows
#else
        mkdir(directorioLog.c_str(), 0777); // En sistemas UNIX
#endif
    }
}

void Logger::verificarTamañoArchivo() {
    std::string rutaArchivo = directorioLog + "/" + nombreArchivo;

    struct stat archivoInfo;
    if (stat(rutaArchivo.c_str(), &archivoInfo) == 0 && archivoInfo.st_size >= 0 &&
        static_cast<std::uintmax_t>(archivoInfo.st_size) > limiteBytes) {
        rotarArchivos();
    }
}

void Logger::rotarArchivos() {
    limpiarArchivosAntiguos();

    int indice = 1;
    std::string nuevoNombre;
    struct stat buffer;

    do {
        nuevoNombre = obtenerNombreArchivoConIndice(indice++);
    } while (stat((directorioLog + "/" + nuevoNombre).c_str(), &buffer) == 0);

    rename((directorioLog + "/" + nombreArchivo).c_str(),
           (directorioLog + "/" + nuevoNombre).c_str());
}

void Logger::limpiarArchivosAntiguos() {
    const int MAX_ARCHIVOS_LOG = 5;
    std::vector<std::string> archivosParaBorrar;

    for (int i = 1; i <= MAX_ARCHIVOS_LOG - 1; ++i) {
        std::string nombreArchivo = obtenerNombreArchivoConIndice(i);
        std::string rutaArchivo = directorioLog + "/" + nombreArchivo;

        struct stat buffer;
        if (stat(rutaArchivo.c_str(), &buffer) == 0) {
            archivosParaBorrar.push_back(rutaArchivo);
        }
    }

    if (archivosParaBorrar.size() == MAX_ARCHIVOS_LOG - 1) {
        for (const std::string &archivo : archivosParaBorrar) {
            remove(archivo.c_str());
        }
    }
}

std::string Logger::obtenerNombreArchivoConIndice(int indice) {
    return "devlog_" + std::to_string(indice) + ".log";
}

std::string Logger::obtenerFechaHoraActual() {
    auto t = std::time(nullptr);
    std::tm tm;
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
