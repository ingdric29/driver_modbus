#ifndef DRIVER_PENDING_BATCH_H
#define DRIVER_PENDING_BATCH_H
#include "sample_window.h"
#include <utility>

// One fixed-size window (<=3 samples). Confirm only after a positive database
// acknowledgement; an error never discards the remaining suffix.
class PendingBatch {
    std::vector<SampleValue> samples_;
    size_t confirmed_ = 0;
public:
    PendingBatch() = default;
    explicit PendingBatch(std::vector<SampleValue> samples) : samples_(std::move(samples)) {}
    bool empty() const { return confirmed_ == samples_.size(); }
    template<class Store> bool flush(Store&& store) {
        while (!empty()) {
            if (!store(samples_[confirmed_])) return false;
            ++confirmed_;
        }
        return true;
    }
};
#endif
