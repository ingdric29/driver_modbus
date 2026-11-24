/*
 * Archivo: cls_cmd.cpp
 * Descripción: Escribe comandos de un registro por Modbus TCP o RTU.
 * Necesita: Destino, esclavo, registro, valor, timeout y estado de ejecución.
 * Retornos: Métodos de escritura: resultado libmodbus; ejecutar: 0 si fue aceptada, negativo
 * ante fallo.
 */

#include "include/cls_cmd.h"
#include "include/conversiones.h"
#include <errno.h>
#include <vector>
#include <ctype.h>
#include <modbus.h>
#include "include/modbus_transport.h"
#include "include/runtime_control.h"
#include "include/command_metrics.h"
#include <arpa/inet.h>

using namespace std;

extern void esperar(double segundos);
extern uint32_t G_modbus_timeout;

/**
 * Valida FC06 sin acceder al enlace ni convertir direcciones documentales 4xxxx.
 * Retorno: motivo de rechazo o cadena vacía. Acepta WORD e INT16 en complemento a dos.
 */
string cls_cmd::validar() const {
    if (!configuration_error.empty())
        return configuration_error;
    if (id <= 0)
        return "id de comando invalido";
    if (!habilitar)
        return "dispositivo deshabilitado";
    if (registro < 0 || registro > 65535)
        return "registro fuera de 0..65535";
    if (clsvalor < -32768 || clsvalor > 65535)
        return "valor fuera de INT16/WORD (-32768..65535)";
    if (protocolo == "MBTCP") {
        in_addr address{};
        if (direccion_ip.find('\0') != string::npos ||
            inet_pton(AF_INET, direccion_ip.c_str(), &address) != 1)
            return "direccion IPv4 invalida";
        if (puerto < 1 || puerto > 65535)
            return "puerto TCP fuera de 1..65535";
        if (direccion_esclavo < 0 || direccion_esclavo > 255)
            return "esclavo TCP fuera de 0..255";
    } else if (protocolo == "MBSERIAL") {
        if (interface1.empty() || interface1.find('\0') != string::npos)
            return "interfaz RTU vacia o invalida";
        if (velocidad <= 0)
            return "velocidad RTU invalida";
        const char parity = paridad.empty() ? 'N' : toupper(static_cast<unsigned char>(paridad[0]));
        if (paridad.size() > 1 || (parity != 'N' && parity != 'E' && parity != 'O'))
            return "paridad RTU invalida";
        if (bitsparada != 1 && bitsparada != 2)
            return "bits de parada RTU fuera de 1..2";
        if (direccion_esclavo < 0 || direccion_esclavo > 247)
            return "esclavo RTU fuera de 0..247";
    } else {
        return "protocolo no soportado para escritura Modbus";
    }
    // Preserve the existing 0 -> 1 normalization; this driver does not write broadcasts.
    return "";
}

namespace {
bool valid_command(const cls_cmd &command, const string &transport) {
    command_metrics::transport_trace() = command_metrics::TransportTrace{};
    auto invalid = command.validar();
    if (invalid.empty() && command.protocolo != transport)
        invalid = "metodo de transporte incompatible con el protocolo";
    if (invalid.empty())
        return true;
    command_metrics::transport_trace().error = EINVAL;
    fprintf(stderr, "[ AVISO ] Comando %d rechazado: %s\n", command.id, invalid.c_str());
    errno = EINVAL;
    return false;
}
} // namespace

// retorna 1 si tiene exito escribiendo un registro, sino retorna -1
/**
 * Escribe un registro de 16 bits mediante un lease Modbus.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 1 si libmodbus confirma el registro; -1 ante fallo o parada sin éxito.
 * Efectos: Puede reintentar; una respuesta Modbus no demuestra el efecto físico final.
 */
