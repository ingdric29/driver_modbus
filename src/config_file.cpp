/*
 * Archivo: config_file.cpp
 * Descripción: Carga, valida y resume la configuración inicial.
 * Necesita: configdb.cfg en el padre del directorio de trabajo; variables globales de
 * configuración.
 * Retornos: loadconfigdb: 1 si la configuración es válida; -1 si no puede cargarse o validarse.
 */

#include "include/driver_services.h"
#include "include/conversiones.h"
#include "include/repository_support.h"
#include "include/INIReader.h"
#include "include/startup_console.h"

using namespace std;

extern storigen GLOBAL_email_origen;

/**
 * Lee y valida configdb.cfg desde el padre del directorio de trabajo.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 1 si termina correctamente; -1 si no puede cargarse o validarse.
 * Efectos: Actualiza parámetros globales y muestra un resumen sin contraseñas.
 */
int loadconfigdb() {
    INIReader reader(obtenerDirectorio("configdb.cfg"));

    if (reader.ParseError() != 0) {
        std::cout
            << "[ ERROR ] No se pudo leer configdb.cfg (archivo ausente o formato invalido).\n";
        return -1;
    }

    std::set<std::string> sections = reader.GetSections();
    for (std::set<std::string>::iterator sectionsIt = sections.begin();
         sectionsIt != sections.end(); sectionsIt++) {
        GLOBAL_db_direccion = reader.Get(*sectionsIt, "db_direccion", "127.0.0.1");
        GLOBAL_db_nombre = reader.Get(*sectionsIt, "db_nombre", "dbdevlog");
        GLOBAL_db_puerto = reader.Get(*sectionsIt, "db_puerto", "3306");
        GLOBAL_db_usuario = reader.Get(*sectionsIt, "db_usuario", "sigge");
        GLOBAL_db_clave = reader.Get(*sectionsIt, "db_clave", "master3112");
        GLOBAL_periodo_muestreo_tbejecutar =
            cast_string_to_double(reader.Get(*sectionsIt, "periodo_muestreo_tbejecutar", "10"));
        GLOBAL_periodo_muestreo_tbcambios =
            cast_string_to_double(reader.Get(*sectionsIt, "periodo_muestreo_tbcambios", "10"));

        GLOBAL_dbext_direccion = reader.Get(*sectionsIt, "dbext_direccion", "127.0.0.1");
        GLOBAL_dbext_nombre = reader.Get(*sectionsIt, "dbext_nombre", "dbdevlog");
        GLOBAL_dbext_puerto = reader.Get(*sectionsIt, "dbext_puerto", "3306");
        GLOBAL_dbext_usuario = reader.Get(*sectionsIt, "dbext_usuario", "sigge");
        GLOBAL_dbext_clave = reader.Get(*sectionsIt, "dbext_clave", "master3112");
        GLOBAL_periodoext_muestreo_tbejecutar =
            cast_string_to_double(reader.Get(*sectionsIt, "periodoext_muestreo_tbejecutar", "5"));

        GLOBAL_picos_tags = reader.Get(*sectionsIt, "picos_tags", "");
        GLOBAL_replicacion_inicial_id =
            reader.GetInteger(*sectionsIt, "replicacion_inicial_id", -1);
        G_modbus_timeout = reader.GetInteger(*sectionsIt, "modbus_timeout_us", 90000);
        if (G_modbus_timeout < 1000 || G_modbus_timeout > 3000000)
            return -1;
        GLOBAL_nombre_concentrador = reader.Get(*sectionsIt, "nombre_concentrador", "raspi");

        GLOBAL_periodo_minimo_escritura_tbvalores = cast_string_to_double(
            reader.Get(*sectionsIt, "periodo_minimo_escritura_tbvalores", "30"));
        GLOBAL_periodo_minimo_escritura_tbvalores_ex = cast_string_to_double(
            reader.Get(*sectionsIt, "periodo_minimo_escritura_tbvalores_ext", "30"));

        GLOBAL_periodo_revision_tablas_locales = cast_string_to_double(
            reader.Get(*sectionsIt, "periodo_revision_tablas_locales", "300"));
        GLOBAL_periodo_revision_tablas_externas = cast_string_to_double(
            reader.Get(*sectionsIt, "periodo_revision_tablas_externas", "300"));

        GLOBAL_maximo_dias_antiguedad_tbvalores = cast_string_to_double(
            reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbvalores", "30"));
        GLOBAL_maximo_dias_antiguedad_tbinstantaneos = cast_string_to_double(
            reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbinstantaneos", "0.5"));

        GLOBAL_maximo_dias_antiguedad_tbvalores_ext = cast_string_to_double(
            reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbvalores_ext", "30"));
        GLOBAL_maximo_dias_antiguedad_tbinstantaneos_ext = cast_string_to_double(
            reader.Get(*sectionsIt, "maximo_dias_antiguedad_tbinstantaneos_ext", "0.5"));

        GLOBAL_sicronizacion_periodo =
            cast_string_to_double(reader.Get(*sectionsIt, "sicronizacion_periodo", "300"));

        GLOBAL_periodo_revision_alarmas =
            cast_string_to_double(reader.Get(*sectionsIt, "periodo_revision_alarmas", "60"));

        GLOBAL_email_origen.from = reader.Get(*sectionsIt, "smtp_from", "automonitor@cd-solec.com");
        GLOBAL_email_origen.username =
            reader.Get(*sectionsIt, "smtp_usuario", "automonitor@cd-solec.com");
        GLOBAL_email_origen.password = reader.Get(*sectionsIt, "smtp_pass", "automa2424.e");
        GLOBAL_email_origen.smtp = reader.Get(*sectionsIt, "smtp_servidor", "smtps://cd-solec.com");

        GLOBAL_direccionip_dns = reader.Get(*sectionsIt, "direccionip_dns", "1.0.0.1");

        GLOBAL_periodo_reseteo_mysqlexterno =
            cast_string_to_double(reader.Get(*sectionsIt, "periodo_reseteo_mysqlexterno", "600"));

        GLOBAL_habilitar_acceso_externo =
            cast_string_to_int(reader.Get(*sectionsIt, "habilitar_acceso_externo", "1"));

        GLOBAL_periodo_minimo_escritura_tbdispositivos_sta = cast_string_to_double(
            reader.Get(*sectionsIt, "GLOBAL_periodo_minimo_escritura_tbdispositivos_sta", "5"));

        GLOBAL_periodo_revision_eventos =
            cast_string_to_double(reader.Get(*sectionsIt, "GLOBAL_periodo_revision_eventos", "4"));

        GLOBAL_maxima_cuenta_errores =
            cast_string_to_double(reader.Get(*sectionsIt, "maxima_cuenta_errores", "50"));

        GLOBAL_tiempo_restablecimiento_comunicacion = cast_string_to_double(
            reader.Get(*sectionsIt, "tiempo_restablecimiento_comunicacion", "60"));

        GLOBAL_cantidadregistrossubir =
            cast_string_to_int(reader.Get(*sectionsIt, "cantidadregistrossubir", "500"));
    }

    try {
        for (double period :
             {GLOBAL_periodo_muestreo_tbejecutar, GLOBAL_periodoext_muestreo_tbejecutar,
              GLOBAL_periodo_muestreo_tbcambios, GLOBAL_periodo_revision_alarmas,
              GLOBAL_periodo_revision_eventos, GLOBAL_periodo_minimo_escritura_tbdispositivos_sta})
            PeriodicDeadline::interval(period);
        if (!std::isfinite(GLOBAL_periodo_minimo_escritura_tbvalores) ||
            GLOBAL_periodo_minimo_escritura_tbvalores < 0)
            return -1;
    } catch (const std::invalid_argument &error) {
        logger_loadconfig.registrarError(error.what());
        return -1;
    }
    cout << "  CONFIGURACION\n";
    startup_row("Archivo", obtenerDirectorio("configdb.cfg"));
    startup_row("Concentrador", GLOBAL_nombre_concentrador);
    startup_row("Base local",
                GLOBAL_db_direccion + ":" + GLOBAL_db_puerto + " / " + GLOBAL_db_nombre);
    startup_row("Usuario local", GLOBAL_db_usuario);
    startup_row("Base externa", GLOBAL_habilitar_acceso_externo
                                    ? GLOBAL_dbext_direccion + ":" + GLOBAL_dbext_puerto + " / " +
                                          GLOBAL_dbext_nombre
                                    : "Deshabilitada");
    if (GLOBAL_habilitar_acceso_externo) {
        startup_row("Usuario externo", GLOBAL_dbext_usuario);
        startup_row("Sincronizacion", startup_seconds(GLOBAL_sicronizacion_periodo));
        startup_row("Lote de replicacion",
                    std::to_string(GLOBAL_cantidadregistrossubir) + " registros");
    }
    cout << "\n  INTERVALOS\n";
    startup_row("Ordenes locales", startup_seconds(GLOBAL_periodo_muestreo_tbejecutar));
    if (GLOBAL_habilitar_acceso_externo)
        startup_row("Ordenes externas", startup_seconds(GLOBAL_periodoext_muestreo_tbejecutar));
    startup_row("Revisar configuracion", startup_seconds(GLOBAL_periodo_muestreo_tbcambios));
    startup_row("Historico (minimo)", startup_seconds(GLOBAL_periodo_minimo_escritura_tbvalores));
    return 1;
}
