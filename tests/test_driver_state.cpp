// Archivo: test_driver_state.cpp
// Descripción: Comprueba parsing serie, muestras concurrentes, catálogos y perfiles SQL reservados.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "globales.h"
#include <cassert>
#include <limits>
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
#include "cls_defalarmas.h"
#include "fake_email.h"
#include "command_metrics.h"
#include <poll.h>
#include <fcntl.h>
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

void serial_window_test() {
    cls_equipos tag;
    tag.protocolo = "RS232";
    tag.html_id = "<v>"; tag.html_final = "</v>";
    const char* frame = "<v>12.5</v>";
    std::memcpy(tag.buf, frame, std::strlen(frame));
    tag.n_buf = std::strlen(frame);
    tag.tamanoventana = -1;
    assert(tag.leer() == -1 && !tag.resultado_leer);
    tag.tamanoventana = std::numeric_limits<int>::max();
    assert(tag.leer() == 1 && tag.clsvalorfloat == 12.5f);
}

void directory_lookup_test() {
    // A positive stale count must never dereference a null directory.
    cmd = nullptr; emaillist = nullptr; smslist = nullptr;
    GLOBAL_contador_cmd = GLOBAL_contador_directorio = GLOBAL_contador_numerossms = 2;
    assert(cmd_buscarid(7) == -1 && email_buscarid(7) == -1 && sms_buscarid(7) == -1);
    cls_cmd commands[2]; cls_email emails[2]; cls_sms numbers[2];
    commands[0].id = emails[0].id = numbers[0].clsid = 7;
    commands[1].id = emails[1].id = numbers[1].clsid = 9;
    cmd = commands; emaillist = emails; smslist = numbers;
    assert(cmd_buscarid(7) == 0 && cmd_buscarid(9) == 1 && cmd_buscarid(10) == -1);
    assert(email_buscarid(7) == 0 && email_buscarid(9) == 1 && email_buscarid(10) == -1);
    assert(sms_buscarid(7) == 0 && sms_buscarid(9) == 1 && sms_buscarid(10) == -1);
    GLOBAL_contador_cmd = GLOBAL_contador_directorio = GLOBAL_contador_numerossms = 0;
    assert(cmd_buscarid(7) == -1 && email_buscarid(7) == -1 && sms_buscarid(7) == -1);
    cmd = nullptr; emaillist = nullptr; smslist = nullptr;
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

void alarm_directory_test() {
    cls_equipos tag; tag.nombre = "meter"; tag.tag = "rVoltaje";
    tag.clsvalorfloat = 280; tag.clsvalor = 280; tag.clsvalorstr = "280";
    tag.resultado_leer = true; tag.ultimo_muestreo = time(nullptr); tag.unidad = "V";
    emaillist = nullptr; GLOBAL_contador_directorio = 0;
    // Alarm is created BEFORE the directory, matching the old crash sequence.
    cls_defalarmas alarm;
    alarm.id = 42; alarm.clsdispositivo = "meter"; alarm.clstag = "rVoltaje";
    alarm.clsvalorAlarma = "rVoltaje"; alarm.descripcion = "voltage spike";
    alarm.clsequipo_mbtcp = &tag; alarm.clscantidad_equipo_mbtcp = 1;
    alarm.expresionactivar = "$(meter.rVoltaje)>250";
    alarm.expresiondesactivar = "$(meter.rVoltaje)<240";
    alarm.accionactivar = alarm.acciondesactivar = "EMAIL[7];";
    emaillist = new cls_email[1]; GLOBAL_contador_directorio = 1;
    emaillist[0].id = 7; emaillist[0].to = "first@example.invalid";
    emaillist[0].asunto = "directory subject";
    alarm.ejecutado_activar_bd = true; // This test isolates actions, not SQL history.
    test_email::deliveries.clear();
    assert(alarm.evaluarcompleto() == 1);
    assert(alarm.ejecutado_activar_email && test_email::deliveries.size() == 1);
    assert(test_email::deliveries[0].to == "first@example.invalid");
    assert(emaillist[0].asunto == "directory subject"); // Prepared message is a copy.
    delete[] emaillist;
    emaillist = new cls_email[1]; emaillist[0].id = 7; emaillist[0].to = "replacement@example.invalid";
    tag.clsvalorfloat = 220; tag.clsvalor = 220;
    alarm.ejecutado_desactivar_bd = true;
    assert(alarm.evaluarcompleto() == 2);
    assert(test_email::deliveries.size() == 2);
    assert(test_email::deliveries.back().to == "replacement@example.invalid");
    emaillist[0].id = 8; // Missing recipient must remain retryable, not be marked sent.
    tag.clsvalorfloat = 280; tag.clsvalor = 280; alarm.ejecutado_activar_bd = true;
    assert(alarm.evaluarcompleto() == 1);
    assert(!alarm.ejecutado_activar_email && test_email::deliveries.size() == 2);
    emaillist[0].id = 7;
    assert(alarm.evaluarcompleto() == 1);
    assert(alarm.ejecutado_activar_email && test_email::deliveries.size() == 3);
    delete[] emaillist; emaillist = nullptr;
    alarm.ejecutado_activar_email = false;
    assert(alarm.evaluarcompleto() == 1); // Defensive handling of null directory.
    assert(!alarm.ejecutado_activar_email && test_email::deliveries.size() == 3);
    emaillist = new cls_email[1]; emaillist[0].id = 7; emaillist[0].to = "retry@example.invalid";
    GLOBAL_contador_directorio = 1;
    cls_defalarmas alarms[3] = {alarm, alarm, alarm};
    test_email::result = 28;
    const auto before_retry = test_email::deliveries.size();
    for (int i = 0; i < 3; ++i) {
        alarms[i].id = 42 + i;
        assert(alarms[i].evaluarcompleto() == 1);
        assert(!alarms[i].ejecutado_activar_email);
        assert(test_email::deliveries.back().context.find("alarma=" + std::to_string(42 + i)) != std::string::npos);
    }
    assert(test_email::deliveries.size() == before_retry + 3);
    test_email::result = 0;
    for (auto& pending : alarms) {
        assert(pending.evaluarcompleto() == 1 && pending.ejecutado_activar_email);
        assert(pending.evaluarcompleto() == 1); // Accepted notification is not sent again.
    }
    assert(test_email::deliveries.size() == before_retry + 6);
    delete[] emaillist; emaillist = nullptr;
    GLOBAL_contador_directorio = 0;
}
void command_validation_test() {
    auto server = modbus_new_tcp("127.0.0.1", 0);
    assert(server);
    int listener = modbus_tcp_listen(server, 1);
    assert(listener >= 0);
    sockaddr_in address{}; socklen_t length = sizeof(address);
    assert(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length) == 0);
    auto mapping = modbus_mapping_new_start_address(0, 0, 0, 0, 65535, 1, 0, 0);
    assert(mapping);
    cls_cmd command;
    command.id = 123; command.habilitar = true; command.protocolo = "MBTCP";
    command.direccion_ip = "127.0.0.1"; command.puerto = ntohs(address.sin_port);
    command.direccion_esclavo = 255; command.registro = 65535; command.clsvalor = 128;
    assert(command.validar().empty());
    auto reject = [&](cls_cmd invalid) {
        assert(!invalid.validar().empty());
        assert(invalid.ejecutar() == -1);
        assert(command_metrics::transport_trace().attempts == 0);
        assert(command_metrics::transport_trace().error == EINVAL);
        pollfd descriptor{listener, POLLIN, 0};
        assert(poll(&descriptor, 1, 0) == 0); // Validation never creates a connection.
    };
    auto invalid = command;
    invalid.habilitar = false; reject(invalid);
    invalid = command; invalid.registro = -1; reject(invalid);
    invalid = command; invalid.registro = 65536; reject(invalid);
    invalid = command; invalid.clsvalor = -32769; reject(invalid);
    invalid = command; invalid.clsvalor = 65536; reject(invalid);
    invalid = command; invalid.direccion_ip = "not-an-ip"; reject(invalid);
    invalid = command; invalid.puerto = 0; reject(invalid);
    invalid = command; invalid.puerto = 65536; reject(invalid);
    invalid = command; invalid.direccion_esclavo = 256; reject(invalid);
    invalid = command; invalid.protocolo = "SIEMENS_S7"; reject(invalid);
    invalid = command; invalid.configuration_error = "valor: nulo"; reject(invalid);
    invalid = command; invalid.registro = -1;
    assert(invalid.cmdescribir_tcp() == -1); // Direct public helper is guarded too.
    assert(command.cmdescribir_serial() == -1); // Wrong transport must not open a link.

    std::thread peer([&] {
        assert(modbus_tcp_accept(server, &listener) >= 0);
        for (int expected : {128, 65535, 32768}) {
            uint8_t request[MODBUS_TCP_MAX_ADU_LENGTH];
            const auto received = modbus_receive(server, request);
            assert(received == 12 && request[6] == 255 && request[7] == 6);
            assert(modbus_reply(server, request, received, mapping) > 0);
            assert(mapping->tab_registers[0] == expected);
        }
    });
    // Preserve explicit alarm CMD usage even when excluded from manually triggered batches.
    command.hab_ejecutar = false;
    for (int value : {128, -1, -32768}) {
        command.clsvalor = value;
        assert(command.ejecutar() == 0);
        assert(command_metrics::transport_trace().attempts == 1);
    }
    peer.join(); ModbusLease::clear(); modbus_close(server); close(listener);
    modbus_mapping_free(mapping); modbus_free(server);

    cls_cmd rtu;
    rtu.id = 124; rtu.habilitar = true; rtu.protocolo = "MBSERIAL";
    rtu.interface1 = "/dev/fixture"; rtu.velocidad = 9600; rtu.bitsparada = 1;
    rtu.direccion_esclavo = 247;
    assert(rtu.validar().empty());
    rtu.paridad = "n"; assert(rtu.validar().empty());
    rtu.direccion_esclavo = 0; assert(rtu.validar().empty()); // Legacy normalization to 1.
    rtu.direccion_esclavo = 248; assert(!rtu.validar().empty());
    rtu.direccion_esclavo = 1; rtu.paridad = "X"; assert(!rtu.validar().empty());
    rtu.paridad = "N"; rtu.bitsparada = 3; assert(!rtu.validar().empty());
    rtu.bitsparada = 1; rtu.velocidad = 0; assert(!rtu.validar().empty());
    rtu.velocidad = 9600; rtu.interface1.clear(); assert(!rtu.validar().empty());

    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
    rtu.interface1 = ptsname(master); rtu.paridad = "n";
    rtu.direccion_esclavo = 0; rtu.clsvalor = -32768;
    server = modbus_new_rtu(rtu.interface1.c_str(), 9600, 'N', 8, 1);
    assert(server && modbus_set_socket(server, master) == 0);
    assert(modbus_set_slave(server, 1) == 0);
    mapping = modbus_mapping_new(0, 0, 1, 0);
    assert(mapping);
    std::thread serial_peer([&] {
        uint8_t request[MODBUS_RTU_MAX_ADU_LENGTH];
        const auto received = modbus_receive(server, request);
        assert(received > 0 && request[0] == 1 && request[1] == 6);
        assert(modbus_reply(server, request, received, mapping) > 0);
        assert(mapping->tab_registers[0] == 32768);
    });
    assert(rtu.ejecutar() == 0 && rtu.direccion_esclavo == 1);
    assert(command_metrics::transport_trace().attempts == 1);
    serial_peer.join(); ModbusLease::clear();
    modbus_close(server); modbus_mapping_free(mapping); modbus_free(server);
}

