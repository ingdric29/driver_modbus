// Original implementation: Alejandro Diomede / Automatroni; modularized without logic changes.
#include "include/globales.h"
#include "include/conversiones.h"
#include "include/equipment_internal.h"
#include "include/modbus_decode.h"
#include "include/modbus_transport.h"
#include "include/register_batch.h"
#include "include/runtime_control.h"
#include <cerrno>
#include <cmath>
#include <limits>
#include <sstream>

using namespace std;

void cls_equipos::leer_mbtcp(void) {
    modbus_t *ctx_mbtcp = NULL;
    bool conectadocondispositivo = true;
    string mensajedeerror = "";
    int intento = 0;
    int intentosmax = 3;
    uint16_t datos[20]; //reserva suficiente espacio para 10 registros long o 20 reg word
    uint8_t datos8b[4];
    float valor_float;
    uint64_t valor_long;
    uint16_t valor;
    char buffer[32];
    stringstream ssdatos;
    int rc = -1;

    static_clsnombre_mbtcp = nombre;
    auto lease = ModbusLease::tcp(direccion_ip, puerto);
    ctx_mbtcp = lease.get();
    rastro = nombre + ";" + direccion_ip + ":" + cast_int_to_string(puerto) + ";" + tag + ";" +
             "idslave=" + cast_int_to_string(direccion_esclavo) +
             ";reg=" + cast_int_to_string(registro);

    if (ctx_mbtcp == NULL) {
        fprintf(stderr, "[ ERROR ] No se pudo iniciar libmodbus.\n");
        logger.registrarError("ERROR: No se pudo iniciar libmodbus en: leer_mbtcp() : " +
                              std::string(__FILE__));
        conectadocondispositivo = false;
        resultado_leer = false;
        resultadoleer_rc = -1;
        return;
    }

    if (direccion_esclavo == 0)
        direccion_esclavo = 1;
    modbus_set_slave(ctx_mbtcp, direccion_esclavo);

    if (lease.connect(G_modbus_timeout) == -1) {
        if (errno == EAGAIN) {
            resultadoleer_rc = -3;
            resultado_leer = false;
            return;
        }
        string smsmodbus = string(modbus_strerror(errno));
        mensajedeerror = "fallo en modbus errno= " + smsmodbus + " ";
        mensajedeerror += rastro;
        imprimir_mensaje(mensajedeerror, rojo); //Loggin
        logger.registrarError("AVISO: Fallo de lectura Modbus TCP." + smsmodbus + " " + rastro);
        lease.invalidate();
        conectadocondispositivo = false;
        resultado_leer = false;
        rc = -1;
    }

    // lee los holding registers
    rc = -1;
    intento = 0;
    cantidad1 = std::max(1, std::min(cantidad1, 20));
    //modbus_set_response_timeout(ctx_mbtcp, 0, 200000);

    while ((intento < intentosmax) && driver_running() && (rc == -1) &&
           (conectadocondispositivo == true)) {

        intento++;

        memset(datos, 0, 20 * sizeof(uint16_t));
        memset(datos8b, 0, 4 * sizeof(uint8_t));
        memset(buffer, 0, sizeof(buffer));

        valor_float = 0.0;
        valor_long = 0;
        valor = 0;

        if (intento > 1) {
            esperar(0.1);
        };

        // ============================================================
        // HOLDING REGISTERS (hr)
        // ============================================================

        if (funcion.compare("hr") == 0) {

            if (interpretacion.compare("float32abcd") == 0) {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0) {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0) {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0) {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
                imprimir_mensaje("**" + cast_float_to_string(valor_float), negro);
            }

            else if (interpretacion.compare("float32fp") == 0) {
                // solo para los PM factor de potencia
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                valor_float = convert_float_fp(valor_float);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0) {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_long = decode_u32_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longle64") == 0) {
                rc = read_cached_registers(lease, 3, registro, 4, datos);
                valor_long = decode_u64_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longbe") == 0) {
                rc = read_cached_registers(lease, 3, registro, 2, datos);
                valor_long = decode_u32_be(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("word") == 0) {
                rc = read_cached_registers(lease, 3, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                } else {
                    valor = datos[0];
                }

                snprintf(buffer, sizeof(buffer), "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0) {
                rc = read_cached_registers(lease, 3, registro, cantidad1, datos);

                int16_t valor_signed = 0;

                if (rc != -1) {
                    if ((bit1 >= 0) && (bit1 <= 15)) {
                        valor_signed = obtenervalorbit(datos[0], bit1);
                    } else {
                        valor_signed = static_cast<int16_t>(datos[0]);
                    }
                }

                snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(valor_signed));
            }

            else {
                cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;
            }
        }

        // ============================================================
        // INPUT REGISTERS (rr)
        // ============================================================

        if (funcion.compare("rr") == 0) {

            if (interpretacion.compare("float32abcd") == 0) {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_abcd(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32badc") == 0) {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_badc(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32cdab") == 0) {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_cdab(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("float32dcba") == 0) {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_float = cast_get_float_dcba(datos);
                snprintf(buffer, sizeof(buffer), "%f", valor_float);
            }

            else if (interpretacion.compare("longle") == 0) {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_long = decode_u32_le(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("longbe") == 0) {
                rc = read_cached_registers(lease, 4, registro, 2, datos);
                valor_long = decode_u32_be(datos);
                snprintf(buffer, sizeof(buffer), "%" PRIu64, valor_long);
            }

            else if (interpretacion.compare("word") == 0) {
                rc = read_cached_registers(lease, 4, registro, cantidad1, datos);

                if ((bit1 >= 0) && (bit1 <= 15)) {
                    valor = obtenervalorbit(datos[0], bit1);
                } else {
                    valor = datos[0];
                }

                snprintf(buffer, sizeof(buffer), "%i", valor);
            }

            // ========================================================
            // NUEVA INTERPRETACION: WORD CON SIGNO (INT16)
            // ========================================================

            else if (interpretacion.compare("word_signed") == 0) {
                rc = read_cached_registers(lease, 4, registro, cantidad1, datos);

                int16_t valor_signed = 0;

                if (rc != -1) {
                    if ((bit1 >= 0) && (bit1 <= 15)) {
                        valor_signed = obtenervalorbit(datos[0], bit1);
                    } else {
                        valor_signed = static_cast<int16_t>(datos[0]);
                    }
                }

                snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(valor_signed));
            }

            else {
                cout << "En \"" << nombre << "\" tipo no def!!: " << interpretacion << endl;
            }
        }

        // ============================================================
        // COILS (cs)
        // ============================================================

        if (funcion.compare("cs") == 0) {
            rc = modbus_read_bits(ctx_mbtcp, registro, 1, datos8b);
            snprintf(buffer, sizeof(buffer), "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

        // ============================================================
        // DISCRETE INPUTS (ir)
        // ============================================================

        if (funcion.compare("ir") == 0) {
            rc = modbus_read_input_bits(ctx_mbtcp, registro, 1, datos8b);
            snprintf(buffer, sizeof(buffer), "%i", datos8b[0]);
            memcpy(datos, datos8b, 4);
        }

    } // fin del while

    // ================================================================
    // RESULTADO DE LA LECTURA
    // ================================================================

    if (rc != -1) {

        setvalorstring(cast_arrchar_to_string(buffer));
        resultado_leer = true;
        *pcontador_errores = 0;

        ssdatos << "dato[0]: " << datos[0] << " dato[1]: " << datos[1];
        imprimir_mensaje(ssdatos.str(), negro);

        imprimir_mensaje("valor float: " + cast_float_to_string(atof(buffer)), negro);
        imprimir_mensaje("valor float escalado: " + cast_float_to_string(clsvalorfloat), negro);

        for (int k = 0; k < 20; ++k)
            static_clsdatosleidos_mbtcp[k] = datos[k];

        for (int offset1 = 0; offset1 < 20; offset1++) {
            if (offset1 >= cantidad1) {
                static_clsregistros_mbtcp[offset1] = -1;
            } else {
                static_clsregistros_mbtcp[offset1] = registro + offset1;
            }
        }

        //leido del tag

        //clsvalor = atoi(buffer);
        //clsvalorfloat = atof(buffer) * escalamiento;
        //clsvalorstr = cast_float_to_string(clsvalorfloat);
    }

    else {
        resultado_leer = false;
        incrementarerrores();
        descripcionerror = rastro + " - error modbus";
        logger.registrarError("AVISO: Fallo de lectura Modbus TCP. " + rastro);
        imprimir_mensaje(descripcionerror, rojo);
    }

    if (rc < 0)
        lease.invalidate();

    resultadoleer_rc = rc;
}
