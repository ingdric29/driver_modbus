// Servidores S7/Modbus de laboratorio: comandos stdin solo modifican el fixture.
#include "s7_fixture.h"
#include <modbus.h>
#include <atomic>
#include <thread>
#include <iostream>
#include <poll.h>
#include <cerrno>
#include <mutex>
int main() {
    try {
        S7Fixture fixture;
        S7Fixture second;
        // DB100: BOOL=true, BYTE=255, INT=-123, DINT=-456, REAL=25, DWORD=max.
        byte values[] = {1, 255, 255, 133, 255, 255, 254, 56, 0x41, 0xc8, 0, 0, 255, 255, 255, 255};
        std::copy(values, values + sizeof(values), fixture.data.begin());
        auto *ctx = modbus_new_tcp("127.0.0.1", 0);
        if (!ctx)
            throw std::runtime_error("modbus context");
        int listener = modbus_tcp_listen(ctx, 4);
        if (listener < 0)
            throw std::runtime_error("modbus listen");
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        if (getsockname(listener, reinterpret_cast<sockaddr *>(&address), &length))
            throw std::runtime_error("modbus getsockname");
        auto *mapping = modbus_mapping_new(0, 0, 32, 32);
        if (!mapping)
            throw std::runtime_error("modbus mapping");
        mapping->tab_registers[0] = 1234;
        modbus_set_indication_timeout(ctx, 0, 100000);
        std::mutex mapping_mutex;
        std::atomic<bool> stop{false};
        std::thread mb([&] {
            int peer = -1;
            uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
            while (!stop) {
                if (peer < 0) {
                    pollfd fd{listener, POLLIN, 0};
                    if (poll(&fd, 1, 50) <= 0)
                        continue;
                    peer = modbus_tcp_accept(ctx, &listener);
                    continue;
                }
                int count = modbus_receive(ctx, query);
                if (count > 0) {
                    std::lock_guard<std::mutex> lock(mapping_mutex);
                    if (query[7] == 3 || query[7] == 4 || query[7] == 6)
                        modbus_reply(ctx, query, count, mapping);
                    else
                        modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
                } else if (errno != ETIMEDOUT && errno != EAGAIN) {
                    close(peer);
                    peer = -1;
                }
            }
            if (peer >= 0)
                close(peer);
        });
        std::cout << fixture.port << ' ' << ntohs(address.sin_port) << ' ' << second.port
                  << std::endl;
        char command;
        while (std::cin.get(command)) {
            if (command == 'm') {
                std::lock_guard<std::mutex> lock(mapping_mutex);
                mapping->tab_registers[0] = 1400;
            }
            if (command == 'b' || command == 'B') {
                fixture.server.LockArea(srvAreaDB, 100);
                fixture.data[0] = command == 'B' ? 1 : 0;
                fixture.server.UnlockArea(srvAreaDB, 100);
            }
            if (command == 'h' || command == 'H') {
                fixture.server.LockArea(srvAreaDB, 100);
                fixture.data[12] = 0; fixture.data[13] = command == 'h' ? 1 : 2;
                fixture.data[14] = fixture.data[15] = 0;
                fixture.server.UnlockArea(srvAreaDB, 100);
            }
            if (command == 'l') {
                fixture.server.LockArea(srvAreaDB, 100);
                fixture.data[8] = 0x41; fixture.data[9] = 0xc0;
                fixture.data[10] = fixture.data[11] = 0;
                fixture.server.UnlockArea(srvAreaDB, 100);
            }
            if (command == 'r') {
                fixture.server.LockArea(srvAreaDB, 100);
                fixture.data[8] = 0x41;
                fixture.data[9] = 0xcc;
                fixture.data[10] = 0xcc;
                fixture.data[11] = 0xcd;
                fixture.server.UnlockArea(srvAreaDB, 100);
            }
            if (command == 'v') {
                fixture.server.LockArea(srvAreaDB, 100);
                fixture.data[8] = 0x41;
                fixture.data[9] = 0xd0;
                fixture.data[12] = 0;
                fixture.data[13] = 0;
                fixture.data[14] = 0;
                fixture.data[15] = 42;
                fixture.server.UnlockArea(srvAreaDB, 100);
            }
            if (command == 'q')
                break;
            if (command == 'x')
                fixture.server.Stop();
            if (command == 's')
                fixture.server.StartTo("127.0.0.1");
        }
        stop = true;
        mb.join();
        close(listener);
        modbus_mapping_free(mapping);
        modbus_free(ctx);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
