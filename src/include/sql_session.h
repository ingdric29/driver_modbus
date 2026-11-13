#ifndef DRIVER_SQL_SESSION_H
#define DRIVER_SQL_SESSION_H
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/datatype.h>
#include <memory>
#include <map>
#include <tuple>
#include <string>
#include <stdexcept>
#include <cctype>
#include <ctime>
#include "sample_window.h"

class SqlSessionPool {
    using Key = std::tuple<std::string, std::string, std::string, std::string>;
    sql::Driver* driver_;
    std::map<Key, std::shared_ptr<sql::Connection>> connections_;
public:
    SqlSessionPool() : driver_(get_driver_instance()) { driver_->threadInit(); }
    ~SqlSessionPool() { connections_.clear(); driver_->threadEnd(); }
    void invalidate() { connections_.clear(); }
    std::shared_ptr<sql::Connection> get(const std::string& endpoint, const std::string& user, const std::string& password, const std::string& schema) {
        const Key key{endpoint, user, password, schema};
        auto existing = connections_.find(key);
        if (existing != connections_.end() && !existing->second->isClosed()) return existing->second;
        sql::ConnectOptionsMap options;
        options["hostName"] = endpoint; options["userName"] = user; options["password"] = password;
        options["OPT_CONNECT_TIMEOUT"] = 3; options["OPT_READ_TIMEOUT"] = 3; options["OPT_WRITE_TIMEOUT"] = 3;
        auto connection = std::shared_ptr<sql::Connection>(driver_->connect(options));
        connection->setSchema(schema);
        if (connections_.size() >= 8) connections_.clear();
        connections_[key] = connection;
        return connection;
    }
};
inline SqlSessionPool& sql_sessions() { static thread_local SqlSessionPool pool; return pool; }
inline std::shared_ptr<sql::Connection> sql_connection(const std::string& endpoint, const std::string& user, const std::string& password, const std::string& schema) {
    return sql_sessions().get(endpoint, user, password, schema);
}
inline std::string sql_identifier(const std::string& name) {
    if (name.empty()) throw std::invalid_argument("empty SQL identifier");
    for (unsigned char c : name) if (!std::isalnum(c) && c != '_') throw std::invalid_argument("invalid SQL identifier");
    return "`" + name + "`";
}
inline void bind_legacy(sql::PreparedStatement& statement, int index, const std::string& value) {
    if (value == "NULL" || value == "-nan") statement.setNull(index, sql::DataType::VARCHAR);
    else statement.setString(index, value);
}
inline std::string sql_timestamp(time_t time) {
    std::tm local{}; localtime_r(&time, &local);
    char result[32]; std::strftime(result, sizeof(result), "%Y-%m-%d %H:%M:%S", &local);
    return result;
}
class SqlTransaction {
    std::shared_ptr<sql::Connection> connection_;
    bool committed_ = false;
public:
    explicit SqlTransaction(std::shared_ptr<sql::Connection> connection) : connection_(std::move(connection)) { connection_->setAutoCommit(false); }
    void commit() { connection_->commit(); committed_ = true; connection_->setAutoCommit(true); }
    ~SqlTransaction() { if (!committed_) { try { connection_->rollback(); connection_->setAutoCommit(true); } catch (...) {} } }
};
inline void store_sample_batch(const std::shared_ptr<sql::Connection>& connection, const std::string& table,
    const std::vector<SampleValue>& samples, const std::string& tag, int device,
    const std::string& trace, const std::string& concentrator, const std::string& name) {
    SqlTransaction transaction(connection);
    std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement("INSERT INTO " + sql_identifier(table)
        + " (timedate,fechatiempo,tag,valorstr,valorfloat,dispositivo,rastro,concentrador,nombredispositivo) VALUES (CURRENT_TIMESTAMP,?,?,?,?,?,?,?,?)"));
    for (const auto& sample : samples) {
        statement->setString(1, sql_timestamp(sample.acquired)); statement->setString(2, tag);
        if (std::isfinite(sample.value)) { statement->setString(3, sample.text); statement->setDouble(4, sample.value); }
        else { statement->setNull(3, sql::DataType::VARCHAR); statement->setNull(4, sql::DataType::DOUBLE); }
        statement->setInt(5, device); statement->setString(6, trace);
        statement->setString(7, concentrator); statement->setString(8, name);
        if (statement->executeUpdate() != 1) throw std::runtime_error("historical insert did not affect one row");
    }
    transaction.commit();
}
#endif
