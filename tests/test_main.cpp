#include <iostream>

#include "MiniTest.hpp"

int main() {
    int ran = 0;
    for (const auto &test : minitest::registry()) {
        const int before = minitest::failures;
        std::cout << "[ RUN  ] " << test.name << '\n';
        test.fn();
        ran++;
        if (minitest::failures == before) {
            std::cout << "[  OK  ] " << test.name << '\n';
        } else {
            std::cout << "[ FAIL ] " << test.name << '\n';
        }
    }
    std::cout << "\n" << ran << " test(s) run, " << minitest::failures
              << " failure(s).\n";
    return minitest::failures == 0 ? 0 : 1;
}
