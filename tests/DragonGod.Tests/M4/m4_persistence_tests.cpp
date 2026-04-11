#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/m1_single_frame.h"

#include <string>
#include <vector>

namespace
{
    namespace Keys
    {
        constexpr dragongod::BbKey<bool> ChildSawAlerted{ .name = "ChildSawAlerted", .slot = 2 };
        constexpr dragongod::BbKey<int> Counter{ .name = "Counter", .slot = 3 };
        constexpr dragongod::BbKey<int> FirstMessageValue{ .name = "FirstMessageValue", .slot = 4 };
        constexpr dragongod::BbKey<int> SecondMessageValue{ .name = "SecondMessageValue", .slot = 5 };
        constexpr dragongod::BbKey<bool> MailboxTriggered{ .name = "MailboxTriggered", .slot = 7 };
    }

    [[nodiscard]] std::vector<std::string> SerializeTrace(const std::vector<dragongod::FrameTraceEvent>& trace)
    {
        std::vector<std::string> serialized;
        serialized.reserve(trace.size());

        for (const dragongod::FrameTraceEvent& event : trace) {
            serialized.push_back(
                std::to_string(event.tick) + ":" +
                std::to_string(static_cast<int>(event.kind)) + ":" +
                std::to_string(static_cast<int>(event.activeFrame)) + ":" +
                std::to_string(event.framePc) + ":" +
                std::to_string(static_cast<int>(event.control)) + ":" +
                std::to_string(static_cast<int>(event.targetFrame)) + ":" +
                std::to_string(event.stackDepth));
        }

        return serialized;
    }

    [[nodiscard]] std::vector<std::string> SerializeVisibleMailbox(
        const std::vector<std::vector<dragongod::Message>>& visibleMailboxByTick)
    {
        std::vector<std::string> serialized;
        serialized.reserve(visibleMailboxByTick.size());

        for (const std::vector<dragongod::Message>& tickMessages : visibleMailboxByTick) {
            std::string line;
            for (const dragongod::Message& message : tickMessages) {
                line += std::to_string(static_cast<int>(message.kind));
                line += ":";
                line += std::to_string(message.value);
                line += ";";
            }

            serialized.push_back(line);
        }

        return serialized;
    }

    [[nodiscard]] std::vector<std::string> SerializeDirty(
        const std::vector<std::vector<std::uint32_t>>& dirtySlotsByTick)
    {
        std::vector<std::string> serialized;
        serialized.reserve(dirtySlotsByTick.size());

        for (const std::vector<std::uint32_t>& tickDirty : dirtySlotsByTick) {
            std::string line;
            for (const std::uint32_t slot : tickDirty) {
                line += std::to_string(slot);
                line += ";";
            }

            serialized.push_back(line);
        }

        return serialized;
    }

    void AppendTrace(std::vector<dragongod::FrameTraceEvent>& into, const std::vector<dragongod::FrameTraceEvent>& from)
    {
        for (const dragongod::FrameTraceEvent& event : from) {
            into.push_back(event);
        }
    }

    void AppendDirty(
        std::vector<std::vector<std::uint32_t>>& into,
        const std::vector<std::vector<std::uint32_t>>& from)
    {
        for (const std::vector<std::uint32_t>& tickDirty : from) {
            into.push_back(tickDirty);
        }
    }

    void AppendMailbox(
        std::vector<std::vector<dragongod::Message>>& into,
        const std::vector<std::vector<dragongod::Message>>& from)
    {
        for (const std::vector<dragongod::Message>& tickVisible : from) {
            into.push_back(tickVisible);
        }
    }
}

