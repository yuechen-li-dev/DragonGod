#include "m1_single_frame.h"

namespace dragongod
{
    namespace
    {
        [[nodiscard]] FrameKind ScenarioRootFrame(StackScriptScenario scenario)
        {
            if (scenario == StackScriptScenario::PushPopComplete) {
                return FrameKind::RootPushChild;
            }

            if (scenario == StackScriptScenario::ReplaceComplete) {
                return FrameKind::RootReplace;
            }

            if (scenario == StackScriptScenario::WaitPushPopComplete) {
                return FrameKind::RootWaitThenPush;
            }

            return FrameKind::RootPushFailingChild;
        }

        void EmitTrace(
            FrameRunResult& result,
            TickIndex tick,
            FrameTraceKind kind,
            const FrameState& active,
            FrameControl control,
            FrameKind targetFrame,
            std::size_t stackDepth)
        {
            result.trace.push_back(FrameTraceEvent{
                .tick = tick,
                .kind = kind,
                .activeFrame = active.kind,
                .frameStep = active.step,
                .control = control,
                .targetFrame = targetFrame,
                .stackDepth = stackDepth
            });
        }
    }

    [[nodiscard]] FrameRunResult StackFrameRuntime::RunForTicks(RuntimeState initialState, TickIndex tickCount) const
    {
        FrameRunResult result;

        if (initialState.stack.empty()) {
            initialState.stack.push_back(FrameState{ .kind = ScenarioRootFrame(initialState.scenario) });
        }

        for (TickIndex tick = 0; tick < tickCount; ++tick) {
            if (initialState.stack.empty()) {
                result.finalOutcome = StackRunOutcome::Completed;
                break;
            }

            FrameState& frame = initialState.stack.back();
            EmitTrace(result, tick, FrameTraceKind::Tick, frame, FrameControl::Continue, frame.kind, initialState.stack.size());

            if (!frame.entered) {
                frame.entered = true;
                EmitTrace(result, tick, FrameTraceKind::Enter, frame, FrameControl::Continue, frame.kind, initialState.stack.size());
            }

            const FrameControlAction action = StepTopFrame(initialState, frame);

            EmitTrace(result, tick, FrameTraceKind::Step, frame, action.control, action.target, initialState.stack.size());

            if (action.control == FrameControl::Continue) {
                result.finalOutcome = StackRunOutcome::Continue;
                continue;
            }

            if (action.control == FrameControl::Wait) {
                result.finalOutcome = StackRunOutcome::Wait;
                continue;
            }

            if (action.control == FrameControl::Push) {
                EmitTrace(result, tick, FrameTraceKind::Push, frame, action.control, action.target, initialState.stack.size());
                initialState.stack.push_back(FrameState{ .kind = action.target });
                result.finalOutcome = StackRunOutcome::Continue;
                continue;
            }

            if (action.control == FrameControl::Replace) {
                const std::size_t depthBeforeReplace = initialState.stack.size();
                const FrameState replaced = frame;
                EmitTrace(result, tick, FrameTraceKind::Replace, replaced, action.control, action.target, depthBeforeReplace);
                initialState.stack.pop_back();
                initialState.stack.push_back(FrameState{ .kind = action.target });
                result.finalOutcome = StackRunOutcome::Continue;
                continue;
            }

            if (action.control == FrameControl::Pop || action.control == FrameControl::Complete) {
                const std::size_t depthBeforePop = initialState.stack.size();
                const FrameState completedFrame = frame;
                EmitTrace(result, tick, FrameTraceKind::ExitCompleted, completedFrame, action.control, completedFrame.kind, depthBeforePop);
                initialState.stack.pop_back();
                EmitTrace(result, tick, FrameTraceKind::Pop, completedFrame, action.control, completedFrame.kind, depthBeforePop);

                if (initialState.stack.empty()) {
                    result.finalOutcome = StackRunOutcome::Completed;
                    EmitTrace(result, tick, FrameTraceKind::TerminalCompleted, completedFrame, action.control, completedFrame.kind, 0);
                    break;
                }

                result.finalOutcome = StackRunOutcome::Continue;
                continue;
            }

            const std::size_t depthBeforeFail = initialState.stack.size();
            const FrameState failedFrame = frame;
            EmitTrace(result, tick, FrameTraceKind::ExitFailed, failedFrame, action.control, failedFrame.kind, depthBeforeFail);
            result.finalOutcome = StackRunOutcome::Failed;
            EmitTrace(result, tick, FrameTraceKind::TerminalFailed, failedFrame, action.control, failedFrame.kind, depthBeforeFail);
            break;
        }

        return result;
    }

    [[nodiscard]] FrameControlAction StackFrameRuntime::StepTopFrame(const RuntimeState& state, FrameState& frame)
    {
        if (state.scenario == StackScriptScenario::PushPopComplete) {
            if (frame.kind == FrameKind::RootPushChild) {
                ++frame.step;
                if (frame.step == 1) {
                    return FrameControlAction{
                        .control = FrameControl::Push,
                        .target = FrameKind::ChildComplete
                    };
                }

                return FrameControlAction{
                    .control = FrameControl::Complete,
                    .target = frame.kind
                };
            }

            ++frame.step;
            return FrameControlAction{
                .control = FrameControl::Pop,
                .target = frame.kind
            };
        }

        if (state.scenario == StackScriptScenario::ReplaceComplete) {
            ++frame.step;
            if (frame.kind == FrameKind::RootReplace) {
                return FrameControlAction{
                    .control = FrameControl::Replace,
                    .target = FrameKind::ReplacementComplete
                };
            }

            return FrameControlAction{
                .control = FrameControl::Complete,
                .target = frame.kind
            };
        }

        if (state.scenario == StackScriptScenario::WaitPushPopComplete) {
            if (frame.kind == FrameKind::RootWaitThenPush) {
                if (frame.step == 0) {
                    ++frame.step;
                    return FrameControlAction{
                        .control = FrameControl::Wait,
                        .target = frame.kind
                    };
                }

                ++frame.step;
                if (frame.step == 2) {
                    return FrameControlAction{
                        .control = FrameControl::Push,
                        .target = FrameKind::ChildComplete
                    };
                }

                return FrameControlAction{
                    .control = FrameControl::Complete,
                    .target = frame.kind
                };
            }

            ++frame.step;
            return FrameControlAction{
                .control = FrameControl::Pop,
                .target = frame.kind
            };
        }

        if (frame.kind == FrameKind::RootPushFailingChild) {
            ++frame.step;
            return FrameControlAction{
                .control = FrameControl::Push,
                .target = FrameKind::ChildFail
            };
        }

        ++frame.step;
        return FrameControlAction{
            .control = FrameControl::Fail,
            .target = frame.kind
        };
    }

    [[nodiscard]] bool StackFrameRuntime::IsTerminal(StackRunOutcome outcome)
    {
        return outcome == StackRunOutcome::Completed || outcome == StackRunOutcome::Failed;
    }
}
