#include "beowulf_model.h"

namespace dragongod_samples::ariadne_beowulf
{
    [[nodiscard]] BeowulfModelState BuildInitialModelState()
    {
        BeowulfModelState state{};
        state.sceneIndex = 0;
        state.memoryTokenCount = 0;
        state.rollbackCheckpointReady = false;
        return state;
    }

    [[nodiscard]] bool IsScaffoldStateShapeValid(const BeowulfModelState& state)
    {
        if (state.sceneIndex < 0) {
            return false;
        }

        if (state.memoryTokenCount < 0) {
            return false;
        }

        return true;
    }
}