FACT(M4_SaveRestoreContinue_MatchesUninterruptedExecution)
{
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 }
    };

    dragongod::StackFrameRuntimeSession uninterrupted(
        dragongod::StackScriptScenario::MailboxEnqueueDuringTickComplete,
        mailboxInput);
    const dragongod::FrameRunResult uninterruptedResult = uninterrupted.RunForTicks(8);

    dragongod::StackFrameRuntimeSession split(
        dragongod::StackScriptScenario::MailboxEnqueueDuringTickComplete,
        mailboxInput);
    const dragongod::FrameRunResult firstLeg = split.RunForTicks(1);
    const dragongod::RuntimeChunk snapshot = split.Save();
    dragongod::StackFrameRuntimeSession restored(snapshot);
    const dragongod::FrameRunResult secondLeg = restored.RunForTicks(7);

    std::vector<dragongod::FrameTraceEvent> splitTrace;
    AppendTrace(splitTrace, firstLeg.trace);
    AppendTrace(splitTrace, secondLeg.trace);

    std::vector<std::vector<std::uint32_t>> splitDirty;
    AppendDirty(splitDirty, firstLeg.dirtySlotsByTick);
    AppendDirty(splitDirty, secondLeg.dirtySlotsByTick);

    std::vector<std::vector<dragongod::Message>> splitVisibleMailbox;
    AppendMailbox(splitVisibleMailbox, firstLeg.visibleMailboxByTick);
    AppendMailbox(splitVisibleMailbox, secondLeg.visibleMailboxByTick);

    ASSERT_TRUE(uninterruptedResult.finalOutcome == secondLeg.finalOutcome, "restored run should match uninterrupted final outcome");
    ASSERT_SEQUENCE_EQUAL(SerializeTrace(uninterruptedResult.trace), SerializeTrace(splitTrace), "restore path should match uninterrupted trace exactly");
    ASSERT_SEQUENCE_EQUAL(SerializeDirty(uninterruptedResult.dirtySlotsByTick), SerializeDirty(splitDirty), "restore path should match uninterrupted dirty slots per tick");
    ASSERT_SEQUENCE_EQUAL(
        SerializeVisibleMailbox(uninterruptedResult.visibleMailboxByTick),
        SerializeVisibleMailbox(splitVisibleMailbox),
        "restore path should match uninterrupted visible mailbox observations");
    ASSERT_EQUAL(
        uninterruptedResult.finalBlackboard.GetOr(Keys::SecondMessageValue, -1),
        secondLeg.finalBlackboard.GetOr(Keys::SecondMessageValue, -1),
        "restored blackboard should preserve mailbox-driven writes");
}

FACT(M4_StackChunk_RestoresPcEnterWaitAndFrameOrder)
{
    dragongod::StackFrameRuntimeSession initial(
        dragongod::StackScriptScenario::WaitPushPopComplete,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult firstTick = initial.RunForTicks(1);
    const dragongod::RuntimeChunk snapshot = initial.Save();

    ASSERT_EQUAL(static_cast<std::size_t>(1), snapshot.stack.frames.size(), "wait scenario should keep one active frame at snapshot boundary");
    ASSERT_TRUE(snapshot.stack.frames[0].entered, "root frame entered state should be persisted");
    ASSERT_EQUAL(1u, snapshot.stack.frames[0].pc, "resume pc should be persisted after wait");
    ASSERT_EQUAL(0u, snapshot.stack.frames[0].remainingWaitTicks, "remaining wait ticks should be persisted");

    dragongod::StackFrameRuntimeSession restored(snapshot);
    const dragongod::FrameRunResult remaining = restored.RunForTicks(8);

    bool sawPushAfterRestore = false;
    bool sawCompleteAfterRestore = false;
    for (const dragongod::FrameTraceEvent& event : remaining.trace) {
        if (event.kind == dragongod::FrameTraceKind::Step &&
            event.activeFrame == dragongod::FrameId::RootWaitThenPush &&
            event.framePc == 1 &&
            event.control == dragongod::FrameControlKind::Push) {
            sawPushAfterRestore = true;
        }

        if (event.kind == dragongod::FrameTraceKind::TerminalCompleted &&
            event.activeFrame == dragongod::FrameId::RootWaitThenPush) {
            sawCompleteAfterRestore = true;
        }
    }

    ASSERT_EQUAL(static_cast<std::size_t>(1), firstTick.dirtySlotsByTick.size(), "first leg should execute exactly one tick before snapshot");
    ASSERT_TRUE(sawPushAfterRestore, "restored run should continue from pc=1 and push child");
    ASSERT_TRUE(sawCompleteAfterRestore, "restored run should preserve frame order to completion");
}

FACT(M4_BlackboardDirtyAndMailboxChunks_RestoreAccordingToBoundaryContract)
{
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 }
    };

    dragongod::StackFrameRuntimeSession initial(
        dragongod::StackScriptScenario::MailboxEnqueueDuringTickComplete,
        mailboxInput);
    const dragongod::FrameRunResult firstTick = initial.RunForTicks(1);
    const dragongod::RuntimeChunk snapshot = initial.Save();

    ASSERT_TRUE(!snapshot.blackboard.dirtySlots.empty(), "dirty state should be persisted exactly from the completed pre-snapshot tick");
    ASSERT_EQUAL(static_cast<std::size_t>(0), snapshot.mailbox.visibleMessages.size(), "visible mailbox should be empty after first consume tick");
    ASSERT_EQUAL(static_cast<std::size_t>(1), snapshot.mailbox.stagedMessages.size(), "staged mailbox should retain during-tick enqueue for next tick");

    dragongod::StackFrameRuntimeSession restored(snapshot);
    const dragongod::FrameRunResult continued = restored.RunForTicks(4);

    ASSERT_TRUE(continued.finalOutcome == dragongod::StackRunOutcome::Completed, "restored mailbox scenario should still complete");
    ASSERT_TRUE(continued.finalBlackboard.GetOr(Keys::MailboxTriggered, false), "restored blackboard bool state should be preserved");
    ASSERT_EQUAL(11, continued.finalBlackboard.GetOr(Keys::FirstMessageValue, -1), "restored blackboard int state should be preserved");
    ASSERT_EQUAL(22, continued.finalBlackboard.GetOr(Keys::SecondMessageValue, -1), "restored staged mailbox message should become visible and be consumed");
}

