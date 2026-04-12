#include "beowulf_nodes.h"

#include "beowulf_model.h"

namespace dragongod_samples::ariadne_beowulf
{
    [[nodiscard]] BeowulfScaffoldSmoke RunBeowulfScaffoldSmokePath()
    {
        const BeowulfModelState state = BuildInitialModelState();
        if (!IsScaffoldStateShapeValid(state)) {
            return BeowulfScaffoldSmoke{};
        }

        const dragongod::StackFrameRuntime runtime{};
        const dragongod::FrameRunResult run = runtime.RunForTicks(
            dragongod::StackScriptScenario::PushPopComplete,
            3);

        return BeowulfScaffoldSmoke{
            .outcome = run.finalOutcome,
            .ticksRan = run.tickTrace.size()
        };
    }
}
