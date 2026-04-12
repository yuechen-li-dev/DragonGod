#pragma once

#include "beowulf_model.h"

#include <string>
#include <vector>

namespace dragongod_samples::ariadne_beowulf
{
    struct BeowulfRunRequest
    {
        std::vector<dragongod::Message> mailboxInput;
        int maxSteps = 12;

        [[nodiscard]] bool operator==(const BeowulfRunRequest& other) const = default;
    };

    struct BeowulfRunResult
    {
        BeowulfState finalState{};
        BeowulfOutput output{};
        bool failed = false;
        std::string failureReason;

        [[nodiscard]] bool operator==(const BeowulfRunResult& other) const = default;
    };

    [[nodiscard]] BeowulfRunResult RunBeowulfGoldenPath(const BeowulfRunRequest& request);
}
