#include "test_harness.h"

#include <string_view>

int main(int argc, char** argv)
{
    std::string_view filter;
    if (argc >= 2 && argv[1] != nullptr) {
        filter = argv[1];
    }

    return ::marionette::tests::RunAllTests(filter);
}
