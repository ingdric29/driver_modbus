#include "sql_session.h"
#include <cppconn/statement.h>
#include <cassert>
int main() {
    const std::string endpoint = "tcp://127.0.0.1:33077";
    monitor::State::instance().configure("test", endpoint, "driver_test");
    auto connection = sql_connection(endpoint, "root", "", "driver_test");
    assert(connection == sql_connection(endpoint, "root", "", "driver_test"));
    std::unique_ptr<sql::Statement> ddl(connection->createStatement());
    ddl->execute("DROP TABLE IF EXISTS history_fixture");
    ddl->execute("CREATE TABLE history_fixture (id BIGINT AUTO_INCREMENT PRIMARY KEY, timedate TIMESTAMP NULL, fechatiempo DATETIME, tag TEXT, valorstr TEXT, valorfloat DOUBLE CHECK(valorfloat>=0), dispositivo INT, rastro TEXT, concentrador TEXT, nombredispositivo TEXT) ENGINE=InnoDB");
    SampleValue sample; sample.acquired = 1700000000; sample.value = 220;
    sample.text = std::string(5000, '%') + " 'voltage' \\ fin";
    SampleValue bad = sample; bad.value = -1;
    bool failed = false;
    try { store_sample_batch(connection, "history_fixture", {sample, bad}, "rVoltaje'", 1, "trace", "hub", "meter"); }
    catch (const sql::SQLException&) { failed = true; }
    assert(failed);
    assert(monitor::State::instance().copy().db_result == -1);
    std::unique_ptr<sql::ResultSet> count(ddl->executeQuery("SELECT COUNT(*) FROM history_fixture"));
    assert(count->next() && count->getInt(1) == 0); // First insert rolled back too.
    count.reset();
    store_sample_batch(connection, "history_fixture", {sample, sample, sample}, "rVoltaje'", 1, "trace", "hub", "meter");
    std::unique_ptr<sql::ResultSet> rows(ddl->executeQuery("SELECT valorstr,tag FROM history_fixture ORDER BY id"));
    int seen = 0;
    while (rows->next()) { assert(rows->getString(1) == sample.text); assert(rows->getString(2) == "rVoltaje'"); ++seen; }
    assert(seen == 3);
    assert(monitor::State::instance().copy().db_result == 1);
    assert(monitor::State::instance().copy().sql_errors == 1);
    bool rejected = false;
    try { sql_identifier("table;DROP"); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    ddl->execute("DROP TABLE history_fixture");
}
