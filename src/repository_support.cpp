// Automonitor - separated from loadconfig.cpp; public function contracts unchanged.
// Original implementation: Alejandro Diomede / Automatroni.
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

bool is_external_target(const std::string &host, int port, const std::string &schema) {
    const bool external = host == GLOBAL_dbext_direccion &&
                          port == cast_string_to_int(GLOBAL_dbext_puerto) &&
                          schema == GLOBAL_dbext_nombre;
    const bool local = host == GLOBAL_db_direccion &&
                       port == cast_string_to_int(GLOBAL_db_puerto) && schema == GLOBAL_db_nombre;
    return external && !local;
}

bool database_allowed(const stdatabase *db) {
    return db != &stdbexterna || (GLOBAL_habilitar_acceso_externo && GLOBAL_ACCESO_EXTERNO);
}

std::shared_ptr<sql::Connection> database_session(const stdatabase *db) {
    return sql_connection("tcp://" + db->direccion + ":" + std::to_string(db->puerto), db->usuario,
                          db->clave, db->nombre);
}

void database_failure(const std::exception &error) {
    ++DriverMetrics::instance().sql_failures;
    sql_sessions().invalidate();
    logger_loadconfig.registrarError(std::string("SQL operation failed: ") + error.what());
}
} // namespace repository_detail
