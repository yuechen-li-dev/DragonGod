#include "m1_single_frame.h"

namespace dragongod
{
    [[nodiscard]] FrameRunResult SingleFrameRuntime::RunForTicks(RuntimeState initialState, TickIndex tickCount) const
    {
        FrameRunResult result;

        for (TickIndex tick = 0; tick < tickCount; ++tick) {
            result.trace.push_back(FrameTraceEvent{
                .tick = tick,
                .kind = FrameTraceKind::Tick,
                .frameStep = initialState.frame.frameStep,
                .outcome = FrameStepOutcome::Continue
            });

            if (!initialState.frame.entered) {
                initialState.frame.entered = true;
                result.trace.push_back(FrameTraceEvent{
                    .tick = tick,
                    .kind = FrameTraceKind::Enter,
                    .frameStep = initialState.frame.frameStep,
                    .outcome = FrameStepOutcome::Continue
                });
            }

            const FrameStepOutcome outcome = StepFrame(initialState);
            result.trace.push_back(FrameTraceEvent{
                .tick = tick,
                .kind = FrameTraceKind::Step,
                .frameStep = initialState.frame.frameStep,
                .outcome = outcome
            });
            result.finalOutcome = outcome;

            if (outcome == FrameStepOutcome::Completed) {
                result.trace.push_back(FrameTraceEvent{
                    .tick = tick,
                    .kind = FrameTraceKind::ExitCompleted,
                    .frameStep = initialState.frame.frameStep,
                    .outcome = outcome
                });
            }

            if (outcome == FrameStepOutcome::Failed) {
                result.trace.push_back(FrameTraceEvent{
                    .tick = tick,
                    .kind = FrameTraceKind::ExitFailed,
                    .frameStep = initialState.frame.frameStep,
                    .outcome = outcome
                });
            }

            if (IsTerminal(outcome)) {
                break;
            }
        }

        return result;
    }

    [[nodiscard]] FrameStepOutcome SingleFrameRuntime::StepFrame(RuntimeState& state)
    {
        if (state.scenario == FrameScriptScenario::EnterWaitThenComplete) {
            if (!state.waitConsumed && state.frame.frameStep == state.waitAtStep) {
                state.waitConsumed = true;
                return FrameStepOutcome::Wait;
            }

            ++state.frame.frameStep;
            if (state.frame.frameStep >= state.completeAtStep) {
                return FrameStepOutcome::Completed;
            }

            return FrameStepOutcome::Continue;
        }

        ++state.frame.frameStep;
        if (state.frame.frameStep >= state.failAtStep) {
            return FrameStepOutcome::Failed;
        }

        return FrameStepOutcome::Continue;
    }

    [[nodiscard]] bool SingleFrameRuntime::IsTerminal(FrameStepOutcome outcome)
    {
        return outcome == FrameStepOutcome::Completed || outcome == FrameStepOutcome::Failed;
    }
}
