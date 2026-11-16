#ifndef DRIVER_RETRY_BACKOFF_H
#define DRIVER_RETRY_BACKOFF_H
#include <chrono>
#include <algorithm>
class RetryBackoff {
    using Clock = std::chrono::steady_clock;
    Clock::time_point next_{};
    unsigned failures_ = 0;

  public:
    bool ready(Clock::time_point now = Clock::now()) const { return now >= next_; }
    void failed(Clock::time_point now = Clock::now()) {
        const unsigned milliseconds = std::min(30000u, 100u << std::min(failures_, 9u));
        failures_ = std::min(failures_ + 1, 10u);
        next_ = now + std::chrono::milliseconds(milliseconds);
    }
    void succeeded() {
        failures_ = 0;
        next_ = Clock::time_point{};
    }
};
#endif
