// test_main.cpp - runs every registered test case and reports the result.
#include "test_main.h"

// Runs all tests; exit code 1 when any check failed.
int main() {
    for (const auto& c : testing::cases()) {
        const int before = testing::failures();
        std::cerr << "[ RUN  ] " << c.name << "\n";
        c.fn();
        std::cerr << (testing::failures() == before ? "[  OK  ] " : "[ FAIL ] ") << c.name << "\n";
    }
    std::cerr << "\n" << testing::cases().size() << " tests, " << testing::failures() << " failed checks\n";
    return testing::failures() == 0 ? 0 : 1;
}
