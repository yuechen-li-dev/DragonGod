#include "beowulf_model.h"

namespace dragongod_samples::ariadne_beowulf
{
    [[nodiscard]] BeowulfState BuildInitialBeowulfState()
    {
        return BeowulfState{
            .scene = BeowulfScene::BarrowApproach,
            .tone = BeowulfTone::Unset,
            .loyaltyPressure = BeowulfLoyaltyPressure::Unset,
            .resolve = 0,
            .preBattleChoiceConsumed = false,
            .firstClashBegun = false,
            .retainersFled = false,
            .wiglafRemains = false,
            .collapseSpoken = false,
            .wiglafSpoken = false
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

        if (state.preBattleChoiceConsumed && state.loyaltyPressure == BeowulfLoyaltyPressure::Unset) {
            return false;
        }

        if (state.firstClashBegun && state.scene == BeowulfScene::BarrowApproach) {
            return false;
        }

        if (state.retainersFled && !state.firstClashBegun) {
            return false;
        }

        if (state.wiglafRemains && !state.retainersFled) {
            return false;
        }

        if (state.wiglafSpoken && !state.wiglafRemains) {
            return false;
        }

        if (state.collapseSpoken && !state.retainersFled) {
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

    [[nodiscard]] std::optional<BeowulfLoyaltyPressure> TryLoyaltyPressureFromTone(const BeowulfTone tone)
    {
        if (tone == BeowulfTone::Proud) {
            return BeowulfLoyaltyPressure::BoastShattered;
        }

        if (tone == BeowulfTone::Grim) {
            return BeowulfLoyaltyPressure::DoomWeight;
        }

        if (tone == BeowulfTone::Kingly) {
            return BeowulfLoyaltyPressure::OathBurden;
        }

        return std::nullopt;
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

        if (scene == BeowulfScene::RetainerCollapse) {
            if (tone == BeowulfTone::Proud) {
                return std::vector<std::string>{
                    "Boast-smoke thins; their courage thins with it.",
                    "Heat drives them from the shield-wall.",
                    "Only one shadow does not break."
                };
            }

            if (tone == BeowulfTone::Grim) {
                return std::vector<std::string>{
                    "He named doom true; now fear proves him right.",
                    "Mail turns and runs from the furnace-breath.",
                    "One young spear still faces fire."
                };
            }

            return std::vector<std::string>{
                "The oath-ring trembles in the dragon wind.",
                "Thanes break rank and flee the blaze.",
                "One kinsman keeps his place."
            };
        }

        if (scene == BeowulfScene::WiglafRemains) {
            return std::vector<std::string>{
                "Wiglaf steps through ash to his lord.",
                "Loyalty stands where numbers failed.",
                "Two blades answer one doom."
            };
        }

        if (scene == BeowulfScene::TragicHandoff) {
            return std::vector<std::string>{
                "The fight narrows to king and heir of courage.",
                "Flame and fate draw close together.",
                "Night leans toward its grievous turn."
            };
        }

        return std::vector<std::string>{
            "The beat is done. Ash settles."
        };
    }
}
