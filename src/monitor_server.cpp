#include "include/monitor_server.h"
#include "include/monitor_state.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
namespace monitor {
namespace {
void fail(const char* operation) { throw std::runtime_error(std::string("Monitor: ") + operation + ": " + std::strerror(errno)); }
}
Server::Server(const std::string& directory) : path_(directory + "/automonitor.sock") {
    try {
        if (mkdir(directory.c_str(), 0700) < 0 && errno != EEXIST) fail("mkdir");
        struct stat st{};
        if (lstat(directory.c_str(), &st) < 0) fail("lstat");
        if (!S_ISDIR(st.st_mode) || st.st_uid != geteuid() || (st.st_mode & 0077))
            throw std::runtime_error("Monitor: .runtime debe ser un directorio privado del usuario (0700)");
        const std::string lock_path = directory + "/automonitor.lock";
        lock_ = open(lock_path.c_str(), O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (lock_ < 0) fail("open lock");
        if (fstat(lock_, &st) < 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid())
            throw std::runtime_error("Monitor: archivo de bloqueo invalido");
        if (flock(lock_, LOCK_EX | LOCK_NB) < 0)
            throw std::runtime_error("Monitor: ya hay un driver ejecutandose en este directorio");
        sockaddr_un address{}; address.sun_family = AF_UNIX;
        if (path_.size() >= sizeof(address.sun_path)) throw std::runtime_error("Monitor: ruta de socket demasiado larga");
        std::memcpy(address.sun_path, path_.c_str(), path_.size() + 1);
        if (lstat(path_.c_str(), &st) == 0) {
            if (!S_ISSOCK(st.st_mode) || st.st_uid != geteuid()) throw std::runtime_error("Monitor: ruta de socket ocupada");
            if (unlink(path_.c_str()) < 0) fail("unlink stale socket");
        } else if (errno != ENOENT) fail("lstat socket");
        socket_ = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (socket_ < 0) fail("socket");
        if (bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) fail("bind");
        bound_ = true;
        if (chmod(path_.c_str(), 0600) < 0) fail("chmod socket");
        if (listen(socket_, 8) < 0) fail("listen");
        thread_ = std::thread(&Server::serve, this);
    } catch (...) { cleanup(); throw; }
}
void Server::cleanup() noexcept {
    if (socket_ >= 0) { close(socket_); socket_ = -1; }
    if (bound_) { unlink(path_.c_str()); bound_ = false; }
    if (lock_ >= 0) { close(lock_); lock_ = -1; }
}
Server::~Server() {
    stop_ = true;
    if (thread_.joinable()) thread_.join();
    cleanup();
}
void Server::serve() {
    // Socket I/O and JSON formatting happen in this thread, outside state locks.
    std::string cached;
    auto cache_time = Clock::time_point::min();
    while (!stop_) {
        pollfd listener{socket_, POLLIN, 0};
        if (poll(&listener, 1, 100) <= 0 || !(listener.revents & POLLIN)) continue;
        int client = accept4(socket_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client < 0) continue;
        try {
            auto now = Clock::now();
            if (cached.empty() || now - cache_time >= std::chrono::seconds(1)) {
                cached = snapshot_json(State::instance().copy(), now); cache_time = now;
                if (cached.size() > 8 * 1024 * 1024) cached = "{\"version\":1,\"error\":\"snapshot demasiado grande\"}\n";
            }
            size_t offset = 0;
            auto deadline = Clock::now() + std::chrono::milliseconds(200);
            while (offset < cached.size() && !stop_ && Clock::now() < deadline) {
                auto sent = send(client, cached.data() + offset, cached.size() - offset, MSG_NOSIGNAL);
                if (sent > 0) { offset += sent; continue; }
                if (sent < 0 && errno == EINTR) continue;
                if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                    pollfd ready{client, POLLOUT, 0}; poll(&ready, 1, 20); continue;
                }
                break;
            }
        } catch (...) { /* A disconnected/slow viewer cannot stop the driver. */ }
        close(client);
    }
}
}
