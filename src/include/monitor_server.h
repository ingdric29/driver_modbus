#ifndef AUTOMONITOR_MONITOR_SERVER_H
#define AUTOMONITOR_MONITOR_SERVER_H
#include <atomic>
#include <string>
#include <thread>
namespace monitor {
// One-way local snapshot endpoint. There are intentionally no commands.
class Server {
    std::string path_;
    int socket_ = -1, lock_ = -1;
    bool bound_ = false;
    std::atomic<bool> stop_{false};
    std::thread thread_;
    void serve();
    void cleanup() noexcept;
public:
    explicit Server(const std::string& directory);
    ~Server();
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
};
}
#endif
