#pragma once

namespace dragongod_samples::ariadne_beowulf
{
    struct BeowulfModelState
    {
        int sceneIndex = 0;
        int memoryTokenCount = 0;
        bool rollbackCheckpointReady = false;

        [[nodiscard]] bool operator==(const BeowulfModelState& other) const = default;
    };

    [[nodiscard]] BeowulfModelState BuildInitialModelState();
    [[nodiscard]] bool IsScaffoldStateShapeValid(const BeowulfModelState& state);
}
