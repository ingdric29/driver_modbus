#ifndef DRIVER_WORKER_GROUP_H
#define DRIVER_WORKER_GROUP_H
#include <atomic>
#include <thread>
#include <vector>
#include <utility>

// Owns every worker in a configuration generation. Destruction also handles
// partial startup: signal stop before joining, never detach a live worker.
class WorkerGroup {
    std::atomic<bool>& running_;
    std::vector<std::thread> threads_;
public:
    explicit WorkerGroup(std::atomic<bool>& running) : running_(running) {}
    WorkerGroup(const WorkerGroup&) = delete;
    WorkerGroup& operator=(const WorkerGroup&) = delete;
    template<class F, class... Args> void start(F&& f, Args&&... args) {
        threads_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...);
    }
    void join() {
        for (auto& thread : threads_) if (thread.joinable()) thread.join();
    }
    ~WorkerGroup() { running_ = false; join(); }
};
#endif
