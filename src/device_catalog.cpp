/*
 * Archivo: device_catalog.cpp
 * Descripción: Carga equipos y asocia su estado compartido por dispositivo.
 * Necesita: Perfil SQL configurado, qrytablacom y catálogos globales sin trabajadores activos.
 * Retornos: 1 si termina la carga; -1 ante error. Actualiza arreglos y contadores globales.
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

/**
 * Carga los equipos del protocolo y asocia estado compartido.
 * Parámetros: GLOBAL_enum_protocolo protocoloenum
 * Retorno: 1 si finaliza; -1 ante fallo.
 * Efectos: Reemplaza el catálogo con los trabajadores detenidos.
 */
int actualizar_leer_equipo(GLOBAL_enum_protocolo protocoloenum) {

    int id = 0;
    int ind = 0;
    int cuenta = 0;
    int i2 = 0;
    string cadena;
    string direccionpuerto;
    direccionpuerto = "tcp://" + GLOBAL_db_direccion + ":" + GLOBAL_db_puerto;
    GLOBAL_contador_equipos = 0;
    string query;

    switch (protocoloenum) {
    case MBTCP:
        query = "SELECT * FROM `qrytablacom` WHERE `habilitar`=TRUE AND `protocolo`= 'MBTCP' ORDER "
                "BY `id` ASC";
        break;
    case MBSERIAL:
        query = "SELECT * FROM `qrytablacom` WHERE `habilitar`=TRUE AND `protocolo`= 'MBSERIAL' "
                "ORDER BY `id` ASC";
        break;
    case RS232:
        query = "SELECT * FROM `qrytablacom` WHERE `habilitar`=TRUE AND `protocolo`= 'RS232' ORDER "
                "BY `id` ASC";
        break;
    }

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
        res.reset(stmt->executeQuery(query.c_str()));

        while (res->next() && (ind < 512)) {
            cuenta = cuenta + 1;
            ind++;
        }

        switch (protocoloenum) {
        case MBTCP:
            GLOBAL_contador_equipos_mbtcp = cuenta;
            break;
        case MBSERIAL:
            GLOBAL_contador_equipos_mbserial = cuenta;
            break;
        case RS232:
            GLOBAL_contador_equipos_rs232 = cuenta;
            GLOBAL_contador_equipos_rs232_distintos = 0; //mas adelante se completa
            break;
        }

        /* asignacion de memoria RAM*/

        switch (protocoloenum) {
        case MBTCP:
            equipo_mbtcp = new cls_equipos[GLOBAL_contador_equipos_mbtcp + 2];
            break;
        case MBSERIAL:
            equipo_mbserial = new cls_equipos[GLOBAL_contador_equipos_mbserial + 2];
            break;
        case RS232:
            equipo_rs232 = new cls_equipos[GLOBAL_contador_equipos_rs232 + 2];
            equipo_rs232_distintos = new cls_equipos[GLOBAL_contador_equipos_rs232 + 2];
            break;
        }

        res.reset(stmt->executeQuery(query.c_str()));

        switch (protocoloenum) {

        case MBTCP:
            ind = 0;
            equipo_mbtcp[0].protocolo = "MBTCP";
            while (res->next() && (ind < 512)) {
                id = res->getInt("id"); //
                equipo_mbtcp[ind].id = id;
                equipo_mbtcp[ind].nombre = res->getString("nombre");                    //
                equipo_mbtcp[ind].direccion_ip = res->getString("direccion_ip");        //
                equipo_mbtcp[ind].puerto = res->getInt("puerto");                       //
                equipo_mbtcp[ind].direccion_esclavo = res->getInt("direccion_esclavo"); //
                equipo_mbtcp[ind].interface1 = res->getString("interface");             //
                equipo_mbtcp[ind].velocidad = res->getInt("velocidad");                 //
                equipo_mbtcp[ind].paridad = res->getString("paridad");                  //
                equipo_mbtcp[ind].bitsparada = res->getInt("bitsparada");               //
                equipo_mbtcp[ind].equipo = res->getInt("equipo");                       //
                equipo_mbtcp[ind].usuario = res->getString("usuario");                  //
                equipo_mbtcp[ind].password = res->getString("password");                //
                equipo_mbtcp[ind].habilitar = ((res->getInt("habilitar")) != 0);        //
                equipo_mbtcp[ind].tag = res->getString("tag");
                equipo_mbtcp[ind].preservar_picos =
                    ("," + GLOBAL_picos_tags + ",")
                        .find("," + equipo_mbtcp[ind].nombre + "/" + equipo_mbtcp[ind].tag + ",") !=
                    std::string::npos;                                               //
                equipo_mbtcp[ind].protocolo = res->getString("protocolo");           //
                equipo_mbtcp[ind].registro = res->getInt("registro");                //
                equipo_mbtcp[ind].funcion = res->getString("funcion");               //
                equipo_mbtcp[ind].interpretacion = res->getString("interpretacion"); //
                cadena = res->getString("escalamiento");                             //
                equipo_mbtcp[ind].escalamiento = getValfloat(cadena);

                equipo_mbtcp[ind].periodo = cast_string_to_double(res->getString("periodo"));
                try {
                    PeriodicDeadline::interval(equipo_mbtcp[ind].periodo);
                } catch (const std::invalid_argument &e) {
                    logger_loadconfig.registrarError(std::string("Invalid tag period: ") +
                                                     e.what());
                    return -1;
                } //
                equipo_mbtcp[ind].bit1 = res->getInt("bit1");                //
                equipo_mbtcp[ind].cantidad1 = res->getInt("cantidad1");      //
                equipo_mbtcp[ind].comentario = res->getString("comentario"); //
                equipo_mbtcp[ind].unidad = res->getString("unidad");         //
                equipo_mbtcp[ind].solocambios = res->getInt("solocambios");  //
                equipo_mbtcp[ind].registrable = res->getInt("registrable");  //

                ind++;
            }
            break;

        case MBSERIAL:
            ind = 0;
            equipo_mbserial[0].protocolo = "MBSERIAL";
            while (res->next() && (ind < 512)) {
                id = res->getInt("id"); //
                equipo_mbserial[ind].id = id;
                cadena = res->getString("nombre");
                equipo_mbserial[ind].nombre = cadena; //
                //equipo_mbserial[ind].direccion_ip = res->getString("direccion_ip"); //
                //equipo_mbserial[ind].puerto = res->getInt("puerto"); //
                equipo_mbserial[ind].direccion_esclavo = res->getInt("direccion_esclavo"); //
                equipo_mbserial[ind].interface1 = res->getString("interface");             //
                equipo_mbserial[ind].velocidad = res->getInt("velocidad");                 //
                equipo_mbserial[ind].paridad = res->getString("paridad");                  //
                equipo_mbserial[ind].bitsparada = res->getInt("bitsparada");               //
                equipo_mbserial[ind].equipo = res->getInt("equipo");                       //
                equipo_mbserial[ind].usuario = res->getString("usuario");                  //
                equipo_mbserial[ind].password = res->getString("password");                //
                equipo_mbserial[ind].habilitar = ((res->getInt("habilitar")) != 0);        //
                equipo_mbserial[ind].tag = res->getString("tag");
                equipo_mbserial[ind].preservar_picos =
                    ("," + GLOBAL_picos_tags + ",")
                        .find("," + equipo_mbserial[ind].nombre + "/" + equipo_mbserial[ind].tag +
                              ",") != std::string::npos;                                //
                equipo_mbserial[ind].protocolo = res->getString("protocolo");           //
                equipo_mbserial[ind].registro = res->getInt("registro");                //
                equipo_mbserial[ind].funcion = res->getString("funcion");               //
                equipo_mbserial[ind].interpretacion = res->getString("interpretacion"); //
                cadena = res->getString("escalamiento");                                //
                equipo_mbserial[ind].escalamiento = getValfloat(cadena);
                equipo_mbserial[ind].periodo = cast_string_to_double(res->getString("periodo"));
                try {
                    PeriodicDeadline::interval(equipo_mbserial[ind].periodo);
                } catch (const std::invalid_argument &e) {
                    logger_loadconfig.registrarError(std::string("Invalid tag period: ") +
                                                     e.what());
                    return -1;
                } //
                equipo_mbserial[ind].bit1 = res->getInt("bit1");                //
                equipo_mbserial[ind].cantidad1 = res->getInt("cantidad1");      //
                equipo_mbserial[ind].comentario = res->getString("comentario"); //
                equipo_mbserial[ind].unidad = res->getString("unidad");         //
                equipo_mbserial[ind].solocambios = res->getInt("solocambios");  //
                equipo_mbserial[ind].registrable = res->getInt("registrable");  //
                ind++;
            }
            break;

        case RS232:
            ind = 0;
            equipo_rs232[0].protocolo = "RS232";
            while (res->next() && (ind < 512)) {
                id = res->getInt("id"); //
                equipo_rs232[ind].id = id;
                cadena = res->getString("nombre");
                equipo_rs232[ind].nombre = cadena;                               //
                equipo_rs232[ind].interface1 = res->getString("interface");      //
                equipo_rs232[ind].velocidad = res->getInt("velocidad");          //
                equipo_rs232[ind].paridad = res->getString("paridad");           //
                equipo_rs232[ind].bitsparada = res->getInt("bitsparada");        //
                equipo_rs232[ind].equipo = res->getInt("equipo");                //
                equipo_rs232[ind].habilitar = ((res->getInt("habilitar")) != 0); //
                equipo_rs232[ind].tag = res->getString("tag");
                equipo_rs232[ind].preservar_picos =
                    ("," + GLOBAL_picos_tags + ",")
                        .find("," + equipo_rs232[ind].nombre + "/" + equipo_rs232[ind].tag + ",") !=
                    std::string::npos;                                               //
                equipo_rs232[ind].protocolo = res->getString("protocolo");           //
                equipo_rs232[ind].registro = res->getInt("registro");                //
                equipo_rs232[ind].funcion = res->getString("funcion");               //
                equipo_rs232[ind].interpretacion = res->getString("interpretacion"); //
                cadena = res->getString("escalamiento");                             //
                equipo_rs232[ind].escalamiento = getValfloat(cadena);
                equipo_rs232[ind].html_id =
                    tratamientocadena(res->getString("html_id")); // delimitador inicial
                equipo_rs232[ind].html_final =
                    tratamientocadena(res->getString("html_final")); // delimitador final
                equipo_rs232[ind].periodo = cast_string_to_double(res->getString("periodo"));
                try {
                    PeriodicDeadline::interval(equipo_rs232[ind].periodo);
                } catch (const std::invalid_argument &e) {
                    logger_loadconfig.registrarError(std::string("Invalid tag period: ") +
                                                     e.what());
                    return -1;
                } //
                equipo_rs232[ind].bit1 = res->getInt("bit1");                //
                equipo_rs232[ind].cantidad1 = res->getInt("cantidad1");      //
                equipo_rs232[ind].comentario = res->getString("comentario"); //
                equipo_rs232[ind].unidad = res->getString("unidad");         //
                equipo_rs232[ind].solocambios = res->getInt("solocambios");  //
                equipo_rs232[ind].registrable = res->getInt("registrable");  //
                ind++;
            }
            break;
        }

        int cantidaddispo = ind;
        switch (protocoloenum) {
        case MBTCP:
            for (int i = 0; i < cantidaddispo; i++) {
                int j;
                for (j = 0; j < i; j++) {
                    if (equipo_mbtcp[i].nombre.compare(equipo_mbtcp[j].nombre) == 0) {
                        equipo_mbtcp[i].state_mutex = equipo_mbtcp[j].state_mutex;
                        equipo_mbtcp[i].suspension_logged = equipo_mbtcp[j].suspension_logged;
                        equipo_mbtcp[i].pcontador_errores = equipo_mbtcp[j].pcontador_errores;
                        equipo_mbtcp[i].pcontador_errores_acumulados =
                            equipo_mbtcp[j].pcontador_errores_acumulados;
                        equipo_mbtcp[i].ptiempotranscurridoerrores =
                            equipo_mbtcp[j].ptiempotranscurridoerrores;
                        equipo_mbtcp[i].ptiemporeset_cuenta_errores =
                            equipo_mbtcp[j].ptiemporeset_cuenta_errores;
                        equipo_mbtcp[i].pultima_llamada_leer = equipo_mbtcp[j].pultima_llamada_leer;
                        break;
                    }
                }
                if (i == j) {
                    //solo se ejecuta uno por cada dispositivo
                }
            }
            break;
        case MBSERIAL:
            for (int i = 0; i < cantidaddispo; i++) {
                int j;
                for (j = 0; j < i; j++) {
                    if (equipo_mbserial[i].nombre.compare(equipo_mbserial[j].nombre) == 0) {
                        equipo_mbserial[i].state_mutex = equipo_mbserial[j].state_mutex;
                        equipo_mbserial[i].suspension_logged = equipo_mbserial[j].suspension_logged;
                        equipo_mbserial[i].pcontador_errores = equipo_mbserial[j].pcontador_errores;
                        equipo_mbserial[i].pcontador_errores_acumulados =
                            equipo_mbserial[j].pcontador_errores_acumulados;
                        equipo_mbserial[i].ptiempotranscurridoerrores =
                            equipo_mbserial[j].ptiempotranscurridoerrores;
                        equipo_mbserial[i].ptiemporeset_cuenta_errores =
                            equipo_mbserial[j].ptiemporeset_cuenta_errores;
                        equipo_mbserial[i].pultima_llamada_leer =
                            equipo_mbserial[j].pultima_llamada_leer;
                        break;
                    }
                }
                if (i == j) {
                    //solo se ejecuta uno por cada dispositivo
                }
            }
            break;
        case RS232:
            for (int i = 0; i < cantidaddispo; i++) {
                int j;
                for (j = 0; j < i; j++) {
                    if (equipo_rs232[i].nombre.compare(equipo_rs232[j].nombre) == 0) {
                        equipo_rs232[i].state_mutex = equipo_rs232[j].state_mutex;
                        equipo_rs232[i].suspension_logged = equipo_rs232[j].suspension_logged;
                        equipo_rs232[i].pcontador_errores = equipo_rs232[j].pcontador_errores;
                        equipo_rs232[i].pcontador_errores_acumulados =
                            equipo_rs232[j].pcontador_errores_acumulados;
                        equipo_rs232[i].ptiempotranscurridoerrores =
                            equipo_rs232[j].ptiempotranscurridoerrores;
                        equipo_rs232[i].ptiemporeset_cuenta_errores =
                            equipo_rs232[j].ptiemporeset_cuenta_errores;
                        equipo_rs232[i].pultima_llamada_leer = equipo_rs232[j].pultima_llamada_leer;
                        equipo_rs232[i].buffer_mutex = equipo_rs232[j].buffer_mutex;
                        equipo_rs232[i].buffer_owner = equipo_rs232[j].buffer_owner;
                        equipo_rs232[i].buf = equipo_rs232[j].buf;
                        equipo_rs232[i].p_n_buf = equipo_rs232[j].p_n_buf;
                        break;
                    }
                }
                if (i == j) {
                    //solo se ejecuta uno por cada dispositivo
                    GLOBAL_contador_equipos_rs232_distintos++;
                    equipo_rs232_distintos[i2] = equipo_rs232[i];
                    i2++;
                }
            }

            break;
        }

        return 1;

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
