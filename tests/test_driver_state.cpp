#include "globales.h"
#include <cassert>
#include <cstring>
#include "modbus_transport.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "sql_session.h"
#include "estructuras.h"
#include <cstdlib>
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
int main() { serial_test(); batch_period_test(); if (std::getenv("DRIVER_SQL_TEST")) generic_sql_test(); }
