/*
 * Archivo: local_config.cpp
 * Descripción: Carga INI local usando INIReader existente, sin credenciales SQL.
 * Necesita: Archivo de hasta 256 KiB; nombres de dispositivo/tag explícitos.
 * Retornos: Catálogo completo o invalid_argument; nunca aplica carga parcial.
 */
#include "include/local_acquisition.h"
#include "include/INIReader.h"
#include "include/periodic_deadline.h"
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <locale>

namespace local_driver {
namespace {
bool identifier(const std::string &text) {
    if (text.empty() || text.size() > 30)
        return false;
    for (char c : text)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    return true;
}
double number(const std::string &text) {
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    double result;
    char extra;
    if (!(input >> result) || input >> extra || !std::isfinite(result))
        throw std::invalid_argument("Local config: invalid finite number");
    return result;
}
int integer(const std::string &text, int low, int high) {
    double value = number(text);
    if (value < low || value > high || std::floor(value) != value)
        throw std::invalid_argument("Local config: integer out of range");
    return static_cast<int>(value);
}
void fields(INIReader &reader, const std::string &section, const std::set<std::string> &allowed) {
    for (const auto &key : reader.GetFields(section))
        if (!allowed.count(key))
            throw std::invalid_argument("Local config: unknown field " + key);
}
std::string required(INIReader &reader, const std::string &section, const std::string &key) {
    auto value = reader.Get(section, key, "");
    if (value.empty())
        throw std::invalid_argument("Local config: missing " + key);
    return value;
}
} // namespace
Config load(const std::string &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 0 || file.tellg() > 262144)
        throw std::invalid_argument("Local config: unreadable or larger than 256 KiB");
    INIReader reader(path);
    if (reader.ParseError() != 0)
        throw std::invalid_argument("Local config: INI parse error");
    Config config;
    std::map<std::string, size_t> devices;
    std::set<std::string> endpoints;
    const auto sections = reader.GetSections();
    for (const auto &section : sections) {
        if (section.compare(0, 7, "device:") != 0) {
            if (section.compare(0, 4, "tag:") != 0)
                throw std::invalid_argument("Local config: unknown section");
            continue;
        }
        Device d;
        d.name = section.substr(7);
        if (!identifier(d.name))
            throw std::invalid_argument("Local config: invalid device name");
        fields(reader, section,
               {"protocol", "host", "port", "rack", "slot", "timeout_ms", "slave", "enabled"});
        d.protocol = required(reader, section, "protocol");
        if (d.protocol != "SIEMENS_S7" && d.protocol != "MBTCP")
            throw std::invalid_argument("Local config: protocol must be SIEMENS_S7 or MBTCP");
        d.endpoint.host = required(reader, section, "host");
        d.endpoint.port =
            integer(reader.Get(section, "port", d.protocol == "MBTCP" ? "502" : "102"), 1, 65535);
        d.endpoint.rack = integer(reader.Get(section, "rack", "0"), 0, 7);
        d.endpoint.slot = integer(reader.Get(section, "slot", "1"), 0, 31);
        d.endpoint.timeout_ms = integer(reader.Get(section, "timeout_ms", "1000"), 50, 5000);
        d.slave = integer(reader.Get(section, "slave", "1"), 1, 247);
        d.enabled = integer(reader.Get(section, "enabled", "1"), 0, 1) != 0;
        s7::validate_endpoint(d.endpoint);
        std::string key = d.protocol + ":" + d.endpoint.host + ":" +
                          std::to_string(d.endpoint.port) + ":" + std::to_string(d.endpoint.rack) +
                          ":" + std::to_string(d.endpoint.slot);
        if (!endpoints.insert(key).second)
            throw std::invalid_argument("Local config: duplicate endpoint");
        devices.emplace(d.name, config.devices.size());
        config.devices.push_back(d);
    }
    size_t tag_count = 0;
    for (const auto &section : sections) {
        if (section.compare(0, 4, "tag:") != 0)
            continue;
        size_t separator = section.find(':', 4);
        if (separator == std::string::npos)
            throw std::invalid_argument("Local config: missing tag name");
        auto found = devices.find(section.substr(4, separator - 4));
        if (found == devices.end())
            throw std::invalid_argument("Local config: tag references missing device");
        auto &device = config.devices[found->second];
        Tag t;
        t.name = section.substr(separator + 1);
        if (!identifier(t.name))
            throw std::invalid_argument("Local config: invalid tag name");
        fields(reader, section, {"address", "type", "period", "scale", "unit", "function"});
        t.address_text = required(reader, section, "address");
        t.interpretation = required(reader, section, "type");
        t.period = number(reader.Get(section, "period", "1"));
        PeriodicDeadline::interval(t.period);
        if (t.period < 0.02)
            throw std::invalid_argument("Local config: minimum period is 20 ms");
        t.scale = number(reader.Get(section, "scale", "1"));
        t.unit = reader.Get(section, "unit", "");
        if (t.unit.size() > 30 || t.unit.find_first_of("\r\n\t") != std::string::npos)
            throw std::invalid_argument("Local config: invalid unit");
        t.function = reader.Get(section, "function", "hr");
        if (device.protocol == "SIEMENS_S7") {
            t.address = s7::parse_address(t.address_text);
            t.type = s7::parse_type(t.interpretation);
            s7::validate(t.address, t.type);
            if (reader.GetFields(section).count("function"))
                throw std::invalid_argument("Local config: S7 has no Modbus function");
        } else {
            size_t position = 0;
            t.modbus_register = s7::decimal(t.address_text, position, 65535);
            if (position != t.address_text.size() ||
                (t.interpretation != "word" && t.interpretation != "word_signed") ||
                (t.function != "hr" && t.function != "rr") ||
                !std::isfinite(static_cast<float>(t.scale)))
                throw std::invalid_argument("Local config: invalid Modbus word tag");
        }
        device.tags.push_back(t);
        ++tag_count;
    }
    if (config.devices.empty() || config.devices.size() > 64 || tag_count > 2048)
        throw std::invalid_argument("Local config: requires 1..64 devices and at most 2048 tags");
    for (const auto &device : config.devices)
        if (device.enabled && device.tags.empty())
            throw std::invalid_argument("Local config: enabled device has no tags");
    return config;
}
} // namespace local_driver
