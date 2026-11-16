/*
programa dedarrollado por ing. Alejandro diomede - Automatroni - email: diomede3009@gmail.com
Define la estructura de datos llamada Equipos - tcp y serial
*/

#include "include/cls_cmd.h"
#include "include/conversiones.h"
#include <errno.h>
#include <vector>
#include <ctype.h>
#include <modbus.h>
#include "include/modbus_transport.h"
#include "include/runtime_control.h"

using namespace std;

extern void esperar(double segundos);
extern uint32_t G_modbus_timeout;

// retorna 1 si tiene exito escribiendo un registro, sino retorna -1
int cls_cmd::cmdescribir_serial(void) {
    // declaracion de variables para uso con el protocolo modbus
    modbus_t *ctx_tcp = NULL;
    int rc = -1;
    int intentosmax = 3;
    int intento = 0;
    auto lease =
        ModbusLease::rtu(interface1, velocidad, paridad.empty() ? 'N' : paridad[0], 8, bitsparada);
    ctx_tcp = lease.get();
    if (ctx_tcp == NULL) { // if 1
        fprintf(stderr, "[ ERROR ] Escritura RTU: no se pudo iniciar libmodbus.\n");
        return -1;
    } // fin if 1
    if (direccion_esclavo == 0)
        direccion_esclavo = 1;
    modbus_set_slave(ctx_tcp, direccion_esclavo);
    if (lease.connect(G_modbus_timeout) == -1) { // if 3
        fprintf(stderr, "[ ERROR ] Escritura RTU: fallo de conexion: %s\n", modbus_strerror(errno));
        lease.invalidate();
        return -1;
    } // fin del if 3

    while ((intento < intentosmax) && driver_running() && (rc == -1)) { //while 1
        intento++;
        if (intento > 1) {
            esperar(0.1);
        };
        rc = modbus_write_register(ctx_tcp, registro, clsvalor);
    } // fin del while 1

    if (rc < 0)
        lease.invalidate();
    return rc;
}

// retorna 1 si tiene exito escribiendo un registro, sino retorna -1
int cls_cmd::cmdescribir_tcp(void) {
    modbus_t *ctx_tcp = NULL;
    int rc = -1;
    int intentosmax = 3;
    int intento = 0;
    auto lease = ModbusLease::tcp(direccion_ip, puerto);
    ctx_tcp = lease.get();
    if (ctx_tcp == NULL) { // if 1
        fprintf(stderr, "[ ERROR ] Escritura TCP: no se pudo iniciar libmodbus.\n");
        return -1;
    } // fin if 1
    if (direccion_esclavo == 0)
        direccion_esclavo = 1;
    modbus_set_slave(ctx_tcp, direccion_esclavo);
    if (lease.connect(G_modbus_timeout) == -1) { // if 3
        fprintf(stderr, "[ ERROR ] Escritura TCP: fallo de conexion: %s\n", modbus_strerror(errno));
        lease.invalidate();
        return -1;
    } // fin del if 3

    while ((intento < intentosmax) && driver_running() && (rc == -1)) { //while 1
        intento++;
        if (intento > 1) {
            esperar(0.1);
        };
        rc = modbus_write_register(ctx_tcp, registro, clsvalor);
    } // fin del while 1
    if (rc < 0)
        lease.invalidate();
    return rc;
}

int cls_cmd::ejecutar(void) {
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
