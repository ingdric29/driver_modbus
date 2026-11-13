#include "alarm_expression.h"
#include <cassert>
int main() {
    auto evaluate = [](const std::string& s) { return alarm_evaluate_postfix(alarm_postfix(s)); };
    assert(evaluate("220>200&25<50") == 1);
    assert(evaluate("-12.5<0") == 1);
    assert(evaluate("!!1") == 1);
    assert(evaluate("!(1|0)") == 0);
    assert(evaluate("3@1=1") == 1);
    assert(evaluate("1e2>99") == 1);
    for (const std::string invalid : {"", "(1", "1)", "1&", "&1", "1 2", "1%2", "()", "1e999>1", "1.2.3"}) {
        bool rejected = false; try { evaluate(invalid); } catch (const std::invalid_argument&) { rejected = true; } assert(rejected);
    }
    bool rejected = false; try { alarm_evaluate_postfix("1 &"); } catch (const std::invalid_argument&) { rejected = true; } assert(rejected);
}
