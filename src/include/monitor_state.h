#ifndef AUTOMONITOR_MONITOR_STATE_H
#define AUTOMONITOR_MONITOR_STATE_H
#include "version.h"
// Producers copy results here. The viewer never takes acquisition/SQL locks.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <deque>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <locale>
#include <unistd.h>
#include "driver_metrics.h"
namespace monitor {
using Clock = std::chrono::steady_clock;
inline long long age_ms(Clock::time_point now, Clock::time_point then) {
    return std::max<long long>(
        0, std::chrono::duration_cast<std::chrono::milliseconds>(now - then).count());
}
// JSON strings preserve valid UTF-8 and replace invalid serial bytes.
inline std::string quote(const std::string &text) {
    std::string out = "\"";
    const char *hex = "0123456789abcdef";
    for (size_t i = 0; i < text.size();) {
        unsigned char c = text[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
            ++i;
        } else if (c < 32) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
            ++i;
        } else if (c < 128) {
            out += char(c);
            ++i;
        } else {
            size_t n = c >= 0xc2 && c <= 0xdf   ? 2
                       : c >= 0xe0 && c <= 0xef ? 3
                       : c >= 0xf0 && c <= 0xf4 ? 4
                                                : 0;
            bool valid = n && i + n <= text.size();
            for (size_t j = 1; valid && j < n; ++j)
                valid = (static_cast<unsigned char>(text[i + j]) & 0xc0) == 0x80;
            if (valid && n >= 3) {
                unsigned char b = text[i + 1];
                valid = !(c == 0xe0 && b < 0xa0) && !(c == 0xed && b >= 0xa0) &&
                        !(c == 0xf0 && b < 0x90) && !(c == 0xf4 && b >= 0x90);
            }
            if (valid) {
                out.append(text, i, n);
                i += n;
            } else {
                out += "\\ufffd";
                ++i;
            }
        }
    }
    return out + '"';
}
inline std::string number(double n) {
    if (!std::isfinite(n))
        return "null";
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out.precision(12);
    out << n;
    return out.str();
}
struct Tag {
    uint64_t key = 0;
    std::string device_key, device, protocol, endpoint, name, unit;
    int slave = 0, address = 0;
    double period = 1;
    bool attempted = false, valid = false, has_value = false;
    int result = 0;
    uint64_t reads = 0, failures = 0, consecutive_failures = 0, latency_us = 0;
    std::string text;
    time_t acquired = 0;
    Clock::time_point last_success;
    std::string quality(Clock::time_point now) const {
        if (!attempted)
            return "PENDIENTE";
        if (!valid)
            return result == -2 || result == -3 ? "REINTENTANDO" : "ERROR";
        if (age_ms(now, last_success) > std::max(1000.0, period * 3000.0))
            return "ANTIGUO";
        return "ACTUAL";
    }
};
struct Event {
    time_t time = 0;
    std::string level, component, message;
    uint64_t repeats = 1;
};
struct Alarm {
    bool known = false, active = false;
    std::string device, description;
};
struct Snapshot {
    std::string concentrator, phase, db_endpoint, db_schema;
    uint64_t generation = 0, sql_ok = 0, sql_errors = 0;
    int db_result = 0; // 0 unobserved, 1 successful operation, -1 failed operation
    Clock::time_point started, sql_last;
    std::vector<Tag> tags;
    std::deque<Event> events;
    std::map<int, Alarm> alarms;
};
class State {
    std::mutex mutex_;
    Snapshot state_;
    uint64_t next_key_ = 0;
    void event_locked(std::string level, std::string component, std::string message) {
        message = message.substr(0, 1024);
        auto now = std::time(nullptr);
        if (!state_.events.empty()) {
            Event &last = state_.events.back();
            if (last.level == level && last.component == component && last.message == message) {
                ++last.repeats;
                last.time = now;
                return;
            }
        }
        Event e;
        e.time = now;
        e.level = level;
        e.component = component;
        e.message = message;
        state_.events.push_back(e);
        if (state_.events.size() > 128)
            state_.events.pop_front();
    }

