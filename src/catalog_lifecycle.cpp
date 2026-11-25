/*
 * Archivo: catalog_lifecycle.cpp
 * Descripción: Detecta cambios SQL y libera los catálogos de una generación.
 * Necesita: Tabla tbcambios; arreglos globales. Liberar solo después de unir los trabajadores.
 * Retornos: lee_tbcambios: 0 sin cambios, 1 con cambios, -1 ante error; liberarmemoria: void.
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
#include "include/s7_runtime_samples.h"

using namespace std;

extern cls_defalarmas *defalarmas;
extern cls_defeventos *defeventos;

/**
 * Compara los contadores SQL con la generación conocida.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 0 sin cambios; 1 con cambios; -1 ante fallo SQL.
 * Efectos: Actualiza el contador global de cambios.
 */
int lee_tbcambios(void) {

    int cont_tbregistros;
    int cont_tbdispositivos;
    int cont_tbdefequipos;
    int cont_cambio_tablacom = GLOBAL_cont_cambio_tablacom;
    int cont_tbdefalarmas;
    int cambio = 0;
    string direccionpuerto;
    direccionpuerto = "tcp://" + GLOBAL_db_direccion + ":" + GLOBAL_db_puerto;
    GLOBAL_contador_equipos = 0;

    try {
        std::shared_ptr<sql::Connection> con;
        std::unique_ptr<sql::Statement> stmt;
        std::unique_ptr<sql::ResultSet> res;

        /* Crea una conexion */
        con =
            sql_connection(direccionpuerto, GLOBAL_db_usuario, GLOBAL_db_clave,
                           GLOBAL_db_nombre); //GLOBAL_usuario_db.c_str(), GLOBAL_clave_db.c_str());
        /* Conecta con la base de datos */
        con->setSchema(GLOBAL_db_nombre.c_str());

        stmt.reset(con->createStatement());
        res.reset(stmt->executeQuery("SELECT * FROM `tbcambios` WHERE `id`=1"));

        while (res->next()) {
            cont_tbregistros = res->getInt("cont_tbregistros");
            cont_tbdispositivos = res->getInt("cont_tbdispositivos");
            cont_tbdefequipos = res->getInt("cont_tbdefequipos");
            cont_tbdefalarmas = res->getInt("cont_tbdefalarmas");
            cont_cambio_tablacom =
                cont_tbregistros + cont_tbdispositivos + cont_tbdefequipos + cont_tbdefalarmas;
        }

        if (GLOBAL_cont_cambio_tablacom == cont_cambio_tablacom) {
            cambio = 0;
        } else {
            cambio = 1;
            GLOBAL_cont_cambio_tablacom = cont_cambio_tablacom;
        }

        return cambio;

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
 * Libera catálogos y restablece sus punteros y contadores.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: Ninguno (void).
 * Efectos: Llamar únicamente después de unir los trabajadores.
 */
void liberarmemoria(void) {
    // Called only after all workers have joined (also handles partial startup).
    s7_runtime::Registry::instance().replace({});
    delete[] equipo_mbtcp;
    equipo_mbtcp = nullptr;
    delete[] equipo_mbserial;
    equipo_mbserial = nullptr;
    delete[] equipo_rs232;
    equipo_rs232 = nullptr;
    delete[] equipo_rs232_distintos;
    equipo_rs232_distintos = nullptr;

    delete[] defalarmas;
    defalarmas = nullptr;
    delete[] defeventos;
    defeventos = nullptr;
    delete[] cmd;
    cmd = nullptr;
    delete[] emaillist;
    emaillist = nullptr;
    delete[] smslist;
    smslist = nullptr;
    GLOBAL_contador_equipos_mbtcp = GLOBAL_contador_equipos_mbserial = 0;
    GLOBAL_contador_equipos_rs232 = GLOBAL_contador_equipos_rs232_distintos = 0;
    GLOBAL_contador_defalarmas = GLOBAL_contador_cmd = 0;
    GLOBAL_contador_defeventos = -1;
    GLOBAL_contador_directorio = GLOBAL_contador_numerossms = 0;
}
