#pragma once

#include "../../src/DragonGod/runtime.h"

namespace dragongod_samples::ariadne_beowulf
{
    struct BeowulfScaffoldSmoke
    {
        dragongod::StackRunOutcome outcome = dragongod::StackRunOutcome::Continue;
        dragongod::TickIndex ticksRan = 0;

        [[nodiscard]] bool operator==(const BeowulfScaffoldSmoke& other) const = default;
    };

    [[nodiscard]] BeowulfScaffoldSmoke RunBeowulfScaffoldSmokePath();
}
