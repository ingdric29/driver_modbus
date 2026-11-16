// Archivo: test_monitor.cpp
// Descripción: Comprueba snapshots, antigüedad, recuperación y aislamiento del socket del monitor.
// Necesita: Compilación del runner correspondiente y datos aislados de prueba.
// Retornos: 0 al completar las comprobaciones; un error o aserción hace fallar la prueba.

#include "monitor_state.h"
#include "monitor_server.h"
#include <cassert>
#include <atomic>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
using namespace monitor;
namespace {
int connect_to(const std::string& path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0); assert(fd >= 0);
    timeval timeout{2, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    sockaddr_un addr{}; addr.sun_family = AF_UNIX; assert(path.size() < sizeof(addr.sun_path));
    std::strcpy(addr.sun_path, path.c_str());
    assert(connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    return fd;
}
std::string receive(const std::string& path) {
    int fd = connect_to(path); char buffer[16384]; std::string data;
    while (true) { auto n = recv(fd, buffer, sizeof(buffer), 0); if (!n) break; assert(n > 0); data.append(buffer, n); }
    close(fd); assert(data.back() == '\n'); return data;
}
}
int main() {
    const auto now = Clock::now();
    State state;
    state.configure("test", "tcp://localhost:1", "fixture");
    state.begin_catalog();
    Tag fast; fast.device_key = "device"; fast.device = "meter"; fast.name = "voltage"; fast.period = .1;
    auto voltage = state.add_tag(fast);
    Tag slow = fast; slow.name = "temperature"; slow.period = 60;
    auto temperature = state.add_tag(slow);
    assert(state.copy().tags[0].quality(now) == "PENDIENTE");
    state.publish(voltage, 1, "230", 100, 55, now);
    state.publish(temperature, 1, "24", 100, 60, now);
    auto snapshot = state.copy();
    assert(snapshot_json(snapshot, now).find("\"status\":\"ONLINE\"") != std::string::npos);
    assert(snapshot.tags[0].quality(now + std::chrono::seconds(2)) == "ANTIGUO");
    assert(snapshot.tags[1].quality(now + std::chrono::seconds(2)) == "ACTUAL");
    assert(snapshot_json(snapshot, now + std::chrono::seconds(2)).find("DEGRADADO") != std::string::npos);
    state.publish(voltage, -1, "bad", 0, 100, now);
    snapshot = state.copy();
    assert(snapshot.tags[0].text == "230" && snapshot.tags[0].acquired == 100);
    assert(snapshot.tags[0].quality(now) == "ERROR");
    assert(snapshot.tags[0].failures == 1 && snapshot.tags[0].consecutive_failures == 1);
    state.publish(voltage, -2, "bad", 0, 100, now);
    assert(state.copy().tags[0].quality(now) == "REINTENTANDO");
    state.publish(voltage, 1, "231", 101, 55, now);
    assert(state.copy().tags[0].consecutive_failures == 0);
    state.sql("tcp://other", "fixture", true);
    assert(state.copy().db_result == 0);
    state.sql("tcp://localhost:1", "fixture", false);
    assert(state.copy().db_result == -1);
    state.sql("tcp://localhost:1", "fixture", true);
    assert(state.copy().db_result == 1 && state.copy().sql_errors == 1);
    state.add_alarm(7); state.alarm(7, true, true);
    assert(snapshot_json(state.copy(), now).find("\"alarms_active\":1") != std::string::npos);
    state.alarm(7, false, true);
    assert(snapshot_json(state.copy(), now).find("\"alarms_unknown\":1") != std::string::npos);
    for (int i = 0; i < 200; ++i) state.event("INFO: event " + std::to_string(i));
    state.event("INFO: event 199");
    assert(state.copy().events.size() == 128 && state.copy().events.back().repeats == 2);
    state.event("METRICS reads=100"); assert(state.copy().events.size() == 128);
    assert(quote(std::string("\"\\\n\x1b")) == "\"\\\"\\\\\\u000a\\u001b\"");
    assert(quote("\xc3\xb1") == "\"\xc3\xb1\"");
    assert(quote("\xed\xa0\x80\xff") == "\"\\ufffd\\ufffd\\ufffd\\ufffd\"");
    auto old_generation = state.copy().generation;
    state.begin_catalog(); auto new_key = state.add_tag(fast);
    state.publish(voltage, 1, "old generation", 100, 1, now);
    assert(new_key != voltage && state.copy().generation == old_generation + 1);
    assert(state.copy().tags[0].quality(now) == "PENDIENTE");

    char directory[] = "/tmp/driver-monitor.XXXXXX";
    assert(mkdtemp(directory)); std::string dir(directory), path = dir + "/automonitor.sock";
    // Recover a stale socket, reject another server, retain user-only permissions.
    int stale = socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{}; addr.sun_family = AF_UNIX; std::strcpy(addr.sun_path, path.c_str());
    assert(bind(stale, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0); close(stale);
    auto& live = State::instance(); live.begin_catalog();
    for (int i = 0; i < 2500; ++i) { Tag t = fast; t.name = std::to_string(i); live.add_tag(t); }
    const auto key = live.copy().tags.front().key;
    {
        Server server(dir);
        struct stat st{}; assert(lstat(path.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600);
        bool rejected = false; try { Server another(dir); } catch (const std::exception&) { rejected = true; }
        assert(rejected);
        int slow_client = connect_to(path); // Do not consume: it must time out without blocking producers.
        auto start = Clock::now();
        for (int i = 0; i < 10000; ++i) live.publish(key, 1, "10", 10, 1);
        assert(Clock::now() - start < std::chrono::seconds(2));
        auto data = receive(path);
        assert(data.find("\"version\":1") != std::string::npos && data.find("voltage") == std::string::npos);
        close(slow_client);
        std::atomic<bool> run{true};
        std::thread producer([&] { while (run) live.publish(key, 1, "20", 20, 1); });
        for (int i = 0; i < 3; ++i) assert(!receive(path).empty());
        run = false; producer.join();
    }
    assert(access(path.c_str(), F_OK) == -1);
    assert(chmod(dir.c_str(), 0755) == 0);
    bool insecure_rejected = false; try { Server insecure(dir); } catch (const std::exception&) { insecure_rejected = true; }
    assert(insecure_rejected);
    unlink((dir + "/automonitor.lock").c_str()); assert(rmdir(dir.c_str()) == 0);
    std::cout << "PASS monitor: freshness, periods, recovery, reload, SQL, bounded events, socket isolation and slow viewers\n";
}
