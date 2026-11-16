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
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

using namespace std;

using repository_detail::database_failure;
using repository_detail::is_external_target;

/**
 * Consulta la última solicitud del concentrador y consume su indicador.
 * Parámetros: int &ejecutar, time_t &segundos, string &lote, string host, int port, string
 * schema, string user, string password
 * Retorno: 0 si completa u omite la consulta; -1 ante error SQL.
 * Efectos: Escribe ejecutar, segundos y lote; consumir la solicitud no confirma la escritura
 * Modbus.
 */
int leer_tbejecutar(int &ejecutar, time_t &segundos, string &lote, string host, int port,
                    string schema, string user, string password) {
    ejecutar = 0;
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

/**
 * Carga el catálogo correspondiente desde SQL.
 * Parámetros: string direccion_db, int puerto_db, string nombre_db, string usuario_db, string
 * clave_db
 * Retorno: Cantidad cargada, 0 si se omite la operación o -1 ante error.
 * Efectos: Actualiza el arreglo global y su contador; requiere coordinar su ciclo de vida.
 */
int leer_tbcmd(string direccion_db, int puerto_db, string nombre_db, string usuario_db,
               string clave_db) {
    std::lock_guard<std::recursive_mutex> commands(GLOBAL_command_mutex);

    if (is_external_target(direccion_db, puerto_db, nombre_db)) {

        if (GLOBAL_habilitar_acceso_externo == 0) {
            imprimir_mensaje("en la configuracion configdb esta desactivado el acceso a internet",
                             negro);
            return 0;
        }

        if (GLOBAL_ACCESO_EXTERNO == false) {
            imprimir_mensaje("se desactivo momentaneamente el acceso al servidor mysql externo",
                             negro);
            return 0;
        }
    }

    int pending_count = 0;
    int ind = 0; // contador de comandos a ejecutar
    string cadena;
    string direccionpuerto;
    direccionpuerto = "tcp://" + direccion_db + ":" + cast_int_to_string(puerto_db);

    string cadenaquerycmd = "select `tbcmd`.`id` AS `id`,";
    cadenaquerycmd += "`tbdispositivos`.`nombre` AS `nombre`,";
    cadenaquerycmd += "`tbdispositivos`.`direccion_ip` AS `direccion_ip`,";
    cadenaquerycmd += "`tbdispositivos`.`puerto` AS `puerto`,";
    cadenaquerycmd += "`tbdispositivos`.`direccion_esclavo` AS `direccion_esclavo`,";
    cadenaquerycmd += "`tbdispositivos`.`interface` AS `interface`,";
    cadenaquerycmd += "`tbdispositivos`.`velocidad` AS `velocidad`,";
    cadenaquerycmd += "`tbdispositivos`.`paridad` AS `paridad`,";
    cadenaquerycmd += "`tbdispositivos`.`bitsparada` AS `bitsparada`,";
    cadenaquerycmd += "`tbdispositivos`.`modelo` AS `equipo`,";
    cadenaquerycmd += "`tbdispositivos`.`usuario` AS `usuario`,";
    cadenaquerycmd += "`tbdispositivos`.`password` AS `password`,";
    cadenaquerycmd += "`tbdispositivos`.`protocolo` AS `protocolo`,";
    cadenaquerycmd += "`tbdispositivos`.`habilitar` AS `habilitar`,";
    cadenaquerycmd += "`tbcmd`.`registro` AS `registro`,";
    cadenaquerycmd += "`tbcmd`.`valor` AS `valor`,";
    cadenaquerycmd += "`tbcmd`.`cmd_datetime` AS `datetime`,";
    cadenaquerycmd += "`tbcmd`.`hab_ejecutar` AS `hab_ejecutar`,";
    cadenaquerycmd += "`tbcmd`.`concentrador` AS `concentrador`, ";
    cadenaquerycmd += "`tbcmd`.`lote` AS `lote` ";
    cadenaquerycmd += "from (`tbdispositivos` join `tbcmd` ";
    cadenaquerycmd += "on((`tbdispositivos`.`nombre` = `tbcmd`.`nombredispositivo`) and";
    cadenaquerycmd += "(`tbdispositivos`.`concentrador` = `tbcmd`.`concentrador`) )) ";
    cadenaquerycmd += "order by `tbdispositivos`.`id`";

    try {
        std::shared_ptr<sql::Connection> con;
        std::unique_ptr<sql::PreparedStatement> stmt;
        std::unique_ptr<sql::ResultSet> res;

        /* Crea una conexion */
        con = sql_connection(direccionpuerto, usuario_db, clave_db, nombre_db);
        /* Conecta con la base de datos */
        con->setSchema(nombre_db.c_str());

        stmt.reset(con->prepareStatement("SELECT COUNT(id) AS contador FROM (" + cadenaquerycmd +
                                         ") AS vistacmd WHERE concentrador=?"));
        stmt->setString(1, GLOBAL_nombre_concentrador);
        res.reset(stmt->executeQuery());
        while (res->next()) {
            pending_count = res->getInt("contador");
        }

        std::unique_ptr<cls_cmd[]> pending(new cls_cmd[pending_count + 1]());
        res.reset();
        stmt.reset(con->prepareStatement("SELECT * FROM (" + cadenaquerycmd +
                                         ") AS vistacmd WHERE concentrador=? ORDER BY id ASC"));
        stmt->setString(1, GLOBAL_nombre_concentrador);
        res.reset(stmt->executeQuery());
        while (res->next() && (ind < 128)) {
            pending[ind].id = res->getInt("id");
            pending[ind].nombre = res->getString("nombre");                    //
            pending[ind].direccion_ip = res->getString("direccion_ip");        //
            pending[ind].puerto = res->getInt("puerto");                       //
            pending[ind].direccion_esclavo = res->getInt("direccion_esclavo"); //
            pending[ind].interface1 = res->getString("interface");             //
            pending[ind].velocidad = res->getInt("velocidad");                 //
            pending[ind].paridad = res->getString("paridad");                  //
            pending[ind].bitsparada = res->getInt("bitsparada");               //
            pending[ind].equipo = res->getString("equipo");                    //

            pending[ind].usuario = res->getString("usuario");                 //
            pending[ind].password = res->getString("password");               //
            pending[ind].habilitar = ((res->getInt("habilitar")) != 0);       //
            pending[ind].protocolo = res->getString("protocolo");             //
            pending[ind].registro = res->getInt("registro");                  //
            pending[ind].clsvalor = res->getInt("valor");                     //
            pending[ind].datetime = res->getString("datetime");               //
            pending[ind].hab_ejecutar = ((res->getInt("hab_ejecutar")) != 0); //
            pending[ind].lote = res->getString("lote");                       //
            ind++;
        }

        delete[] cmd;
        cmd = pending.release();
        GLOBAL_contador_cmd = ind;
        return (ind);

    } catch (sql::SQLException &e) {
        sql_sessions().invalidate();
        GLOBAL_ACCESO_EXTERNO = false;
        imprimir_mensaje("falla en mysql: ", rojo);
        imprimir_mensaje("se deshabilito lectura mysql externo por " +
                             cast_int_to_string(GLOBAL_periodo_reseteo_mysqlexterno) + " seg ",
                         amarillo);
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
