#pragma once

#include "../../src/DragonGod/runtime.h"

#include <optional>
#include <string>
#include <vector>

namespace dragongod_samples::ariadne_beowulf
{
    enum class BeowulfScene
    {
        BarrowApproach = 0,
        BeforeBattleChoice = 1,
        FirstClash = 2,
        Completed = 3
    };

    enum class BeowulfTone
    {
        Unset = 0,
        Proud = 1,
        Grim = 2,
        Kingly = 3
    };

    enum class BeowulfChoiceId
    {
        SpeakProudly = 1,
        SpeakGrimly = 2,
        SpeakAsKing = 3
    };

    struct BeowulfChoice
    {
        BeowulfChoiceId id = BeowulfChoiceId::SpeakProudly;
        std::string label;

        [[nodiscard]] bool operator==(const BeowulfChoice& other) const = default;
    };

    struct BeowulfState
    {
        BeowulfScene scene = BeowulfScene::BarrowApproach;
        BeowulfTone tone = BeowulfTone::Unset;
        int resolve = 0;
        bool preBattleChoiceConsumed = false;
        bool firstClashBegun = false;

        [[nodiscard]] bool operator==(const BeowulfState& other) const = default;
    };

    struct BeowulfOutput
    {
        BeowulfScene scene = BeowulfScene::BarrowApproach;
        std::vector<std::string> sceneLines;
        std::vector<BeowulfChoice> choices;
        std::vector<std::string> trace;
        bool awaitingChoice = false;
        bool advanced = false;

        [[nodiscard]] bool operator==(const BeowulfOutput& other) const = default;
    };

    [[nodiscard]] BeowulfState BuildInitialBeowulfState();
    [[nodiscard]] bool IsBeowulfStateValid(const BeowulfState& state);
    [[nodiscard]] std::vector<BeowulfChoice> BuildPreBattleChoices();
    [[nodiscard]] std::optional<BeowulfTone> TryToneFromChoiceId(BeowulfChoiceId choice);
    [[nodiscard]] std::optional<BeowulfTone> TryToneFromMessage(const dragongod::Message& message);
    [[nodiscard]] std::vector<std::string> BuildSceneLines(BeowulfScene scene, BeowulfTone tone);
}
