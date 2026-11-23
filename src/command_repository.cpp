/*
 * Archivo: command_repository.cpp
 * Descripción: Consulta solicitudes, carga comandos y confirma su fecha en SQL.
 * Necesita: Perfil SQL, concentrador configurado, tbejecutar y tbcmd.
 * Retornos: Estado de consulta, cantidad cargada o resultado de actualización; ver cada función.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/repository_support.h"
#include "include/sql_session.h"
#include "include/command_repository.h"
#include "include/command_integer.h"
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <algorithm>
#include <stdexcept>

using namespace std;

using repository_detail::database_failure;
using repository_detail::is_external_target;

/**
 * Consulta la última solicitud del concentrador y consume su indicador.
 * Parámetros: int &ejecutar, time_t &segundos, string &lote, string host, int port, string
 * schema, string user, string password
 * Retorno: 0 si completa u omite la consulta; -1 ante error SQL.
 * Efectos: Escribe ejecutar, segundos y lote; consumir la solicitud no confirma la escritura
 * Modbus. request_id opcional identifica la fila observada, no una ejecución única.
 */
int leer_tbejecutar(int &ejecutar, time_t &segundos, string &lote, string host, int port,
                    string schema, string user, string password, int *request_id) {
    ejecutar = 0;
    if (request_id)
        *request_id = 0;
    if (is_external_target(host, port, schema) &&
        (!GLOBAL_habilitar_acceso_externo || !GLOBAL_ACCESO_EXTERNO))
        return 0;
    try {
        auto connection =
            sql_connection("tcp://" + host + ":" + std::to_string(port), user, password, schema);
        std::unique_ptr<sql::PreparedStatement> select(connection->prepareStatement(
            "SELECT * FROM tbejecutar WHERE concentrador=? ORDER BY id DESC LIMIT 1"));
        select->setString(1, GLOBAL_nombre_concentrador);
        std::unique_ptr<sql::ResultSet> row(select->executeQuery());
        if (!row->next())
            return 0;
        if (request_id)
            *request_id = row->getInt("id");
        const int requested = row->getInt("ejecutar_tbcmd");
        segundos = cast_string_to_double(row->getString("tiempo_segundos"));
        lote = row->getString("lote");
        if (requested) {
            std::unique_ptr<sql::PreparedStatement> update(connection->prepareStatement(
                "UPDATE tbejecutar SET ejecutar_tbcmd=0,tiempo_tbcmd=?,tiempo_segundos=? WHERE "
                "id=?"));
            const auto now = time(nullptr);
            update->setString(1, cast_time_to_datetime(now));
            update->setInt64(2, now);
            update->setInt(3, row->getInt("id"));
            update->executeUpdate();
            ejecutar = requested;
        }
        return 0;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}

namespace {
int integer_field(sql::ResultSet &row, const char *field, cls_cmd &command) {
    int value = 0;
    if (row.isNull(field) || !command_integer(row.getString(field), value)) {
        if (command.configuration_error.empty())
            command.configuration_error = string(field) + ": nulo o entero invalido";
    }
    return value;
}

// One prepared query, no COUNT and no mutation of the alarm catalog.
std::vector<cls_cmd> load_commands(const string &host, int port, const string &schema,
                                   const string &user, const string &password,
                                   const string *batch) {
    auto connection =
        sql_connection("tcp://" + host + ":" + std::to_string(port), user, password, schema);
    string query = "SELECT c.id,d.nombre,d.direccion_ip,d.puerto,d.direccion_esclavo,d.interface,"
                   "d.velocidad,d.paridad,d.bitsparada,d.modelo AS equipo,d.protocolo,d.habilitar,"
                   "c.registro,c.valor,c.cmd_datetime,c.hab_ejecutar,c.lote "
                   "FROM tbcmd c JOIN tbdispositivos d ON d.nombre=c.nombredispositivo "
                   "AND d.concentrador=c.concentrador WHERE c.concentrador=?";
    if (batch)
        // Keep an indexable prefilter, NULL/empty legacy equivalence and exact matching
        // after converting Latin-1 catalogs and parameters to the same character set.
        query += " AND (c.lote=? OR (c.lote IS NULL AND ?=''))"
                 " AND BINARY CONVERT(COALESCE(c.lote,'') USING utf8mb4)="
                 "BINARY CONVERT(? USING utf8mb4) AND c.hab_ejecutar<>0";
    query += " ORDER BY c.id ASC,d.id ASC LIMIT " +
             std::to_string(command_repository::maximum_commands + 1);
    std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement(query));
    statement->setString(1, GLOBAL_nombre_concentrador);
    if (batch) {
        statement->setString(2, *batch);
        statement->setString(3, *batch);
        statement->setString(4, *batch);
    }
    std::unique_ptr<sql::ResultSet> rows(statement->executeQuery());
    std::vector<cls_cmd> commands;
    while (rows->next()) {
        if (commands.size() == command_repository::maximum_commands)
            throw std::runtime_error("Command catalog/batch exceeds limit of " +
                                     std::to_string(command_repository::maximum_commands));
        cls_cmd value{};
        value.id = rows->getInt("id");
        value.nombre = rows->getString("nombre");
        value.direccion_ip = rows->getString("direccion_ip");
        value.interface1 = rows->getString("interface");
        value.paridad = rows->getString("paridad");
        value.equipo = rows->getString("equipo");
        value.protocolo = rows->getString("protocolo");
        value.habilitar = rows->getInt("habilitar") != 0;
        value.registro = integer_field(*rows, "registro", value);
        value.clsvalor = integer_field(*rows, "valor", value);
        if (value.protocolo == "MBTCP" || value.protocolo == "MBSERIAL")
            value.direccion_esclavo = integer_field(*rows, "direccion_esclavo", value);
        if (value.protocolo == "MBTCP")
            value.puerto = integer_field(*rows, "puerto", value);
        else if (value.protocolo == "MBSERIAL") {
            value.velocidad = integer_field(*rows, "velocidad", value);
            value.bitsparada = integer_field(*rows, "bitsparada", value);
        }
        value.datetime = rows->getString("cmd_datetime");
        value.hab_ejecutar = rows->getInt("hab_ejecutar") != 0;
        value.lote = rows->getString("lote");
        commands.push_back(value);
    }
    return commands;
}

bool commands_allowed(const string &host, int port, const string &schema) {
    return !is_external_target(host, port, schema) ||
           (GLOBAL_habilitar_acceso_externo && GLOBAL_ACCESO_EXTERNO);
}
} // namespace

