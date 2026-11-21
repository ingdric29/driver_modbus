/*
 * Archivo: command_poll.h
 * Descripción: Planifica sondeos local/externo con plazos independientes.
 * Necesita: Períodos válidos y finalización después de cada operación.
 * Retornos: Vencimientos monotónicos; omite intervalos perdidos sin emitir ráfagas.
 */
#ifndef DRIVER_COMMAND_POLL_H
#define DRIVER_COMMAND_POLL_H
#include "periodic_deadline.h"
#include <algorithm>

class CommandPollSchedule {
    double local_seconds_, external_seconds_;
    PeriodicDeadline local_, external_;

  public:
    using Clock = PeriodicDeadline::Clock;
    using Time = PeriodicDeadline::Time;
    CommandPollSchedule(double local_seconds, double external_seconds, Time now = Clock::now())
        : local_seconds_(local_seconds), external_seconds_(external_seconds) {
        // First poll is immediately due, including after a catalog generation reload.
        local_.defer(local_seconds_, now - PeriodicDeadline::interval(local_seconds_));
        external_.defer(external_seconds_, now - PeriodicDeadline::interval(external_seconds_));
    }
    bool local_due(Time now = Clock::now()) const { return local_.due(now); }
    bool external_due(bool enabled, Time now = Clock::now()) const {
        return enabled && external_.due(now);
    }
    void local_finished(Time now = Clock::now()) { local_.advance(local_seconds_, now); }
    void external_finished(Time now = Clock::now()) { external_.advance(external_seconds_, now); }
    Time next_local() const { return local_.next(); }
    Time next_external() const { return external_.next(); }
    Time next(bool external_enabled) const {
        return external_enabled ? std::min(local_.next(), external_.next()) : local_.next();
    }
};
#endif
