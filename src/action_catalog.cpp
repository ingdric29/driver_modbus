/*
 * Archivo: action_catalog.cpp
 * Descripción: Carga definiciones de alarmas, correo, eventos y SMS.
 * Necesita: Conexión y esquema SQL; catálogos reemplazados entre generaciones.
 * Retornos: Cantidad cargada, 0 cuando la operación se omite o -1 ante error, según la función.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/repository_support.h"
#include "include/sql_session.h"
#include <cppconn/exception.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include "include/cls_defalarmas.h"
#include "include/cls_defeventos.h"

using namespace std;

extern cls_defalarmas *defalarmas;
extern cls_defeventos *defeventos;

using repository_detail::database_session;

/**
 * Carga el catálogo correspondiente desde SQL.
 * Parámetros: struct stdatabase *db
 * Retorno: Cantidad cargada, 0 si se omite la operación o -1 ante error.
 * Efectos: Actualiza el arreglo global y su contador; requiere coordinar su ciclo de vida.
 */
int actualizar_defalarmas(struct stdatabase *db) {

    if (db == &stdbexterna) {
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

    int ind = 0; // contador de comandos a ejecutar
    string activar = "";
    string desactivar = "";
    string direccionpuerto;
    string SQL_STR = "";
    string descripcion = "";
    string query1 = "";
    string query2 = "";
    //string query1 = "SELECT COUNT(`id`) as contador FROM `tbdefalarmas` WHERE habilitar = 1";
    query1 += "select count(`id`) as `contador` from(";
    query1 += "SELECT `tbdefalarmas`.`id` as `id`,";
    query1 += "`tbdefalarmas`.`habilitar` as `habilitar`,";
    query1 += "`tbdispositivos`.`habilitar` AS `habilitardispo`";
    query1 += "FROM";
    query1 += "(`tbdefalarmas` join `tbdispositivos` on";
    query1 +=
        "(`tbdefalarmas`.`dispositivo` = `tbdispositivos`.`nombre`)) order by `tbdefalarmas`.`id`";
    query1 += ") tabladefalarmas";
    query1 += " where `tabladefalarmas`.`habilitar`='1' and `tabladefalarmas`.`habilitardispo`='1'";

    //string query2 = "SELECT * FROM `tbdefalarmas` WHERE habilitar = 1";

    query2 += "SELECT `tbdefalarmas`.`id` as `id`,";
    query2 += "`tbdefalarmas`.`activar` as `activar`,";
    query2 += "`tbdefalarmas`.`desactivar` as `desactivar`,";
    query2 += "`tbdefalarmas`.`descripcion` as `descripcion`,";
    query2 += "`tbdefalarmas`.`accionactivar` as `accionactivar`,";
    query2 += "`tbdefalarmas`.`acciondesactivar` as `acciondesactivar`,";
    query2 += "`tbdefalarmas`.`dispositivo` as `dispositivo`,";
    query2 += "`tbdefalarmas`.`tag` as `tag`,";
    query2 += "`tbdefalarmas`.`habilitar` as `habilitar`,";
    query2 += "`tbdefalarmas`.`valorAlarma` as `valorAlarma`,";
    query2 += "`tbdispositivos`.`habilitar` AS `habilitardispo`";
    query2 += "FROM";
    query2 += "(`tbdefalarmas` join `tbdispositivos` on";
    query2 += "(`tbdefalarmas`.`dispositivo` = `tbdispositivos`.`nombre`))";
    query2 += " where `tbdefalarmas`.`habilitar` = '1' and `tbdispositivos`.`habilitar` = '1'";
    query2 += "order by `tbdefalarmas`.`id`";

    direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);

    try {
        std::shared_ptr<sql::Connection> con;
        std::unique_ptr<sql::Statement> stmt;
        std::unique_ptr<sql::ResultSet> res;

        /* Crea una conexion con la base de datos externa */
        con = database_session(db);
        /* Conecta con la base de datos */
        con->setSchema((db->nombre).c_str());
        stmt.reset(con->createStatement());
        res.reset(stmt->executeQuery(query1));

        while (res->next()) {
            GLOBAL_contador_defalarmas = res->getInt("contador");
        }
        imprimir_mensaje("cantidad de alarmas definidas: " +
                         cast_int_to_string(GLOBAL_contador_defalarmas));
        res.reset(stmt->executeQuery(query2));
        ind = 0;

        // se reserva memoria
        defalarmas = new cls_defalarmas[GLOBAL_contador_defalarmas + 1];

        while (res->next() && (ind < GLOBAL_contador_defalarmas)) {
            defalarmas[ind].id = res->getInt("id"); //
            defalarmas[ind].expresionactivar = res->getString("activar");
            defalarmas[ind].expresiondesactivar = res->getString("desactivar");
            defalarmas[ind].descripcion = res->getString("descripcion");
            defalarmas[ind].habilitar = res->getInt("habilitar") != 0;             //
            defalarmas[ind].accionactivar = res->getString("accionactivar");       //
            defalarmas[ind].acciondesactivar = res->getString("acciondesactivar"); //
            defalarmas[ind].clsequipo_mbtcp = &equipo_mbtcp[0];
            defalarmas[ind].clscantidad_equipo_mbtcp = GLOBAL_contador_equipos_mbtcp;
            defalarmas[ind].clsequipo_mbserial = &equipo_mbserial[0];
            defalarmas[ind].clscantidad_equipo_mbserial = GLOBAL_contador_equipos_mbserial;
            defalarmas[ind].clsequipo_rs232 = &equipo_rs232[0];
            defalarmas[ind].clscantidad_equipo_rs232 = GLOBAL_contador_equipos_rs232;

            defalarmas[ind].clsdispositivo = res->getString("dispositivo");
            defalarmas[ind].clstag = res->getString("tag");
            defalarmas[ind].clsvalorAlarma = res->getString("valorAlarma");

            ind++;
        }

        GLOBAL_contador_defalarmas = ind;

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
 * Carga el catálogo correspondiente desde SQL.
 * Parámetros: struct stdatabase *db
 * Retorno: Cantidad cargada, 0 si se omite la operación o -1 ante error.
 * Efectos: Actualiza el arreglo global y su contador; requiere coordinar su ciclo de vida.
 */
int actualizar_directorio(struct stdatabase *db) {

    if (db == &stdbexterna) {
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

    int ind = 0; // contador de comandos a ejecutar
    string direccionpuerto;
    string SQL_STR = "";
    string descripcion = "";

    string query1 = "SELECT COUNT(`id`) as contador FROM `tbdirectorio`";
    string query2 = "SELECT * FROM `tbdirectorio`";
    direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);

    try {
        std::shared_ptr<sql::Connection> con;
        std::unique_ptr<sql::Statement> stmt;
        std::unique_ptr<sql::ResultSet> res;

        /* Crea una conexion con la base de datos externa */
        con = database_session(db);
        /* Conecta con la base de datos */
        con->setSchema((db->nombre).c_str());
        stmt.reset(con->createStatement());
        res.reset(stmt->executeQuery(query1));

        while (res->next()) {
            GLOBAL_contador_directorio = res->getInt("contador");
        }
        res.reset(stmt->executeQuery(query2));
        ind = 0;

        // se reserva memoria
        emaillist = new cls_email[GLOBAL_contador_directorio + 1];

        while (res->next() && (ind < GLOBAL_contador_directorio)) {

            emaillist[ind].id = res->getInt("id"); //
            emaillist[ind].from = res->getString("emailfrom");
            emaillist[ind].to = res->getString("emailto");
            emaillist[ind].cc = res->getString("emailcc");
            emaillist[ind].asunto = res->getString("asunto");   //
            emaillist[ind].mensaje = res->getString("mensaje"); //

            ind++;
        }

        GLOBAL_contador_directorio = ind;

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
 * Carga el catálogo correspondiente desde SQL.
 * Parámetros: struct stdatabase *db
 * Retorno: Cantidad cargada, 0 si se omite la operación o -1 ante error.
 * Efectos: Actualiza el arreglo global y su contador; requiere coordinar su ciclo de vida.
 */
int actualizar_defeventos(struct stdatabase *db) {

    if (db == &stdbexterna) {
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

    int ind = 0; // contador de comandos a ejecutar
    string activar = "";
    string desactivar = "";
    string direccionpuerto;
    string SQL_STR = "";
    string descripcion = "";
    int LOCAL_contador_defeventos = 0;

    string query1 = "SELECT COUNT(`id`) as contador FROM `tbdefeventos` WHERE habilitar = 1";
    string query2 = "SELECT * FROM `tbdefeventos` WHERE habilitar = 1";
    direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);

    try {
        std::shared_ptr<sql::Connection> con;
        std::unique_ptr<sql::Statement> stmt;
        std::unique_ptr<sql::ResultSet> res;

        /* Crea una conexion con la base de datos externa */
        con = database_session(db);
        /* Conecta con la base de datos */
        con->setSchema((db->nombre).c_str());
        stmt.reset(con->createStatement());
        res.reset(stmt->executeQuery(query1));

        while (res->next()) {
            LOCAL_contador_defeventos = res->getInt("contador");
        }
        imprimir_mensaje("cantidad de eventos definidos: " +
                         cast_int_to_string(LOCAL_contador_defeventos));
        res.reset(stmt->executeQuery(query2));
        ind = 0;

        // se reserva memoria
        defeventos = new cls_defeventos[LOCAL_contador_defeventos + 1];

        while (res->next() && (ind < LOCAL_contador_defeventos)) {
            defeventos[ind].id = res->getInt("id"); //
            defeventos[ind].nombredispositivo = res->getString("nombredispositivo");
            defeventos[ind].tag = res->getString("tag");
            defeventos[ind].bit1 = res->getInt("bit1");
            defeventos[ind].activado = res->getString("activado");
            defeventos[ind].desactivado = res->getString("desactivado");

            ind++;
        }

        GLOBAL_contador_defeventos = ind;

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
 * Carga el catálogo correspondiente desde SQL.
 * Parámetros: struct stdatabase *db
 * Retorno: Cantidad cargada, 0 si se omite la operación o -1 ante error.
 * Efectos: Actualiza el arreglo global y su contador; requiere coordinar su ciclo de vida.
 */
int actualizar_smslist(struct stdatabase *db) {

    if (db == &stdbexterna) {
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

    int ind = 0; // contador de comandos a ejecutar
    string direccionpuerto;
    string SQL_STR = "";
    string descripcion = "";

    string query1 = "SELECT COUNT(`id`) as contador FROM `tbsms`";
    string query2 = "SELECT * FROM `tbsms`";
    direccionpuerto = "tcp://" + (db->direccion) + ":" + cast_int_to_string(db->puerto);

    try {
        std::shared_ptr<sql::Connection> con;
        std::unique_ptr<sql::Statement> stmt;
        std::unique_ptr<sql::ResultSet> res;

        /* Crea una conexion con la base de datos externa */
        con = database_session(db);
        /* Conecta con la base de datos */
        con->setSchema((db->nombre).c_str());
        stmt.reset(con->createStatement());
        res.reset(stmt->executeQuery(query1));

        while (res->next()) {
            GLOBAL_contador_numerossms = res->getInt("contador");
        }
        res.reset(stmt->executeQuery(query2));
        ind = 0;

        // se reserva memoria
        smslist = new cls_sms[GLOBAL_contador_numerossms + 1];

        while (res->next() && (ind < GLOBAL_contador_numerossms)) {

            smslist[ind].clsid = res->getInt("id"); //
            smslist[ind].clsnumero = res->getString("numero");
            smslist[ind].clsasunto = res->getString("asunto");
            smslist[ind].clsurl_modem = res->getString("url_modem");
            smslist[ind].clsparametros_rest_api = res->getString("parametros_rest_api");
            smslist[ind].clsmetodohtml = res->getString("metodohtml"); //

            ind++;
        }

        GLOBAL_contador_numerossms = ind;

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