  public:
    State() {
        state_.started = Clock::now();
        state_.phase = "INICIANDO";
    }
    static State &instance() {
        static State state;
        return state;
    }
    void configure(const std::string &concentrator, const std::string &endpoint,
                   const std::string &schema) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.concentrator = concentrator;
        state_.db_endpoint = endpoint;
        state_.db_schema = schema;
    }
    void phase(const std::string &phase) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.phase = phase;
        event_locked("INFO", "SISTEMA", phase);
    }
    void begin_catalog() {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.tags.clear();
        state_.alarms.clear();
        ++state_.generation;
    }
    uint64_t add_tag(Tag tag) {
        std::lock_guard<std::mutex> lock(mutex_);
        tag.key = ++next_key_;
        // Keys are contiguous inside a generation, so publication stays O(1).
        state_.tags.push_back(tag);
        return tag.key;
    }
    void add_alarm(int id, const std::string &device = "", const std::string &description = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        Alarm alarm;
        alarm.device = device;
        alarm.description = description;
        state_.alarms.emplace(id, alarm);
    }
    void alarm(int id, bool known, bool active) noexcept {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            auto i = state_.alarms.find(id);
            if (i == state_.alarms.end())
                return;
            if (known && (!i->second.known || i->second.active != active) &&
                (active || i->second.active))
                event_locked(active ? "WARN" : "INFO", "ALARMA",
                             i->second.device + " / " + i->second.description + " (id=" +
                                 std::to_string(id) + (active ? "): ACTIVA" : "): INACTIVA"));
            i->second.known = known;
            i->second.active = active;
        } catch (...) {
        }
    }
    void publish(uint64_t key, int result, const std::string &text, time_t acquired,
                 uint64_t latency, Clock::time_point now = Clock::now()) noexcept {
        if (!key)
            return;
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            if (state_.tags.empty() || key < state_.tags.front().key)
                return;
            auto index = key - state_.tags.front().key;
            if (index >= state_.tags.size())
                return;
            Tag &t = state_.tags[index];
            const bool old_valid = t.valid, old_attempted = t.attempted;
            t.attempted = true;
            t.valid = result > 0;
            t.result = result;
            ++t.reads;
            t.latency_us = latency;
            if (t.valid) {
                t.has_value = true;
                t.text = text.substr(0, 256);
                t.acquired = acquired;
                t.last_success = now;
                t.consecutive_failures = 0;
            } else {
                ++t.failures;
                ++t.consecutive_failures;
            }
            // One transition per tag; repeated errors are counted without flooding events.
            if ((!old_attempted && !t.valid) || (old_attempted && old_valid != t.valid))
                event_locked(t.valid ? "INFO" : "WARN", "DISPOSITIVO",
                             t.device + " / " + t.name +
                                 (t.valid
                                      ? ": lectura recuperada"
                                      : ": fallo de lectura (rc=" + std::to_string(result) + ")"));
        } catch (...) {
        } // Monitoring must never break acquisition.
    }
    void sql(const std::string &endpoint, const std::string &schema, bool success) noexcept {
        try {
            std::lock_guard<std::mutex> lock(mutex_);
            if (endpoint != state_.db_endpoint || schema != state_.db_schema)
                return;
            state_.db_result = success ? 1 : -1;
            state_.sql_last = Clock::now();
            if (success)
                ++state_.sql_ok;
            else
                ++state_.sql_errors;
        } catch (...) {
        }
    }
    void event(const std::string &message) noexcept {
        if (message.compare(0, 8, "METRICS ") == 0)
            return;
        try {
            std::string level = "ERROR", component = "SISTEMA";
            if (message.compare(0, 5, "INFO:") == 0)
                level = "INFO";
            else if (message.compare(0, 5, "WARN:") == 0)
                level = "WARN";
            else if (message.compare(0, 6, "DEBUG:") == 0)
                level = "DEBUG";
            if (message.find("SQL") != std::string::npos)
                component = "SQL";
            else if (message.find("Modbus") != std::string::npos)
                component = "MODBUS";
            else if (message.find("SMTP") != std::string::npos ||
                     message.find("HTML") != std::string::npos)
                component = "CORREO";
            std::lock_guard<std::mutex> lock(mutex_);
            event_locked(level, component, message);
        } catch (...) {
        }
    }
    Snapshot copy() {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }
};
inline std::string snapshot_json(const Snapshot &s, Clock::time_point now = Clock::now()) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    size_t active = 0, unknown = 0;
    for (const auto &a : s.alarms) {
        if (!a.second.known)
            ++unknown;
        if (a.second.active)
            ++active;
    }
    const auto &m = DriverMetrics::instance();
    out << "{\"version\":1,\"pid\":" << getpid() << ",\"generated_at\":" << std::time(nullptr)
        << ",\"driver_version\":" << quote(AUTOMONITOR_VERSION)
        << ",\"driver_commit\":" << quote(AUTOMONITOR_COMMIT)
        << ",\"build_date\":" << quote(AUTOMONITOR_BUILD_DATE)
        << ",\"uptime_s\":" << age_ms(now, s.started) / 1000 << ",\"generation\":" << s.generation
        << ",\"concentrator\":" << quote(s.concentrator) << ",\"phase\":" << quote(s.phase)
        << ",\"alarms_active\":" << active << ",\"alarms_unknown\":" << unknown
        << ",\"sql\":{\"status\":"
        << quote(s.db_result == 0  ? "SIN_DATOS"
                 : s.db_result > 0 ? "OK"
                                   : "ERROR")
        << ",\"endpoint\":" << quote(s.db_endpoint) << ",\"schema\":" << quote(s.db_schema)
        << ",\"operations_ok\":" << s.sql_ok << ",\"operations_failed\":" << s.sql_errors
        << ",\"age_ms\":" << (s.db_result ? std::to_string(age_ms(now, s.sql_last)) : "null") << "}"
        << ",\"metrics\":{\"reads\":" << m.reads.load()
        << ",\"read_failures\":" << m.read_failures.load()
        << ",\"maximum_read_us\":" << m.maximum_read_us.load()
        << ",\"connections\":" << m.connections.load()
        << ",\"reused_connections\":" << m.reused_connections.load()
        << ",\"stored_samples\":" << m.stored_samples.load()
        << ",\"sql_failures\":" << m.sql_failures.load()
        << ",\"pending_windows\":" << m.pending_windows.load() << "},\"devices\":[";
    std::map<std::string, std::vector<const Tag *>> devices;
    for (const auto &tag : s.tags)
        devices[tag.device_key].push_back(&tag);
    bool first = true;
    for (const auto &device : devices) {
        if (!first)
            out << ',';
        first = false;
        const Tag &d = *device.second.front();
        size_t fresh = 0, pending = 0, retry = 0, stale = 0;
        uint64_t failures = 0, reads = 0;
        for (const auto *t : device.second) {
            auto q = t->quality(now);
            fresh += q == "ACTUAL";
            pending += q == "PENDIENTE";
            retry += q == "REINTENTANDO";
            stale += q == "ANTIGUO";
            failures += t->failures;
            reads += t->reads;
        }
        std::string status = fresh == device.second.size()     ? "ONLINE"
                             : fresh                           ? "DEGRADADO"
                             : pending == device.second.size() ? "PENDIENTE"
                             : retry                           ? "REINTENTANDO"
                             : stale                           ? "DATOS_ANTIGUOS"
                                                               : "SIN_RESPUESTA";
        out << "{\"key\":" << quote(device.first) << ",\"name\":" << quote(d.device)
            << ",\"protocol\":" << quote(d.protocol) << ",\"endpoint\":" << quote(d.endpoint)
            << ",\"slave\":" << d.slave << ",\"status\":" << quote(status)
            << ",\"fresh_tags\":" << fresh << ",\"reads\":" << reads << ",\"failures\":" << failures
            << ",\"tags\":[";
        bool first_tag = true;
        for (const auto *t : device.second) {
            if (!first_tag)
                out << ',';
            first_tag = false;
            out << "{\"key\":" << t->key << ",\"name\":" << quote(t->name)
                << ",\"address\":" << t->address << ",\"period_s\":" << number(t->period)
                << ",\"unit\":" << quote(t->unit) << ",\"status\":" << quote(t->quality(now))
                << ",\"value\":" << (t->has_value ? quote(t->text) : "null")
                << ",\"acquired_at\":" << (t->has_value ? std::to_string(t->acquired) : "null")
                << ",\"age_ms\":"
                << (t->has_value ? std::to_string(age_ms(now, t->last_success)) : "null")
                << ",\"result\":" << t->result << ",\"reads\":" << t->reads
                << ",\"failures\":" << t->failures
                << ",\"consecutive_failures\":" << t->consecutive_failures
                << ",\"latency_us\":" << t->latency_us << '}';
        }
        out << "]}";
    }
    out << "],\"events\":[";
    first = true;
    for (const auto &e : s.events) {
        if (!first)
            out << ',';
        first = false;
        out << "{\"time\":" << e.time << ",\"level\":" << quote(e.level)
            << ",\"component\":" << quote(e.component) << ",\"message\":" << quote(e.message)
            << ",\"repeats\":" << e.repeats << '}';
    }
    out << "]}\n";
    return out.str();
}
} // namespace monitor
#endif
