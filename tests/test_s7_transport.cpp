// Transporte real contra servidor Snap7 local: sesión, errores y recuperación.
#include "s7_transport.h"
#include "s7_fixture.h"
#include <cassert>
#include <thread>
#include <iostream>
int main() {
    S7Fixture fixture;
    s7::Endpoint endpoint;
    endpoint.host = "127.0.0.1";
    endpoint.port = fixture.port;
    endpoint.timeout_ms = 150;
    s7::Transport client(endpoint);
    assert(client.read(0, 0, 1).kind == s7::ErrorKind::Configuration);
    assert(client.stats().attempts == 0);
    auto first = client.read(100, 0, 1024);
    assert(first.ok() && first.bytes.size() == 1024);
    for (int i = 0; i < 1024; ++i)
        assert(first.bytes[i] == static_cast<byte>(i));
    assert(client.stats().pdu >= 240);
    assert(client.read(100, 10, 4).ok());
    assert(client.stats().connections == 1);
    assert(client.read(101, 0, 2).kind == s7::ErrorKind::Remote);
    auto invalid = client.read(100, 4095, 2);
    assert(invalid.kind == s7::ErrorKind::Remote && invalid.bytes.empty());
    assert(client.read(100, 0, 2).ok() && client.stats().connections == 1);
    std::thread a([&] {
        for (int i = 0; i < 10; ++i)
            assert(client.read(100, 0, 2).ok());
    });
    std::thread b([&] {
        for (int i = 0; i < 10; ++i)
            assert(client.read(100, 2, 2).ok());
    });
    a.join();
    b.join();
    fixture.server.Stop();
    auto stopped = client.read(100, 0, 2);
    assert(stopped.kind == s7::ErrorKind::Transport && stopped.bytes.empty());
    auto attempts = client.stats().attempts;
    assert(client.read(100, 0, 2).kind == s7::ErrorKind::Retry);
    assert(client.stats().attempts == attempts);
    assert(fixture.server.StartTo("127.0.0.1") == 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(client.read(100, 0, 2).ok());
    assert(client.stats().connections == 2);
    client.disconnect();
    assert(client.read(100, 0, 2).ok() && client.stats().connections == 3);
    // A TCP peer that accepts but never replies must not block indefinitely.
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    assert(listener >= 0);
    sockaddr_in silent_address{};
    silent_address.sin_family = AF_INET;
    silent_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(listener, reinterpret_cast<sockaddr *>(&silent_address), sizeof(silent_address)) ==
           0);
    socklen_t length = sizeof(silent_address);
    assert(getsockname(listener, reinterpret_cast<sockaddr *>(&silent_address), &length) == 0);
    assert(listen(listener, 1) == 0);
    endpoint.port = ntohs(silent_address.sin_port);
    std::thread silent([&] {
        int peer = accept(listener, nullptr, nullptr);
        if (peer >= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            close(peer);
        }
    });
    s7::Transport timed(endpoint);
    auto began = std::chrono::steady_clock::now();
    assert(timed.read(100, 0, 2).kind == s7::ErrorKind::Transport);
    assert(std::chrono::steady_clock::now() - began < std::chrono::seconds(2));
    silent.join();
    close(listener);
    endpoint.host = "hostname.invalid";
    bool rejected = false;
    try {
        s7::Transport bad(endpoint);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    assert(rejected);
    std::cout
        << "PASS S7 local DB read, large read, reuse, remote errors, concurrency and reconnect\n";
}
