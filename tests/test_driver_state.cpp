#include "globales.h"
#include <cassert>
#include <cstring>
#include "modbus_transport.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "sql_session.h"
#include "cls_defeventos.h"
#include "estructuras.h"
#include <cstdlib>
#include "runtime_control.h"
void serial_test() {
    cls_equipos tag;
    tag.protocolo = "RS232";
    tag.nombre = "serial-test";
    tag.tag = "temperature";
    tag.html_id = "<v>";
    tag.html_final = "</v>";
    const char* frame = "<v>12.5</v>";
    std::memcpy(tag.buf, frame, std::strlen(frame));
    tag.n_buf = std::strlen(frame);
    assert(tag.leer() == 1);
    assert(tag.resultado_leer);
    assert(tag.clsvalorfloat == 12.5f);
    std::atomic<bool> running{true};
    std::thread producer([&] {
        for (int i = 0; i < 1000; ++i) {
            {
                std::lock_guard<std::mutex> lock(*tag.buffer_mutex);
                const char* next = i % 2 ? "<v>15.0</v>" : "<v>20.0</v>";
                std::memcpy(tag.buf, next, std::strlen(next));
                tag.n_buf = std::strlen(next);
            }
            tag.leer();
        }
        running = false;
    });
    while (running) {
        std::lock_guard<std::recursive_mutex> lock(*tag.state_mutex);
        assert(tag.clsvalorfloat == 12.5f || tag.clsvalorfloat == 15 || tag.clsvalorfloat == 20);
        assert(tag.clsvalor == static_cast<int>(tag.clsvalorfloat));
    }
    producer.join();
    tag.n_buf = 0;
    assert(tag.leer() == -1);
    assert(!tag.resultado_leer);
}

