/*
 * Archivo: repository_support.cpp
 * Descripción: Declara contratos y servicios compartidos entre módulos.
 * Necesita: Definiciones únicas del estado y las implementaciones correspondientes.
 * Retornos: La cabecera no retorna valores por sí sola; los métodos conservan sus contratos
 * documentados en implementación.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/repository_support.h"
#include "include/sql_session.h"
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

using namespace std;

Logger logger_loadconfig;

namespace repository_detail {

/**
 * Distingue el perfil externo del local por host, puerto y esquema.
 * Parámetros: const std::string &host, int port, const std::string &schema
 * Retorno: true solo si corresponde al externo y no al local.
 */
bool is_external_target(const std::string &host, int port, const std::string &schema) {
    const bool external = host == GLOBAL_dbext_direccion &&
                          port == cast_string_to_int(GLOBAL_dbext_puerto) &&
                          schema == GLOBAL_dbext_nombre;
    const bool local = host == GLOBAL_db_direccion &&
                       port == cast_string_to_int(GLOBAL_db_puerto) && schema == GLOBAL_db_nombre;
    return external && !local;
}

/**
 * Comprueba si se permite operar sobre el perfil indicado.
 * Parámetros: const stdatabase *db
 * Retorno: false si es el perfil externo deshabilitado; true en otro caso.
 */
bool database_allowed(const stdatabase *db) {
    return db != &stdbexterna || (GLOBAL_habilitar_acceso_externo && GLOBAL_ACCESO_EXTERNO);
}

/**
 * Obtiene una sesión del pool por hilo para el perfil indicado.
 * Parámetros: const stdatabase *db
 * Retorno: Conexión compartida; puede propagar excepciones de conexión/esquema.
 */
std::shared_ptr<sql::Connection> database_session(const stdatabase *db) {
    return sql_connection("tcp://" + db->direccion + ":" + std::to_string(db->puerto), db->usuario,
                          db->clave, db->nombre);
}

/**
 * Registra un fallo SQL e invalida las sesiones del hilo.
 * Parámetros: const std::exception &error
 * Retorno: Ninguno (void).
 * Efectos: Incrementa el contador de fallos y añade un evento de log.
 */
void database_failure(const std::exception &error) {
    ++DriverMetrics::instance().sql_failures;
    sql_sessions().invalidate();
    logger_loadconfig.registrarError(std::string("SQL operation failed: ") + error.what());
}
} // namespace repository_detail
