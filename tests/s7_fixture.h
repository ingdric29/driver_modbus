// Servidor Snap7 loopback en puerto libre, sin escrituras de cliente.
#ifndef AUTOMONITOR_TEST_S7_FIXTURE_H
#define AUTOMONITOR_TEST_S7_FIXTURE_H
#include "../third_party/snap7/release/wrappers/c-cpp/snap7.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <vector>
struct S7Fixture {
    std::vector<byte> data;
    TS7Server server;
    word port = 0;
    explicit S7Fixture(size_t size = 4096) : data(size) {
        for (size_t i = 0; i < size; ++i)
            data[i] = static_cast<byte>(i);
        if (server.RegisterArea(srvAreaDB, 100, data.data(), data.size()))
            throw std::runtime_error("fixture DB registration failed");
        bool started = false;
        for (int attempt = 0; attempt < 10 && !started; ++attempt) {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            if (fd < 0)
                throw std::runtime_error("fixture socket failed");
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address))) {
                close(fd);
                throw std::runtime_error("fixture bind failed");
            }
            socklen_t length = sizeof(address);
            if (getsockname(fd, reinterpret_cast<sockaddr *>(&address), &length)) {
                close(fd);
                throw std::runtime_error("fixture getsockname failed");
            }
            port = ntohs(address.sin_port);
            close(fd);
            if (server.SetParam(p_u16_LocalPort, &port))
                throw std::runtime_error("fixture port failed");
            started = server.StartTo("127.0.0.1") == 0;
        }
        if (!started)
            throw std::runtime_error("fixture server failed");
    }
    ~S7Fixture() { server.Stop(); }
};
#endif
