#include "test_harness.h"

#include <string_view>

int main(int argc, char** argv)
{
    if (argc >= 2 && argv[1] != nullptr) {
        const std::string_view mode = argv[1];
        if (mode == "--bench") {
            std::string_view filter;
            if (argc >= 3 && argv[2] != nullptr) {
                filter = argv[2];
            }

            return ::marionette::tests::RunBenchmarks(filter);
        }
    }

    std::string_view filter;
    if (argc >= 2 && argv[1] != nullptr) {
        filter = argv[1];
    }

    return ::marionette::tests::RunAllTests(filter);
}
