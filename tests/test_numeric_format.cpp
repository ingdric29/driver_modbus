#include "conversiones.h"
#include <cassert>
#include <limits>
#include <stdexcept>
int main() {
    assert(cast_double_to_string(12.5, 2) == "12.50");
    assert(cast_float_to_string(-12.5f) == "-12.500000");
    assert(cast_double_to_string(1e300, 6).size() > 300);
    assert(cast_float_to_string(std::numeric_limits<float>::max()).size() > 40);
    bool rejected = false;
    try { cast_double_to_string(1, -1); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    rejected = false;
    try { cast_double_to_string(1, 101); } catch (const std::invalid_argument&) { rejected = true; }
    assert(rejected);
    assert(cast_datetime_to_time("invalid") == time_t(-1));
    assert(cast_datetime_to_time("2026-02-30 10:20:30") == time_t(-1));
    assert(cast_datetime_to_time("2026-11-13 25:20:30") == time_t(-1));
    assert(cast_time_to_datetime(cast_datetime_to_time("2026-11-13 10:20:30")) == "2026-11-13 10:20:30");
    std::cout << "10 numeric/date formatting checks passed\n";
}
