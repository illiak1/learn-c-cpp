// test_main.h - tiny assertion framework for the unit tests (no external dependency).
#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace testing {

struct Case {
    std::string name;
    std::function<void()> fn;
};

// All registered test cases.
inline std::vector<Case>& cases() {
    static std::vector<Case> c;
    return c;
}

inline int& failures() {
    static int f = 0;
    return f;
}

// Registers a test case at static-initialisation time.
struct Registrar {
    Registrar(const std::string& name, std::function<void()> fn) { cases().push_back({name, std::move(fn)}); }
};

}  // namespace testing

#define TEST(name)                                                         \
    static void test_##name();                                             \
    static testing::Registrar registrar_##name(#name, test_##name);        \
    static void test_##name()

#define CHECK(cond)                                                                            \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n";            \
            ++testing::failures();                                                             \
        }                                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                                         \
    do {                                                                                       \
        const auto va = (a);                                                                   \
        const auto vb = (b);                                                                   \
        if (!(va == vb)) {                                                                     \
            std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b "\n";    \
            std::cerr << "    left:  [" << va << "]\n    right: [" << vb << "]\n";              \
            ++testing::failures();                                                             \
        }                                                                                      \
    } while (0)
