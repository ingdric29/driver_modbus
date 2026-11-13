#ifndef DRIVER_RUNTIME_CONTROL_H
#define DRIVER_RUNTIME_CONTROL_H
#include <atomic>
#include <chrono>
#include <thread>
#include <algorithm>
#include <csignal>
extern std::atomic<bool> GLOBAL_EJECUTARHILOS;
extern std::atomic<bool> GLOBAL_stop_requested;
inline bool driver_running() { return GLOBAL_EJECUTARHILOS && !GLOBAL_stop_requested; }
inline void driver_signal_handler(int) {
    static_assert(ATOMIC_BOOL_LOCK_FREE == 2, "signal handling requires lock-free bool atomics");
    GLOBAL_stop_requested.store(true, std::memory_order_relaxed);
    GLOBAL_EJECUTARHILOS.store(false, std::memory_order_relaxed);
}
inline void install_driver_signals() { std::signal(SIGINT, driver_signal_handler); std::signal(SIGTERM, driver_signal_handler); }
inline void wait_driver_until(std::chrono::steady_clock::time_point until) {
    while (driver_running()) {
        auto now = std::chrono::steady_clock::now();
        if (now >= until) return;
        std::this_thread::sleep_until(std::min(until, now + std::chrono::milliseconds(20)));
    }
}
inline void wait_driver_seconds(double seconds) {
    if (!(seconds > 0)) seconds = 0.001;
    wait_driver_until(std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds)));
}
#endif
