/*
 * Archivo: cls_logger.h
 * Descripción: Registra eventos, escribe logs y rota archivos por tamaño.
 * Necesita: Directorio padre del trabajo, permisos de escritura y límite de tamaño.
 * Retornos: Métodos de escritura: void; helpers: ruta o fecha. Notifica fallos de archivo en
 * consola.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <string>

std::string obtenerDirectorio(std::string nombreCarpeta);

class Logger {
  public:
    // Constructor con límite de tamaño predeterminado (1 MB)
    Logger(size_t limiteBytes = 1024 * 1024);
    void registrarError(const std::string &mensajeError, bool mostrarConsola = true);

  private:
    size_t limiteBytes;        // Límite de tamaño del archivo de log
    std::string nombreArchivo; // Nombre del archivo de log principal
    std::string directorioLog; // Directorio para almacenar los archivos de log

    void crearDirectorioLog();      // Crea el directorio "log" si no existe
    void verificarTamañoArchivo();  // Verifica el tamaño del archivo y rota si es necesario
    void rotarArchivos();           // Realiza la rotación de los archivos de log
    void limpiarArchivosAntiguos(); // Elimina archivos antiguos si se alcanzó el límite
    std::string obtenerNombreArchivoConIndice(int indice);
    std::string obtenerFechaHoraActual();
};

#endif // LOGGER_H