int cls_cmd::cmdescribir_serial(void) {
    if (!valid_command(*this, "MBSERIAL"))
        return -1;
    // declaracion de variables para uso con el protocolo modbus
    modbus_t *ctx_tcp = NULL;
    int rc = -1;
    int intentosmax = 3;
    int intento = 0;
    auto &trace = command_metrics::transport_trace();
    auto started = command_metrics::Clock::now();
    auto lease = ModbusLease::rtu(
        interface1, velocidad,
        paridad.empty() ? 'N' : toupper(static_cast<unsigned char>(paridad[0])), 8, bitsparada);
    trace.link_wait_us = command_metrics::elapsed_us(started);
    ctx_tcp = lease.get();
    if (ctx_tcp == NULL) { // if 1
        fprintf(stderr, "[ ERROR ] Escritura RTU: no se pudo iniciar libmodbus.\n");
        return -1;
    } // fin if 1
    if (direccion_esclavo == 0)
        direccion_esclavo = 1;
    modbus_set_slave(ctx_tcp, direccion_esclavo);
    started = command_metrics::Clock::now();
    const auto connected = lease.connect(G_modbus_timeout);
    const int connection_error = connected < 0 ? errno : 0;
    trace.connect_us = command_metrics::elapsed_us(started);
    if (connected == -1) { // if 3
        trace.error = connection_error;
        if (connection_error != EAGAIN)
            fprintf(stderr, "[ ERROR ] Escritura RTU: fallo de conexion: %s\n",
                    modbus_strerror(connection_error));
        lease.invalidate();
        return -1;
    } // fin del if 3

    while ((intento < intentosmax) && driver_running() && (rc == -1)) { //while 1
        intento++;
        if (intento > 1) {
            esperar(0.1);
        };
        started = command_metrics::Clock::now();
        rc = modbus_write_register(ctx_tcp, registro, static_cast<uint16_t>(clsvalor));
        trace.error = rc < 0 ? errno : 0;
        trace.wire_us += command_metrics::elapsed_us(started);
        ++trace.attempts;
    } // fin del while 1

    if (rc < 0)
        lease.invalidate();
    return rc;
}

// retorna 1 si tiene exito escribiendo un registro, sino retorna -1
/**
 * Escribe un registro de 16 bits mediante un lease Modbus.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 1 si libmodbus confirma el registro; -1 ante fallo o parada sin éxito.
 * Efectos: Puede reintentar; una respuesta Modbus no demuestra el efecto físico final.
 */
int cls_cmd::cmdescribir_tcp(void) {
    if (!valid_command(*this, "MBTCP"))
        return -1;
    modbus_t *ctx_tcp = NULL;
    int rc = -1;
    int intentosmax = 3;
    int intento = 0;
    auto &trace = command_metrics::transport_trace();
    auto started = command_metrics::Clock::now();
    auto lease = ModbusLease::tcp(direccion_ip, puerto);
    trace.link_wait_us = command_metrics::elapsed_us(started);
    ctx_tcp = lease.get();
    if (ctx_tcp == NULL) { // if 1
        fprintf(stderr, "[ ERROR ] Escritura TCP: no se pudo iniciar libmodbus.\n");
        return -1;
    } // fin if 1
    if (direccion_esclavo == 0)
        direccion_esclavo = 1;
    modbus_set_slave(ctx_tcp, direccion_esclavo);
    started = command_metrics::Clock::now();
    const auto connected = lease.connect(G_modbus_timeout);
    const int connection_error = connected < 0 ? errno : 0;
    trace.connect_us = command_metrics::elapsed_us(started);
    if (connected == -1) { // if 3
        trace.error = connection_error;
        if (connection_error != EAGAIN)
            fprintf(stderr, "[ ERROR ] Escritura TCP: fallo de conexion: %s\n",
                    modbus_strerror(connection_error));
        lease.invalidate();
        return -1;
    } // fin del if 3

    while ((intento < intentosmax) && driver_running() && (rc == -1)) { //while 1
        intento++;
        if (intento > 1) {
            esperar(0.1);
        };
        started = command_metrics::Clock::now();
        rc = modbus_write_register(ctx_tcp, registro, static_cast<uint16_t>(clsvalor));
        trace.error = rc < 0 ? errno : 0;
        trace.wire_us += command_metrics::elapsed_us(started);
        ++trace.attempts;
    } // fin del while 1
    if (rc < 0)
        lease.invalidate();
    return rc;
}

/**
 * Selecciona el transporte del comando y normaliza su resultado.
 * Parámetros: ninguno; utiliza el contexto descrito en el encabezado.
 * Retorno: 0 si la escritura fue aceptada; negativo si falla o no se soporta el protocolo.
 */
int cls_cmd::ejecutar(void) {
    if (!valid_command(*this, protocolo))
        return -1;
    int res = -1;
    int resultadofinal = 0;
    if (protocolo.compare("MBSERIAL") == 0) {
        res = cmdescribir_serial();
    } else if (protocolo.compare("MBTCP") == 0) {
        res = cmdescribir_tcp();
    } else {
        res = -1;
    }

    if (res > 0) {
        resultadofinal = 0;
    } else {
        resultadofinal = res;
    }

    return resultadofinal;
}
