#include "fake_email.h"
namespace test_email {
std::vector<stdestino> deliveries;
int result = 0;
}
// Tests replace the transport at link time. No SMTP connection is made.
int enviar_email(storigen, stdestino destination) {
    test_email::deliveries.push_back(destination);
    return test_email::result;
}