void batch_period_test() {
    auto server = modbus_new_tcp("127.0.0.1", 0);
    assert(server);
    int listener = modbus_tcp_listen(server, 1);
    assert(listener >= 0);
    sockaddr_in address{}; socklen_t length = sizeof(address);
    assert(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    auto mapping = modbus_mapping_new(0, 0, 4, 0);
    assert(mapping); mapping->tab_registers[0] = 220; mapping->tab_registers[1] = 25;
    std::thread peer([&] {
        assert(modbus_tcp_accept(server, &listener) >= 0);
        for (int expected : {2, 1}) {
            uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
            const int received = modbus_receive(server, query);
            assert(received >= 12);
            assert(((int(query[10]) << 8) | query[11]) == expected);
            assert(modbus_reply(server, query, received, mapping) > 0);
        }
    });
    cls_equipos tags[2];
    for (int i = 0; i < 2; ++i) {
        tags[i].protocolo = "MBTCP"; tags[i].nombre = "meter";
        tags[i].direccion_ip = "127.0.0.1"; tags[i].puerto = ntohs(address.sin_port);
        tags[i].funcion = "hr"; tags[i].interpretacion = "word";
        tags[i].registro = i; tags[i].periodo = i == 0 ? 0.1 : 2;
    }
    ordenlecturaequipos(tags, 2);
    assert(tags[0].clsvalor == 220 && tags[1].clsvalor == 25);
    assert(tags[0].sample_sequence == 1 && tags[1].sample_sequence == 1);
    tags[0].read_deadline.defer(0.001);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    ordenlecturaequipos(tags, 2);
    assert(tags[0].sample_sequence == 2);
    assert(tags[1].sample_sequence == 1); // Slow tag was not read early.
    peer.join(); ModbusLease::clear(); modbus_close(server); close(listener);
    modbus_mapping_free(mapping); modbus_free(server);
}
void generic_sql_test() {
    stdatabase db; db.direccion = "127.0.0.1"; db.puerto = 33077;
    db.nombre = "driver_test"; db.usuario = "root"; db.clave = "";
    auto connection = sql_connection("tcp://127.0.0.1:33077", "root", "", "driver_test");
    std::unique_ptr<sql::Statement> ddl(connection->createStatement());
    ddl->execute("DROP TABLE IF EXISTS generic_fixture");
    ddl->execute("CREATE TABLE generic_fixture (name VARCHAR(100),hub VARCHAR(100),tag VARCHAR(100),value TEXT)");
    std::string fields[] = {"name", "hub", "tag", "value"};
    std::string values[] = {"O'Brien", "hub%", "rVoltaje", std::string(5000, '%')};
    assert(insertar_inteligente3(&db, "generic_fixture", 0, 1, 2, fields, values, 4) == 0);
    values[3] = "new 'value' 100%";
    assert(insertar_inteligente3(&db, "generic_fixture", 0, 1, 2, fields, values, 4) == 0);
    std::unique_ptr<sql::ResultSet> rows(ddl->executeQuery("SELECT value FROM generic_fixture"));
    assert(rows->next() && rows->getString(1) == values[3]); assert(!rows->next());
    rows.reset(); ddl->execute("DROP TABLE generic_fixture");
    assert(insertar_inteligente3(&db, "generic_fixture", 0, 1, 2, fields, values, 4) == -1);
}

void event_lookup_test() {
    cls_equipos tcp[1], rtu[1], serial[1];
    equipo_mbtcp = tcp; equipo_mbserial = rtu; equipo_rs232 = serial;
    GLOBAL_contador_equipos_mbtcp = GLOBAL_contador_equipos_mbserial = GLOBAL_contador_equipos_rs232 = 1;
    tcp[0].nombre = "tcp"; rtu[0].nombre = "rtu"; serial[0].nombre = "serial";
    for (auto* tag : {tcp, rtu, serial}) { tag->tag = "status"; tag->clsvalor = 3; }
    cls_defeventos event; event.tag = "status"; event.bit1 = 1;
    event.nombredispositivo = "rtu";
    assert(event.cargar_posicion() == 0 && event.pequipo == rtu && event.valoractual == 1);
    event.nombredispositivo = "serial";
    assert(event.cargar_posicion() == 0 && event.pequipo == serial);
    event.nombredispositivo = "missing";
    assert(event.cargar_posicion() == -1 && event.pequipo == nullptr);
    equipo_mbtcp = equipo_mbserial = equipo_rs232 = nullptr;
    GLOBAL_contador_equipos_mbtcp = GLOBAL_contador_equipos_mbserial = GLOBAL_contador_equipos_rs232 = 0;
}
void replication_test() {
    stdatabase local, remote;
    local.direccion = remote.direccion = "127.0.0.1"; local.puerto = remote.puerto = 33077;
    local.usuario = remote.usuario = "root"; local.nombre = "driver_test"; remote.nombre = "driver_test_replica";
    GLOBAL_habilitar_acceso_externo = 1; GLOBAL_ACCESO_EXTERNO = true;
    GLOBAL_nombre_concentrador = "replica-test-" + std::to_string(getpid());
    auto source = sql_connection("tcp://127.0.0.1:33077", "root", "", local.nombre);
    auto destination = sql_connection("tcp://127.0.0.1:33077", "root", "", remote.nombre);
    std::unique_ptr<sql::Statement> a(source->createStatement()), b(destination->createStatement());
    a->execute("DROP TABLE IF EXISTS replication_fixture"); b->execute("DROP TABLE IF EXISTS replication_fixture");
    std::string columns = " (id BIGINT AUTO_INCREMENT PRIMARY KEY,timedate TIMESTAMP NULL,fechatiempo DATETIME,tag TEXT,valorstr TEXT,valorfloat DOUBLE,dispositivo INT,rastro TEXT,concentrador TEXT,nombredispositivo TEXT) ENGINE=InnoDB";
    a->execute("CREATE TABLE replication_fixture" + columns); b->execute("CREATE TABLE replication_fixture" + columns);
    b->execute("ALTER TABLE replication_fixture ADD CHECK(valorfloat < 300)");
    SampleValue value; value.acquired = 1700000000; value.value = 220; value.text = "same timestamp";
    store_sample_batch(source, "replication_fixture", {value,value,value,value,value}, "voltage", 1, "trace", GLOBAL_nombre_concentrador, "meter");
    assert(sincronizar_tablas(&local, &remote, "replication_fixture", 2) == 2);
    assert(sincronizar_tablas(&local, &remote, "replication_fixture", 2) == 2);
    assert(sincronizar_tablas(&local, &remote, "replication_fixture", 2) == 1);
    assert(sincronizar_tablas(&local, &remote, "replication_fixture", 2) == 0);
    value.acquired -= 100; // Late data must still replicate despite older timestamp.
    store_sample_batch(source, "replication_fixture", {value}, "voltage", 1, "trace", GLOBAL_nombre_concentrador, "meter");
    assert(sincronizar_tablas(&local, &remote, "replication_fixture", 2) == 1);
    SampleValue invalid = value; invalid.value = 999;
    store_sample_batch(source, "replication_fixture", {value,invalid}, "voltage", 1, "trace", GLOBAL_nombre_concentrador, "meter");
    assert(sincronizar_tablas(&local, &remote, "replication_fixture", 2) == -1);
    std::unique_ptr<sql::ResultSet> rows(b->executeQuery("SELECT COUNT(*) FROM replication_fixture"));
    assert(rows->next() && rows->getInt(1) == 6); rows.reset();
    a->execute("UPDATE replication_fixture SET valorfloat=220 WHERE valorfloat=999");
    assert(sincronizar_tablas(&local, &remote, "replication_fixture", 2) == 2);
    rows.reset(b->executeQuery("SELECT COUNT(*) FROM replication_fixture"));
    assert(rows->next() && rows->getInt(1) == 8); rows.reset();
    a->execute("DROP TABLE replication_fixture"); b->execute("DROP TABLE replication_fixture");
}

void command_sql_test() {
    auto setup = sql_connection("tcp://127.0.0.1:33077", "root", "", "driver_test");
    std::unique_ptr<sql::Statement> ddl(setup->createStatement());
    ddl->execute("CREATE DATABASE IF NOT EXISTS driver_cmd_test");
    auto connection = sql_connection("tcp://127.0.0.1:33077", "root", "", "driver_cmd_test");
    ddl.reset(connection->createStatement());
    ddl->execute("DROP TABLE IF EXISTS tbejecutar,tbcmd,tbdispositivos");
    ddl->execute("CREATE TABLE tbdispositivos (id INT,nombre TEXT,direccion_ip TEXT,puerto INT,direccion_esclavo INT,interface TEXT,velocidad INT,paridad TEXT,bitsparada INT,modelo TEXT,usuario TEXT,password TEXT,protocolo TEXT,habilitar INT,concentrador TEXT)");
    ddl->execute("CREATE TABLE tbcmd (id INT,registro INT,valor INT,cmd_datetime DATETIME,hab_ejecutar INT,concentrador TEXT,lote TEXT,nombredispositivo TEXT)");
    ddl->execute("CREATE TABLE tbejecutar (id INT,concentrador TEXT,ejecutar_tbcmd INT,tiempo_tbcmd DATETIME,tiempo_segundos BIGINT,lote TEXT)");
    GLOBAL_nombre_concentrador = "hub'100%";
    std::unique_ptr<sql::PreparedStatement> statement(connection->prepareStatement("INSERT INTO tbdispositivos VALUES (1,'meter','127.0.0.1',502,1,'',9600,'N',1,'model','','','MBTCP',1,?)"));
    statement->setString(1, GLOBAL_nombre_concentrador); assert(statement->executeUpdate() == 1);
    statement.reset(connection->prepareStatement("INSERT INTO tbcmd VALUES (1,123,321,NULL,1,?,'batch','meter')"));
    statement->setString(1, GLOBAL_nombre_concentrador); assert(statement->executeUpdate() == 1);
    statement.reset(connection->prepareStatement("INSERT INTO tbejecutar VALUES (1,?,1,NULL,42,'batch')"));
    statement->setString(1, GLOBAL_nombre_concentrador); assert(statement->executeUpdate() == 1);
    assert(leer_tbcmd("127.0.0.1",33077,"driver_cmd_test","root","") == 1);
    assert(cmd[0].registro == 123 && cmd[0].clsvalor == 321);
    int requested = 0; time_t timestamp = 0; std::string batch;
    assert(leer_tbejecutar(requested,timestamp,batch,"127.0.0.1",33077,"driver_cmd_test","root","") == 0);
    assert(requested == 1 && timestamp == 42 && batch == "batch");
    assert(leer_tbejecutar(requested,timestamp,batch,"127.0.0.1",33077,"driver_cmd_test","root","") == 0 && requested == 0);
    assert(escribir_tbcmd_fechahora(1,"127.0.0.1",33077,"driver_cmd_test","root","") == 0);
    std::unique_ptr<sql::ResultSet> row(ddl->executeQuery("SELECT cmd_datetime IS NOT NULL FROM tbcmd"));
    assert(row->next() && row->getInt(1) == 1); row.reset(); statement.reset();
    ddl->execute("DROP TABLE tbejecutar,tbcmd,tbdispositivos");
    delete[] cmd; cmd = nullptr; GLOBAL_contador_cmd = 0;
}
void shutdown_test() {
    install_driver_signals(); GLOBAL_EJECUTARHILOS = true; GLOBAL_stop_requested = false;
    const auto begin = std::chrono::steady_clock::now();
    std::thread signaler([] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); std::raise(SIGTERM); });
    wait_driver_seconds(60);
    signaler.join();
    assert(GLOBAL_stop_requested && !GLOBAL_EJECUTARHILOS);
    assert(std::chrono::steady_clock::now() - begin < std::chrono::seconds(1));
    GLOBAL_stop_requested = false; GLOBAL_EJECUTARHILOS = true;
}
int main() { shutdown_test();
    serial_test(); batch_period_test(); event_lookup_test();
    cls_sms disabled_sms; assert(disabled_sms.enviar() == CURLE_FAILED_INIT);
    if (std::getenv("DRIVER_SQL_TEST")) { generic_sql_test(); replication_test(); command_sql_test(); }
}