void invalid_alarm_command_email_test() {
    cls_equipos tag;
    tag.nombre = "meter"; tag.tag = "voltage";
    tag.clsvalorfloat = 280; tag.clsvalor = 280; tag.clsvalorstr = "280";
    tag.resultado_leer = true; tag.ultimo_muestreo = time(nullptr); tag.unidad = "V";
    cls_cmd disabled;
    disabled.id = 404; // Device disabled: no FC06, but email must still be attempted.
    cmd = &disabled; GLOBAL_contador_cmd = 1;
    cls_email recipient;
    recipient.id = 7; recipient.to = "alarm-validation@example.invalid";
    emaillist = &recipient; GLOBAL_contador_directorio = 1;
    test_email::deliveries.clear(); test_email::result = 0;
    for (int id : {404, 999}) {
        cls_defalarmas alarm;
        const auto action = "CMD[" + std::to_string(id) + "];EMAIL[7];";
        alarm.clsdispositivo = "meter"; alarm.clstag = alarm.clsvalorAlarma = "voltage";
        alarm.clsequipo_mbtcp = &tag; alarm.clscantidad_equipo_mbtcp = 1;
        alarm.expresionactivar = "$(meter.voltage)>250";
        alarm.expresiondesactivar = "$(meter.voltage)<240";
        alarm.accionactivar = action; alarm.ejecutado_activar_bd = true;
        assert(alarm.evaluarcompleto() == 1);
        assert(!alarm.ejecutado_activar_cmd && alarm.ejecutado_activar_email);
        assert(alarm.evaluarcompleto() == 1);
    }
    assert(test_email::deliveries.size() == 2); // Each accepted email is sent only once.
    cmd = nullptr; GLOBAL_contador_cmd = 0;
    emaillist = nullptr; GLOBAL_contador_directorio = 0;
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
    serial_test(); serial_window_test(); directory_lookup_test(); batch_period_test(); event_lookup_test(); alarm_directory_test(); command_validation_test(); invalid_alarm_command_email_test();
    cls_sms disabled_sms; assert(disabled_sms.enviar() == CURLE_FAILED_INIT);
    if (std::getenv("DRIVER_SQL_TEST")) { generic_sql_test(); replication_test(); command_sql_test(); }
}
