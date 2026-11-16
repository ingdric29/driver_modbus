/*
 * Archivo: replication.cpp
 * Descripción: Sincroniza históricos y replica registros entre bases.
 * Necesita: Perfiles SQL, tablas compatibles, límites de lote y checkpoint cuando corresponda.
 * Retornos: Cantidad copiada/cargada, 0 si no se copia o -1 ante error.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/repository_support.h"
#include "include/sql_session.h"
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include "include/replication_cursor.h"

using namespace std;

using repository_detail::database_failure;
using repository_detail::database_session;

/**
 * Copia un lote histórico conservando el avance de replicación.
 * Parámetros: stdatabase *local, stdatabase *remote, string table, int limit
 * Retorno: Cantidad copiada, 0 si no hay copia o -1 ante error.
 */
int sincronizar_tablas(stdatabase *local, stdatabase *remote, string table, int limit) {
    if (!GLOBAL_habilitar_acceso_externo || !GLOBAL_ACCESO_EXTERNO)
        return 0;
    if (limit <= 0 || limit > 10000)
        return -1;
    if (local->direccion == remote->direccion && local->puerto == remote->puerto &&
        local->nombre == remote->nombre)
        return -1;
    try {
        const auto identity = local->direccion + ":" + std::to_string(local->puerto) + "/" +
                              local->nombre + "->" + remote->direccion + ":" +
                              std::to_string(remote->puerto) + "/" + remote->nombre + "/" + table +
                              "/" + GLOBAL_nombre_concentrador;
        const auto path = obtenerDirectorio("log") + "/" + replication_cursor_name(identity);
        uint64_t cursor = 0;
        auto destination = database_session(remote);
        if (!read_replication_cursor(path, identity, cursor)) {
            if (GLOBAL_replicacion_inicial_id >= 0)
                cursor = GLOBAL_replicacion_inicial_id;
            else {
                std::unique_ptr<sql::PreparedStatement> check(destination->prepareStatement(
                    "SELECT 1 FROM " + sql_identifier(table) + " WHERE concentrador=? LIMIT 1"));
                check->setString(1, GLOBAL_nombre_concentrador);
                std::unique_ptr<sql::ResultSet> existing(check->executeQuery());
                if (existing->next())
                    throw std::runtime_error("replication destination already has data; configure "
                                             "replicacion_inicial_id or restore checkpoint");
            }
        }
        auto source = database_session(local);
        std::unique_ptr<sql::PreparedStatement> select(
            source->prepareStatement("SELECT * FROM " + sql_identifier(table) +
                                     " WHERE concentrador=? AND id>? ORDER BY id ASC LIMIT ?"));
        select->setString(1, GLOBAL_nombre_concentrador);
        select->setUInt64(2, cursor);
        select->setInt(3, limit);
        std::unique_ptr<sql::ResultSet> rows(select->executeQuery());
        SqlTransaction transaction(destination);
        std::unique_ptr<sql::PreparedStatement> insert(destination->prepareStatement(
            "INSERT INTO " + sql_identifier(table) +
            " (timedate,fechatiempo,tag,valorstr,valorfloat,dispositivo,rastro,concentrador,"
            "nombredispositivo) VALUES (?,?,?,?,?,?,?,?,?)"));
        int copied = 0;
        uint64_t confirmed = cursor;
        while (rows->next()) {
            insert->setString(1, rows->getString("timedate"));
            insert->setString(2, rows->getString("fechatiempo"));
            insert->setString(3, rows->getString("tag"));
            if (rows->isNull("valorstr"))
                insert->setNull(4, sql::DataType::VARCHAR);
            else
                insert->setString(4, rows->getString("valorstr"));
            if (rows->isNull("valorfloat"))
                insert->setNull(5, sql::DataType::DOUBLE);
            else
                insert->setDouble(5, rows->getDouble("valorfloat"));
            insert->setInt(6, rows->getInt("dispositivo"));
            insert->setString(7, rows->getString("rastro"));
            insert->setString(8, rows->getString("concentrador"));
            insert->setString(9, rows->getString("nombredispositivo"));
            if (insert->executeUpdate() != 1)
                throw std::runtime_error("replication insert was not acknowledged");
            confirmed = rows->getUInt64("id");
            ++copied;
        }
        transaction.commit();
        if (copied)
            write_replication_cursor(path, identity, confirmed);
        DriverMetrics::instance().replicated_samples += copied;
        return copied;
    } catch (const std::exception &error) {
        database_failure(error);
        return -1;
    }
}

/**
 * Copia los campos seleccionados entre tablas.
 * Parámetros: struct stdatabase *dborigen, string tablaorigen, struct stdatabase *dbdestino,
 * string tabladestino, string *campos, int cantidadcampos, int limitefilastablaorigen
 * Retorno: Cantidad procesada o -1 ante error SQL.
 */
int replicaciontablaselectiva(struct stdatabase *dborigen, string tablaorigen,
                              struct stdatabase *dbdestino, string tabladestino, string *campos,
                              int cantidadcampos, int limitefilastablaorigen)

{

    int ind = 0; // contador de comandos a ejecutar
    string cadena;
    string direccionpuerto_origen;
    direccionpuerto_origen =
        "tcp://" + dborigen->direccion + ":" + cast_int_to_string(dborigen->puerto);
    string cadenasql_origen = "SELECT ";
    try {
        std::shared_ptr<sql::Connection> con;
        std::unique_ptr<sql::Statement> stmt;
        std::unique_ptr<sql::ResultSet> res;

        for (int j = 0; j < cantidadcampos; j++) {
            if (j == cantidadcampos - 1) {
                cadenasql_origen += "`" + campos[j] + "`";
            } else {
                cadenasql_origen += "`" + campos[j] + "`,";
            }
        }

        cadenasql_origen +=
            " FROM `" + tablaorigen + "` LIMIT " + cast_int_to_string(limitefilastablaorigen);

        /* Crea una conexion */
        con = database_session(dborigen);
        int timeout1 = 3;
        con->setClientOption("MYSQL_OPT_READ_TIMEOUT", &timeout1);
        std::vector<string> valorescampos(cantidadcampos);

        /* Conecta con la base de datos */
        con->setSchema((dborigen->nombre).c_str());

        stmt.reset(con->createStatement());

        res.reset(stmt->executeQuery(cadenasql_origen.c_str()));

        while (res->next() && (ind < limitefilastablaorigen)) {

            for (int i = 0; i < cantidadcampos; i++) {
                valorescampos[i] = res->getString(i + 1); //
            }

            insertar_inteligente(dbdestino, tabladestino, 0, &campos[0], &valorescampos[0],
                                 cantidadcampos);

            ind++;
        }

        return (ind);

    } catch (sql::SQLException &e) {
        sql_sessions().invalidate();
        std::string mensajeError = "ERROR: SQLException in " + std::string(__FILE__) + "(" +
                                   std::string(__FUNCTION__) + ") on line " +
                                   std::to_string(__LINE__) + ".# Error description: " + e.what() +
                                   " (MySQL error code: " + std::to_string(e.getErrorCode()) +
                                   ", SQLState: " + e.getSQLState() + " )";

        std::cout << mensajeError << std::endl;
        logger_loadconfig.registrarError(mensajeError);
        return -1;
    }
}