FACT(M4_RepeatedSnapshotRestoreRuns_AreDeterministic)
{
    dragongod::StackFrameRuntimeSession runA(
        dragongod::StackScriptScenario::BlackboardParentChildComplete,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult runAFirstLeg = runA.RunForTicks(2);
    const dragongod::RuntimeChunk snapshotA = runA.Save();
    dragongod::StackFrameRuntimeSession runARestored(snapshotA);
    const dragongod::FrameRunResult runASecondLeg = runARestored.RunForTicks(10);

    dragongod::StackFrameRuntimeSession runB(
        dragongod::StackScriptScenario::BlackboardParentChildComplete,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult runBFirstLeg = runB.RunForTicks(2);
    const dragongod::RuntimeChunk snapshotB = runB.Save();
    dragongod::StackFrameRuntimeSession runBRestored(snapshotB);
    const dragongod::FrameRunResult runBSecondLeg = runBRestored.RunForTicks(10);

    ASSERT_TRUE(snapshotA.stack.frames == snapshotB.stack.frames, "repeated snapshots should persist identical stack chunks");
    ASSERT_TRUE(snapshotA.blackboard.boolEntries == snapshotB.blackboard.boolEntries, "repeated snapshots should persist identical bool chunks");
    ASSERT_TRUE(snapshotA.blackboard.intEntries == snapshotB.blackboard.intEntries, "repeated snapshots should persist identical int chunks");
    ASSERT_TRUE(snapshotA.mailbox.visibleMessages == snapshotB.mailbox.visibleMessages, "repeated snapshots should persist identical visible mailbox chunks");
    ASSERT_TRUE(snapshotA.mailbox.stagedMessages == snapshotB.mailbox.stagedMessages, "repeated snapshots should persist identical staged mailbox chunks");

    std::vector<dragongod::FrameTraceEvent> traceA;
    AppendTrace(traceA, runAFirstLeg.trace);
    AppendTrace(traceA, runASecondLeg.trace);

    std::vector<dragongod::FrameTraceEvent> traceB;
    AppendTrace(traceB, runBFirstLeg.trace);
    AppendTrace(traceB, runBSecondLeg.trace);

    ASSERT_SEQUENCE_EQUAL(SerializeTrace(traceA), SerializeTrace(traceB), "repeated restore runs should have no trace drift");
    ASSERT_TRUE(runASecondLeg.finalOutcome == runBSecondLeg.finalOutcome, "repeated restore runs should match final outcome");
    ASSERT_TRUE(runASecondLeg.finalBlackboard.GetOr(Keys::ChildSawAlerted, false), "restored run should continue through canonical child frame logic");
    ASSERT_EQUAL(5, runASecondLeg.finalBlackboard.GetOr(Keys::Counter, 0), "restored run should preserve canonical parent-child blackboard semantics");
}
