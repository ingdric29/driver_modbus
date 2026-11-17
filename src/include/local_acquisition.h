/*
 * Archivo: local_acquisition.h
 * Descripción: Configuración y adquisición local multiequipo sin SQL.
 * Necesita: INI explícito; dispositivos S7 o MBTCP y tags validados.
 * Retornos: Catálogo validado o excepción; run retorna código de proceso.
 */
#ifndef AUTOMONITOR_LOCAL_ACQUISITION_H
#define AUTOMONITOR_LOCAL_ACQUISITION_H
#include "s7_decode.h"
#include "s7_transport.h"
#include <vector>
namespace local_driver {
struct Tag {
    std::string name, address_text, interpretation, unit, function;
    s7::Address address;
    s7::Type type = s7::Type::Byte;
    int modbus_register = 0;
    double period = 1, scale = 1;
};
struct Device {
    std::string name, protocol;
    s7::Endpoint endpoint;
    int slave = 1;
    bool enabled = true;
    std::vector<Tag> tags;
};
struct Config {
    std::vector<Device> devices;
};
Config load(const std::string &path);
int run(const std::string &path, bool check_only = false);
} // namespace local_driver
#endif
