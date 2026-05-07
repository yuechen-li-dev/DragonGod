#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/runtime_compat.h"

#include <vector>

namespace
{
    namespace Keys
    {
        constexpr dragongod::BbKey<int> FirstMessageValue{ .name = "FirstMessageValue", .slot = 4 };
        constexpr dragongod::BbKey<int> SecondMessageValue{ .name = "SecondMessageValue", .slot = 5 };
    }

    void AppendTickTrace(
        std::vector<dragongod::TickTraceEntry>& into,
        const std::vector<dragongod::TickTraceEntry>& from)
    {
        for (const dragongod::TickTraceEntry& entry : from) {
            into.push_back(entry);
        }
    }
}

FACT(M9_TypedPhaseHelpers_ConsumeMailboxAndWriteBlackboard)
{
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 22 }
    };

    dragongod::StackFrameRuntimeSession session(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        mailboxInput);
    const dragongod::FrameRunResult result = session.RunForTicks(6);

    ASSERT_TRUE(result.finalOutcome == dragongod::StackRunOutcome::Completed, "typed phase helpers should preserve mailbox consume completion");
    ASSERT_EQUAL(11, result.finalBlackboard.GetOr(Keys::FirstMessageValue, -1), "typed phase helper flow should keep first message write");
    ASSERT_EQUAL(22, result.finalBlackboard.GetOr(Keys::SecondMessageValue, -1), "typed phase helper flow should keep second message write");
}

FACT(M9_Stay_KeepsCurrentPhaseUntilMailboxAdvances)
{
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 41 }
    };
    mailboxInput.scheduledMessages = {
        dragongod::ScheduledMessage{
            .tick = 3,
            .message = dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 99 }
        }
    };

    dragongod::StackFrameRuntimeSession session(
        dragongod::StackScriptScenario::TypedPhaseMailboxActComplete,
        mailboxInput);
    const dragongod::FrameRunResult result = session.RunForTicks(8);

    bool sawStayAtAwaitAlertPhase = false;
    for (const dragongod::FrameTraceEvent& event : result.trace) {
        if (event.kind == dragongod::FrameTraceKind::Step &&
            event.activeFrame == dragongod::CanonicalFrameIds::RootTypedPhaseMailboxAct &&
            event.framePc == 1 &&
            event.control == dragongod::FrameControlKind::Continue) {
            sawStayAtAwaitAlertPhase = true;
            break;
        }
    }

    bool sawDeferredAlarmEmission = false;
    for (const std::vector<dragongod::ActRequest>& tickActuation : result.actuationByTick) {
        for (const dragongod::ActRequest& request : tickActuation) {
            if (request.id == dragongod::ActId::RaiseAlarm && request.deferred) {
                sawDeferredAlarmEmission = true;
                break;
            }
        }

        if (sawDeferredAlarmEmission) {
            break;
        }
    }

    ASSERT_TRUE(sawStayAtAwaitAlertPhase, "Stay should leave the frame on the same typed phase while awaiting mailbox progression");
    ASSERT_TRUE(sawDeferredAlarmEmission, "typed-phase frame should still interoperate with deferred actuation");
    ASSERT_TRUE(result.finalOutcome == dragongod::StackRunOutcome::Completed, "typed-phase stay scenario should complete once scheduled alert arrives");
    ASSERT_EQUAL(41, result.finalBlackboard.GetOr(Keys::FirstMessageValue, -1), "typed-phase stay frame should preserve first mailbox write");
    ASSERT_EQUAL(99, result.finalBlackboard.GetOr(Keys::SecondMessageValue, -1), "typed-phase stay frame should preserve second mailbox write");
}

FACT(M9_TypedPhaseSaveRestore_MatchesUninterruptedTickTrace)
{
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 41 }
    };
    mailboxInput.scheduledMessages = {
        dragongod::ScheduledMessage{
            .tick = 3,
            .message = dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 99 }
        }
    };

    dragongod::StackFrameRuntimeSession uninterrupted(
        dragongod::StackScriptScenario::TypedPhaseMailboxActComplete,
        mailboxInput);
    const dragongod::FrameRunResult uninterruptedResult = uninterrupted.RunForTicks(8);

    dragongod::StackFrameRuntimeSession split(
        dragongod::StackScriptScenario::TypedPhaseMailboxActComplete,
        mailboxInput);
    const dragongod::FrameRunResult firstLeg = split.RunForTicks(2);
    const dragongod::RuntimeChunk snapshot = split.Save();
    dragongod::StackFrameRuntimeSession restored(snapshot);
    const dragongod::FrameRunResult secondLeg = restored.RunForTicks(6);

    std::vector<dragongod::TickTraceEntry> splitTrace;
    AppendTickTrace(splitTrace, firstLeg.tickTrace);
    AppendTickTrace(splitTrace, secondLeg.tickTrace);

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(uninterruptedResult.tickTrace, splitTrace);
    ASSERT_TRUE(comparison.matches, "typed phase authored frames should preserve chunk save/restore determinism");
}

FACT(M9_TypedPhaseRuns_AreDeterministicAcrossRepetition)
{
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 41 }
    };
    mailboxInput.scheduledMessages = {
        dragongod::ScheduledMessage{
            .tick = 3,
            .message = dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 99 }
        }
    };

    dragongod::StackFrameRuntimeSession runA(
        dragongod::StackScriptScenario::TypedPhaseMailboxActComplete,
        mailboxInput);
    dragongod::StackFrameRuntimeSession runB(
        dragongod::StackScriptScenario::TypedPhaseMailboxActComplete,
        mailboxInput);

    const dragongod::FrameRunResult resultA = runA.RunForTicks(8);
    const dragongod::FrameRunResult resultB = runB.RunForTicks(8);

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(resultA.tickTrace, resultB.tickTrace);
    ASSERT_TRUE(comparison.matches, "typed phase + stay runs should not drift across repeated deterministic execution");
}