/**
 * Carga el catálogo local completo para acciones CMD de alarmas de la generación.
 * Retorno: cantidad, 0 omitido o -1 error; conserva el catálogo anterior ante fallo.
 * Efectos: Sustituye el arreglo solo tras cargar todas las filas. No usar para lotes.
 */
int leer_tbcmd(string host, int port, string schema, string user, string password) {
    if (!commands_allowed(host, port, schema))
        return 0;
    try {
        const auto loaded = load_commands(host, port, schema, user, password, nullptr);
        std::unique_ptr<cls_cmd[]> pending(new cls_cmd[loaded.size()]());
        std::copy(loaded.begin(), loaded.end(), pending.get());
        std::lock_guard<std::recursive_mutex> lock(GLOBAL_command_mutex);
        delete[] cmd;
        cmd = pending.release();
        GLOBAL_contador_cmd = static_cast<int>(loaded.size());
        return GLOBAL_contador_cmd;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}

/**
 * Selecciona solo los comandos habilitados del lote en orden de ID.
 * Retorno: cantidad, 0 vacío/omitido o -1 error; deja salida vacía ante fallo.
 * Efectos: Lista privada independiente del catálogo local de alarmas.
 */
int command_repository::load_batch(const string &host, int port, const string &schema,
                                   const string &user, const string &password, const string &batch,
                                   std::vector<cls_cmd> &commands) {
    commands.clear();
    if (!commands_allowed(host, port, schema))
        return 0;
    try {
        commands = load_commands(host, port, schema, user, password, &batch);
        return static_cast<int>(commands.size());
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}

/**
 * Actualiza la fecha SQL del comando aceptado.
 * Parámetros: int id, string direccion_db, int puerto_db, string nombre_db, string usuario_db,
 * string clave_db
 * Retorno: 0 al completar u omitir la operación; -1 ante fallo.
 */
int escribir_tbcmd_fechahora(int id, string direccion_db, int puerto_db, string nombre_db,
                             string usuario_db, string clave_db) {
    if (is_external_target(direccion_db, puerto_db, nombre_db) &&
        (!GLOBAL_habilitar_acceso_externo || !GLOBAL_ACCESO_EXTERNO))
        return -1;
    try {
        auto connection = sql_connection("tcp://" + direccion_db + ":" + std::to_string(puerto_db),
                                         usuario_db, clave_db, nombre_db);
        std::unique_ptr<sql::PreparedStatement> statement(
            connection->prepareStatement("UPDATE tbcmd SET cmd_datetime=? WHERE id=?"));
        statement->setString(1, cast_time_to_datetime(time(nullptr)));
        statement->setInt(2, id);
        statement->executeUpdate();
        return 0;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}
