// Archivo: test_modbus_transport.cpp
// Descripción: Comprueba lectura y escritura TCP/RTU en servidores locales y pseudoterminales.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "modbus_transport.h"
#include <cassert>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
void tcp_test() {
    modbus_t* server = modbus_new_tcp("127.0.0.1", 0);
    assert(server);
    int listener = modbus_tcp_listen(server, 1);
    assert(listener >= 0);
    sockaddr_in address{}; socklen_t size = sizeof(address);
    assert(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
    auto mapping = modbus_mapping_new(0, 0, 4, 0);
    assert(mapping); mapping->tab_registers[0] = 220;
    std::thread peer([&] {
        assert(modbus_tcp_accept(server, &listener) >= 0);
        for (int i = 0; i < 3; ++i) {
            uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
            const int received = modbus_receive(server, query);
            assert(received > 0);
            assert(modbus_reply(server, query, received, mapping) > 0);
        }
    });
    for (int i = 0; i < 3; ++i) {
        auto lease = ModbusLease::tcp("127.0.0.1", ntohs(address.sin_port));
        assert(lease.connect(200000) == 0);
        if (i == 1) assert(modbus_write_register(lease.get(), 0, 280) == 1);
        else { uint16_t value = 0; assert(modbus_read_registers(lease.get(), 0, 1, &value) == 1); assert(value == (i == 0 ? 220 : 280)); }
    }
    peer.join();
    ModbusLease::clear();
    modbus_close(server); close(listener);
    modbus_mapping_free(mapping); modbus_free(server);
}

void rtu_test() {
    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
    const std::string device = ptsname(master);
    modbus_t* server = modbus_new_rtu(device.c_str(), 9600, 'N', 8, 1);
    assert(server && modbus_set_socket(server, master) == 0);
    assert(modbus_set_slave(server, 1) == 0);
    auto mapping = modbus_mapping_new(0, 0, 4, 0);
    assert(mapping); mapping->tab_registers[0] = 25;
    // Connect client first so the pseudo-terminal slave is in raw mode.
    {
        auto lease = ModbusLease::rtu(device, 9600, 'N', 8, 1);
        assert(lease.connect(200000) == 0);
    }
    std::thread peer([&] {
        for (int i = 0; i < 3; ++i) {
            uint8_t query[MODBUS_RTU_MAX_ADU_LENGTH];
            const int received = modbus_receive(server, query);
            assert(received > 0);
            assert(modbus_reply(server, query, received, mapping) > 0);
        }
    });
    for (int i = 0; i < 3; ++i) {
        auto lease = ModbusLease::rtu(device, 9600, 'N', 8, 1);
        assert(lease.connect(200000) == 0);
        assert(modbus_set_slave(lease.get(), 1) == 0);
        if (i == 1) assert(modbus_write_register(lease.get(), 0, 30) == 1);
        else { uint16_t value = 0; assert(modbus_read_registers(lease.get(), 0, 1, &value) == 1); assert(value == (i == 0 ? 25 : 30)); }
    }
    peer.join();
    ModbusLease::clear();
    modbus_close(server); modbus_mapping_free(mapping); modbus_free(server);
}
int main() { tcp_test(); rtu_test(); }
