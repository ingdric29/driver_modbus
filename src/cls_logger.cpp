/*
 * Archivo: cls_logger.cpp
 * Descripción: Registra eventos, escribe logs y rota archivos por tamaño.
 * Necesita: Directorio padre del trabajo, permisos de escritura y límite de tamaño.
 * Retornos: Métodos de escritura: void; helpers: ruta o fecha. Notifica fallos de archivo en
 * consola.
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
/**
 * Obtiene el bloqueo único de escritura y rotación del logger.
 * Parámetros: ninguno.
 * Retorno: referencia al mutex compartido.
 */
std::mutex &log_mutex() {
    static std::mutex m;
    return m;
}
} // namespace

/**
 * Resuelve una ruta respecto al padre del directorio de trabajo.
 * Parámetros: std::string nombreCarpeta
 * Retorno: Ruta de texto; no se basa en la ubicación del ejecutable.
 */
std::string obtenerDirectorio(std::string nombreCarpeta) {
    boost::filesystem::path directorioPrograma = boost::filesystem::current_path();
    boost::filesystem::path directorioCarpeta = directorioPrograma.parent_path();

    //std::string directorioCarpetaLog = directorioCarpeta.string() + "/" + nombreCarpeta;
    //return directorioCarpetaLog;

    boost::filesystem::path rutaFinal =
        directorioCarpeta / nombreCarpeta; // Usa '/' nativo del sistema
    return rutaFinal.string();
}

/**
 * Configura límite y ruta del log y crea su directorio si falta.
 * Parámetros: size_t limiteBytes
 * Retorno: Sin valor de retorno (constructor).
 */
Logger::Logger(size_t limiteBytes)
    : limiteBytes(limiteBytes), nombreArchivo("devlog.log"),
      directorioLog(obtenerDirectorio("log")) {

    crearDirectorioLog();
}

/**
 * Registra el evento y escribe el archivo de log.
 * Parámetros: const std::string &mensajeError, bool mostrarConsola
 * Retorno: Ninguno (void).
 * Efectos: Serializa la escritura y muestra consola solo si se solicita.
 */
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

/**
 * Crea el directorio de log cuando no existe.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Ninguno (void).
 */
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

/**
 * Comprueba el tamaño y solicita rotación al superar el límite.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Ninguno (void).
 */
void Logger::verificarTamañoArchivo() {
    std::string rutaArchivo = directorioLog + "/" + nombreArchivo;

    struct stat archivoInfo;
    if (stat(rutaArchivo.c_str(), &archivoInfo) == 0 && archivoInfo.st_size >= 0 &&
        static_cast<std::uintmax_t>(archivoInfo.st_size) > limiteBytes) {
        rotarArchivos();
    }
}

/**
 * Rota el archivo actual y elimina excedentes.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Ninguno (void).
 * Efectos: Renombra o elimina archivos de log.
 */
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

/**
 * Elimina los archivos rotados que exceden la conservación existente.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Ninguno (void).
 */
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

/**
 * Construye el nombre del archivo rotado.
 * Parámetros: int indice
 * Retorno: Nombre de archivo para el índice indicado.
 */
std::string Logger::obtenerNombreArchivoConIndice(int indice) {
    return "devlog_" + std::to_string(indice) + ".log";
}

/**
 * Formatea fecha y hora locales para el log.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Cadena de fecha/hora del host.
 */
std::string Logger::obtenerFechaHoraActual() {
    auto t = std::time(nullptr);
    std::tm tm;
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
