/*
 * Archivo: repository_support.h
 * Descripción: Declara contratos y servicios compartidos entre módulos.
 * Necesita: Definiciones únicas del estado y las implementaciones correspondientes.
 * Retornos: La cabecera no retorna valores por sí sola; los métodos conservan sus contratos
 * documentados en implementación.
 */

#ifndef AUTOMONITOR_REPOSITORY_SUPPORT_H
#define AUTOMONITOR_REPOSITORY_SUPPORT_H

#include "cls_logger.h"
#include <exception>
#include <memory>
#include <string>

struct stdatabase;
namespace sql {
class Connection;
}

// One shared logger, as before the split. These helpers are internal to the
// repository modules; public driver contracts remain declared in globales.h.
extern Logger logger_loadconfig;
namespace repository_detail {
bool is_external_target(const std::string &host, int port, const std::string &schema);
bool database_allowed(const stdatabase *db);
std::shared_ptr<sql::Connection> database_session(const stdatabase *db);
void database_failure(const std::exception &error);
} // namespace repository_detail
#endif
