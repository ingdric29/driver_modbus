#ifndef DRIVER_WORKER_GROUP_H
#define DRIVER_WORKER_GROUP_H
#include <atomic>
#include <thread>
#include <vector>
#include <utility>
#include <mutex>
#include <exception>
#include <functional>
#include <memory>

// Owns every worker in a configuration generation. Destruction also handles
// partial startup: signal stop before joining, never detach a live worker.
class WorkerGroup {
    std::atomic<bool> &running_;
    std::vector<std::thread> threads_;
    std::mutex failure_mutex_;
    std::exception_ptr failure_;
    void join_all() {
        for (auto &thread : threads_)
            if (thread.joinable())
                thread.join();
    }

  public:
    explicit WorkerGroup(std::atomic<bool> &running) : running_(running) {}
    WorkerGroup(const WorkerGroup &) = delete;
    WorkerGroup &operator=(const WorkerGroup &) = delete;
    template <class F, class... Args> void start(F &&f, Args &&...args) {
        auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        auto owned = std::make_shared<decltype(task)>(std::move(task));
        threads_.emplace_back([this, owned] {
            try {
                (*owned)();
            } catch (...) {
                std::lock_guard<std::mutex> lock(failure_mutex_);
                if (!failure_)
                    failure_ = std::current_exception();
                running_ = false;
            }
        });
    }
    void join() {
        join_all();
        if (failure_)
            std::rethrow_exception(failure_);
    }
    ~WorkerGroup() {
        running_ = false;
        join_all();
    }
};
#endif
