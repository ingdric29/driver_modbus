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

using repository_detail::is_external_target;

int borrar_registros(double dias, string direccion_db, int puerto_db, string nombre_db,
                     string usuario_db, string clave_db, string dbtabla) {
    // Validaciones de red e interfaz
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
        } else {
        }
    }

    string direccionpuerto = "tcp://" + direccion_db + ":" + cast_int_to_string(puerto_db);

    // 1. Calcular la fecha limite directamente en C++ sin consultar a MySQL
    time_t tiempo_corte = time(NULL) - static_cast<time_t>(dias * 86400.0);
    string fechacorte_str = cast_time_to_datetime(tiempo_corte);

    imprimir_mensaje("borrar registros: bd: " + direccionpuerto, amarillo);
    imprimir_mensaje("borrar registros: conservando datos desde: " + fechacorte_str, amarillo);

    try {

        // Uso de smart pointers para garantizar liberacion automatica de memoria (RIIA)
        auto con = sql_connection(direccionpuerto, usuario_db, clave_db, nombre_db);
        con->setSchema(nombre_db.c_str());

        std::unique_ptr<sql::Statement> stmt(con->createStatement());

        // 2. Borrado por lotes (Chunks) de 10,000 registros para evitar bloqueos y timeouts
        const int LOTE_TAMANO = 10000;
        int total_eliminados = 0;
        int filas_afectadas = 0;

        string sql_delete = "DELETE FROM `" + dbtabla + "` WHERE `fechatiempo` < '" +
                            fechacorte_str + "' LIMIT " + std::to_string(LOTE_TAMANO);

        do {
            // executeUpdate retorna el numero de filas eliminadas en este lote
            filas_afectadas = stmt->executeUpdate(sql_delete);
            total_eliminados += filas_afectadas;

            if (filas_afectadas > 0) {
                // Pausa tactica de 50ms para permitir que otros hilos/consultas accedan a la tabla
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

        } while (filas_afectadas >= LOTE_TAMANO);

        imprimir_mensaje("borrar registros: eliminacion completada. Total filas borradas: " +
                             std::to_string(total_eliminados),
                         verde);

        return total_eliminados;
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
