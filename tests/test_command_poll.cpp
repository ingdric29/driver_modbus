// Planificación de órdenes: igualdad, fracciones, orígenes y recuperación sin ráfagas.
#include "command_poll.h"
#include <cassert>
int main() {
    using Schedule = CommandPollSchedule;
    const auto origin = Schedule::Time{} + std::chrono::hours(1);
    Schedule schedule(5, 2, origin);
    int local = 0, external = 0;
    for (int milliseconds = 0; milliseconds <= 10000; milliseconds += 10) {
        const auto now = origin + std::chrono::milliseconds(milliseconds);
        if (schedule.local_due(now)) {
            ++local;
            schedule.local_finished(now);
        }
        if (schedule.external_due(true, now)) {
            ++external;
            schedule.external_finished(now);
        }
        assert(schedule.next(true) > now);
    }
    assert(local == 3 && external == 6); // Includes t=5s exactly, not t=10s only.

    Schedule disabled(5, .01, origin);
    assert(!disabled.external_due(false, origin));
    disabled.local_finished(origin);
    assert(disabled.next(false) == origin + std::chrono::seconds(5));
    // Re-enable: one immediate external poll, followed by a future deadline.
    const auto recovered = origin + std::chrono::seconds(11);
    assert(disabled.external_due(true, recovered));
    disabled.external_finished(recovered);
    assert(!disabled.external_due(true, recovered));

    Schedule fractions(.2, .37, origin);
    local = external = 0;
    for (int milliseconds = 0; milliseconds <= 2000; ++milliseconds) {
        const auto now = origin + std::chrono::milliseconds(milliseconds);
        if (fractions.local_due(now)) {
            ++local;
            // A 7ms query must not add 7ms to every subsequent period.
            fractions.local_finished(now + std::chrono::milliseconds(7));
        }
        if (fractions.external_due(true, now)) {
            ++external;
            fractions.external_finished(now);
        }
    }
    assert(local == 11 && external == 6);

    Schedule delayed(.2, .37, origin);
    delayed.local_finished(origin + std::chrono::milliseconds(955));
    assert(delayed.next_local() == origin + std::chrono::seconds(1));
    assert(!delayed.local_due(origin + std::chrono::milliseconds(999)));
    assert(delayed.local_due(origin + std::chrono::seconds(1)));
}
