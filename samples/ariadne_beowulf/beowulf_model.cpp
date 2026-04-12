#include "beowulf_model.h"

namespace dragongod_samples::ariadne_beowulf
{
    [[nodiscard]] BeowulfState BuildInitialBeowulfState()
    {
        return BeowulfState{
            .scene = BeowulfScene::BarrowApproach,
            .tone = BeowulfTone::Unset,
            .resolve = 0,
            .preBattleChoiceConsumed = false,
            .firstClashBegun = false
        };
    }

    [[nodiscard]] bool IsBeowulfStateValid(const BeowulfState& state)
    {
        if (state.resolve < 0) {
            return false;
        }

        if (state.preBattleChoiceConsumed && state.tone == BeowulfTone::Unset) {
            return false;
        }

        if (state.firstClashBegun && state.scene == BeowulfScene::BarrowApproach) {
            return false;
        }

        return true;
    }

    [[nodiscard]] std::vector<BeowulfChoice> BuildPreBattleChoices()
    {
        return std::vector<BeowulfChoice>{
            BeowulfChoice{ .id = BeowulfChoiceId::SpeakProudly, .label = "Speak proudly" },
            BeowulfChoice{ .id = BeowulfChoiceId::SpeakGrimly, .label = "Speak grimly" },
            BeowulfChoice{ .id = BeowulfChoiceId::SpeakAsKing, .label = "Speak as a king" }
        };
    }

    [[nodiscard]] std::optional<BeowulfTone> TryToneFromChoiceId(const BeowulfChoiceId choice)
    {
        if (choice == BeowulfChoiceId::SpeakProudly) {
            return BeowulfTone::Proud;
        }

        if (choice == BeowulfChoiceId::SpeakGrimly) {
            return BeowulfTone::Grim;
        }

        if (choice == BeowulfChoiceId::SpeakAsKing) {
            return BeowulfTone::Kingly;
        }

        return std::nullopt;
    }

    [[nodiscard]] std::optional<BeowulfTone> TryToneFromMessage(const dragongod::Message& message)
    {
        if (message.kind != dragongod::MessageKind::Signal) {
            return std::nullopt;
        }

        return TryToneFromChoiceId(static_cast<BeowulfChoiceId>(message.value));
    }

    [[nodiscard]] std::vector<std::string> BuildSceneLines(const BeowulfScene scene, const BeowulfTone tone)
    {
        if (scene == BeowulfScene::BarrowApproach) {
            return std::vector<std::string>{
                "Old king under iron sky.",
                "The barrow keeps a worm of fire.",
                "Fate walks with him up the stones."
            };
        }

        if (scene == BeowulfScene::BeforeBattleChoice) {
            return std::vector<std::string>{
                "Shield-rim rings in the wind.",
                "He gives his last words before flame."
            };
        }

        if (scene == BeowulfScene::FirstClash) {
            if (tone == BeowulfTone::Proud) {
                return std::vector<std::string>{
                    "He boasts against the barrow-mouth.",
                    "Fire answers. Steel goes bright.",
                    "The first clash breaks the night."
                };
            }

            if (tone == BeowulfTone::Grim) {
                return std::vector<std::string>{
                    "He names doom and does not turn.",
                    "Fire answers. The hill is red.",
                    "The first clash opens like a wound."
                };
            }

            return std::vector<std::string>{
                "He speaks as ring-giver to his men.",
                "Fire answers. The old guard tightens.",
                "The first clash begins at king's command."
            };
        }

        return std::vector<std::string>{
            "The beat is done. Ash settles."
        };
    }
}
