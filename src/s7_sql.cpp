/* Catálogo y escritor S7. Usa direccion_siemens y s7_*, nunca registro/funcion.
 * Copia muestras bajo mutex; SQL se ejecuta fuera del bloqueo de adquisición. */
#include "include/s7_sql.h"
#include "include/globales.h"
#include "include/sql_session.h"
#include "include/runtime_control.h"
#include "include/periodic_deadline.h"
#include <map>
#include <set>
#include <iomanip>
namespace local_driver {
namespace {
auto connection() -> std::shared_ptr<sql::Connection> {
    return sql_connection("tcp://" + GLOBAL_db_direccion + ":" + GLOBAL_db_puerto,
                          GLOBAL_db_usuario, GLOBAL_db_clave, GLOBAL_db_nombre);
}
void length(const std::string &s, size_t maximum) {
    if (s.empty() || s.size() > maximum)
        throw std::invalid_argument("S7 SQL identity length invalid");
}
} // namespace
SqlCatalog load_sql_s7() {
    SqlCatalog catalog;
    auto con = connection();
    std::unique_ptr<sql::Statement> count_stmt(con->createStatement());
    std::unique_ptr<sql::ResultSet> count(count_stmt->executeQuery(
        "SELECT COUNT(*) AS n FROM tbdispositivos WHERE protocolo='SIEMENS_S7' AND habilitar=1"));
    if (!count->next() || count->getInt("n") == 0)
        return catalog;
    std::unique_ptr<sql::PreparedStatement> query(con->prepareStatement(
        "SELECT d.id AS device_id,d.nombre,d.direccion_ip,d.puerto,d.rack,d.slot,"
        "r.id AS register_id,r.tag,r.direccion_siemens,r.interpretacion,r.periodo,"
        "r.escalamiento,r.unidad,r.registrable,r.solocambios "
        "FROM tbdispositivos d JOIN tbregistros r ON r.modelo=d.modelo "
        "WHERE d.protocolo='SIEMENS_S7' AND d.habilitar=1 AND d.concentrador=? "
        "ORDER BY d.id,r.id"));
    query->setString(1, GLOBAL_nombre_concentrador);
    std::unique_ptr<sql::ResultSet> rows(query->executeQuery());
    std::map<int, size_t> devices;
    std::set<std::pair<int, std::string>> names;
    while (rows->next()) {
        SqlTag entry;
        entry.device_id = rows->getInt("device_id");
        entry.register_id = rows->getInt("register_id");
        entry.device = rows->getString("nombre");
        entry.concentrator = GLOBAL_nombre_concentrador;
        length(entry.device, 20);
        length(entry.concentrator, 20);
        if (!devices.count(entry.device_id)) {
            Device d;
            d.name = entry.device;
            d.protocol = "SIEMENS_S7";
            d.endpoint.host = rows->getString("direccion_ip");
            d.endpoint.port = rows->getInt("puerto");
            if (rows->isNull("rack") || rows->isNull("slot"))
                throw std::invalid_argument("S7 SQL rack/slot is NULL");
            d.endpoint.rack = rows->getInt("rack");
            d.endpoint.slot = rows->getInt("slot");
            s7::validate_endpoint(d.endpoint);
            devices[entry.device_id] = catalog.config.devices.size();
            catalog.config.devices.push_back(d);
        }
        auto &device = catalog.config.devices[devices.at(entry.device_id)];
        auto &tag = entry.tag;
        tag.name = rows->getString("tag");
        length(tag.name, 20);
        if (!names.insert({entry.device_id, tag.name}).second)
            throw std::invalid_argument("S7 duplicate tag for device");
        tag.address_text = rows->getString("direccion_siemens");
        tag.interpretation = rows->getString("interpretacion");
        if (tag.interpretation.compare(0, 3, "s7_") != 0)
            throw std::invalid_argument("S7 SQL requires s7_* interpretation");
        tag.address = s7::parse_address(tag.address_text);
        tag.type = s7::parse_type(tag.interpretation);
        s7::validate(tag.address, tag.type);
        tag.period = rows->getDouble("periodo");
        PeriodicDeadline::interval(tag.period);
        if (tag.period < 0.02)
            throw std::invalid_argument("S7 period below 20 ms");
        tag.scale = rows->getDouble("escalamiento");
        if (!std::isfinite(tag.scale))
            throw std::invalid_argument("S7 invalid scale");
        tag.unit = rows->isNull("unidad") ? "" : rows->getString("unidad");
        entry.registrable = rows->getInt("registrable") != 0;
        entry.only_changes = rows->getInt("solocambios") != 0;
        entry.trace = entry.device + ";" + device.endpoint.host + ":" +
                      std::to_string(device.endpoint.port) + ";" + tag.name +
                      ";protocolo=SIEMENS_S7;rack=" + std::to_string(device.endpoint.rack) +
                      ";slot=" + std::to_string(device.endpoint.slot) + ";dir=" + tag.address_text +
                      ";tipo=" + tag.interpretation + ";esc=" + monitor::number(tag.scale);
        length(entry.trace, 255);
        device.tags.push_back(tag);
        catalog.tags.push_back(entry);
        if (catalog.tags.size() > 2048 || catalog.config.devices.size() > 64)
            throw std::invalid_argument("S7 SQL catalog too large");
    }
    return catalog;
}
StoreResult store_s7(const SqlTag &tag, const SampleValue &sample, bool instant, bool history) {
    if (!std::isfinite(sample.value) || (!instant && !history))
        throw std::invalid_argument("Invalid SQL sample");
    std::shared_ptr<sql::Connection> con;
    bool committing = false;
    try {
        con = connection();
        con->setAutoCommit(false);
        auto text = s7::value_text(sample.value);
        if (text.size() > 20) {
            std::ostringstream compact;
            compact.imbue(std::locale::classic());
            compact << std::setprecision(12) << sample.value;
            text = compact.str();
        }
        auto bind = [&](sql::PreparedStatement &s) {
            s.setString(1, sql_timestamp(sample.acquired));
            s.setString(2, tag.tag.name);
            s.setString(3, text);
            s.setDouble(4, sample.value);
            s.setInt(5, tag.device_id);
            s.setString(6, tag.trace);
            s.setString(7, tag.concentrator);
            s.setString(8, tag.device);
        };
        if (instant) {
            std::unique_ptr<sql::PreparedStatement> find(
                con->prepareStatement("SELECT id FROM tbinstantaneos WHERE tag=? AND "
                                      "concentrador=? AND nombredispositivo=? FOR UPDATE"));
            find->setString(1, tag.tag.name);
            find->setString(2, tag.concentrator);
            find->setString(3, tag.device);
            std::unique_ptr<sql::ResultSet> rows(find->executeQuery());
            bool exists = rows->next();
            int64_t id = exists ? rows->getInt64("id") : 0;
            rows.reset();
            std::unique_ptr<sql::PreparedStatement> stmt(con->prepareStatement(
                exists
                    ? "UPDATE tbinstantaneos SET "
                      "timedate=CURRENT_TIMESTAMP,fechatiempo=?,tag=?,valorstr=?,valorfloat=?,"
                      "dispositivo=?,rastro=?,concentrador=?,nombredispositivo=? WHERE id=?"
                    : "INSERT INTO tbinstantaneos "
                      "(timedate,fechatiempo,tag,valorstr,valorfloat,dispositivo,rastro,"
                      "concentrador,nombredispositivo) VALUES(CURRENT_TIMESTAMP,?,?,?,?,?,?,?,?)"));
            bind(*stmt);
            if (exists)
                stmt->setInt64(9, id);
            stmt->executeUpdate();
        }
        if (history) {
            std::unique_ptr<sql::PreparedStatement> stmt(con->prepareStatement(
                "INSERT INTO tbvalores "
                "(timedate,fechatiempo,tag,valorstr,valorfloat,dispositivo,rastro,concentrador,"
                "nombredispositivo) VALUES(CURRENT_TIMESTAMP,?,?,?,?,?,?,?,?)"));
            bind(*stmt);
            stmt->executeUpdate();
        }
        committing = true;
        con->commit();
        con->setAutoCommit(true);
        committing = false;
        sql_sessions().observe(con.get(), true);
        if (history)
            ++DriverMetrics::instance().stored_samples;
        return StoreResult::Stored;
    } catch (const std::exception &) {
        if (con) {
            sql_sessions().observe(con.get(), false);
            try {
                con->rollback();
            } catch (...) {
            }
        }
        sql_sessions().invalidate();
        ++DriverMetrics::instance().sql_failures;
        monitor::State::instance().event(
            committing ? "WARN: SQL S7 COMMIT incierto; lote no se reintenta"
                       : "WARN: SQL S7 escritura fallida; reintento pendiente");
        return committing ? StoreResult::Uncertain : StoreResult::Retry;
    }
}
void write_sql_s7(std::vector<SqlTag> tags) {
    struct Pending {
        SampleValue sample;
        bool active = false, instant = false, history = false, known = false;
        double confirmed = 0;
        uint64_t consumed = 0;
        PeriodicDeadline instant_due, history_due, retry;
    };
    std::vector<Pending> pending(tags.size());
    while (driver_running()) {
        for (size_t i = 0; i < tags.size() && driver_running(); ++i) {
            auto &t = tags[i];
            auto &p = pending[i];
            if (!p.retry.due())
                continue;
            if (!p.active) {
                std::lock_guard<std::mutex> lock(t.sample->mutex);
                if (!t.sample->valid || t.sample->latest.sequence == p.consumed)
                    continue;
                auto sample = t.sample->latest;
                p.instant = p.instant_due.due();
                bool changed = !p.known || (t.tag.type == s7::Type::Real
                                                ? std::fabs(sample.value - p.confirmed) > 0.0001
                                                : sample.value != p.confirmed);
                p.history = t.registrable && p.history_due.due() && (!t.only_changes || changed);
                if (!p.instant && !p.history)
                    continue;
                p.sample = sample;
                p.active = true;
            }
            auto result = store_s7(t, p.sample, p.instant, p.history);
            if (result == StoreResult::Retry) {
                p.retry.defer(1);
                continue;
            }
            p.active = false;
            p.consumed = p.sample.sequence;
            if (p.instant)
                p.instant_due.advance(t.tag.period);
            if (p.history) {
                p.history_due.advance(
                    std::max(t.tag.period, GLOBAL_periodo_minimo_escritura_tbvalores));
                p.known = result == StoreResult::Stored;
                p.confirmed = p.sample.value;
            }
        }
        wait_driver_seconds(0.02);
    }
    size_t outstanding = 0;
    for (const auto &p : pending)
        outstanding += p.active;
    if (outstanding)
        monitor::State::instance().event("WARN: SQL S7 cierre con muestras pendientes en RAM: " +
                                         std::to_string(outstanding));
}
} // namespace local_driver
