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
        RetainerCollapse = 3,
        WiglafRemains = 4,
        TragicHandoff = 5,
        BeowulfFalls = 6,
        LastWords = 7,
        LegacyEnding = 8,
        Completed = 9
    };

    enum class BeowulfTone
    {
        Unset = 0,
        Proud = 1,
        Grim = 2,
        Kingly = 3
    };

    enum class BeowulfLoyaltyPressure
    {
        Unset = 0,
        BoastShattered = 1,
        DoomWeight = 2,
        OathBurden = 3
    };

    enum class BeowulfLegacyTone
    {
        Unset = 0,
        LonelyAsh = 1,
        DoomEmber = 2,
        OathKept = 3
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
        BeowulfLoyaltyPressure loyaltyPressure = BeowulfLoyaltyPressure::Unset;
        int resolve = 0;
        bool preBattleChoiceConsumed = false;
        bool firstClashBegun = false;
        bool retainersFled = false;
        bool wiglafRemains = false;
        bool collapseSpoken = false;
        bool wiglafSpoken = false;
        bool beowulfFallen = false;
        bool lastWordsSpoken = false;
        bool endingCompleted = false;
        BeowulfLegacyTone legacyTone = BeowulfLegacyTone::Unset;

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
    [[nodiscard]] std::optional<BeowulfLoyaltyPressure> TryLoyaltyPressureFromTone(BeowulfTone tone);
    [[nodiscard]] std::optional<BeowulfLegacyTone> TryLegacyToneFromState(BeowulfTone tone, BeowulfLoyaltyPressure pressure);
    [[nodiscard]] std::vector<std::string> BuildSceneLines(BeowulfScene scene, BeowulfTone tone, BeowulfLegacyTone legacyTone);
}
