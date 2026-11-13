#ifndef DRIVER_PERIODIC_DEADLINE_H
#define DRIVER_PERIODIC_DEADLINE_H
#include <chrono>
#include <cmath>
#include <stdexcept>

// Wall time belongs to stored timestamps; elapsed time belongs to this clock.
class PeriodicDeadline {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
private:
    Time next_{};
public:
    static Clock::duration interval(double seconds) {
        if (!std::isfinite(seconds) || seconds <= 0 || seconds > 31536000)
            throw std::invalid_argument("period must be finite and in (0,31536000] seconds");
        auto value = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        return value > Clock::duration::zero() ? value : Clock::duration(1);
    }
    bool due(Time now = Clock::now()) const { return now >= next_; }
    Time next() const { return next_; }
    void defer(double seconds, Time now = Clock::now()) { next_ = now + interval(seconds); }
    void advance(double seconds, Time now = Clock::now()) {
        auto period = interval(seconds);
        if (next_ == Time{}) next_ = now + period;
        else if (now >= next_) next_ += period * ((now - next_) / period + 1);
    }
};
#endif
