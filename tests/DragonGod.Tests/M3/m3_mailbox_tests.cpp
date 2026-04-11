#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/runtime_compat.h"

#include <string>
#include <vector>

namespace
{
    namespace Keys
    {
        constexpr dragongod::BbKey<int> FirstMessageValue{ .name = "FirstMessageValue", .slot = 4 };
        constexpr dragongod::BbKey<int> SecondMessageValue{ .name = "SecondMessageValue", .slot = 5 };
        constexpr dragongod::BbKey<int> SeenPeekValue{ .name = "SeenPeekValue", .slot = 6 };
        constexpr dragongod::BbKey<bool> MailboxTriggered{ .name = "MailboxTriggered", .slot = 7 };
    }

    [[nodiscard]] bool ContainsSlot(const std::vector<std::uint32_t>& slots, std::uint32_t slot)
    {
        for (const std::uint32_t item : slots) {
            if (item == slot) {
                return true;
            }
        }

        return false;
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
}

FACT(M3_Mailbox_ConsumesMessagesInFifoOrder)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 22 }
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        mailboxInput);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "fifo consume scenario should complete");
    ASSERT_EQUAL(11, run.finalBlackboard.GetOr(Keys::FirstMessageValue, -1), "first consumed value should preserve enqueue order");
    ASSERT_EQUAL(22, run.finalBlackboard.GetOr(Keys::SecondMessageValue, -1), "second consumed value should preserve enqueue order");
}

FACT(M3_Mailbox_VisibilityRule_PreTickVisible_DuringTickEnqueueVisibleNextTick)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 }
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxEnqueueDuringTickComplete,
        8,
        mailboxInput);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "enqueue timing scenario should complete");
    ASSERT_TRUE(run.visibleMailboxByTick.size() >= 2, "scenario should execute at least two ticks");
    ASSERT_EQUAL(1, static_cast<int>(run.visibleMailboxByTick[0].size()), "pre-tick message should be visible on first executed tick");
    ASSERT_EQUAL(1, static_cast<int>(run.visibleMailboxByTick[1].size()), "during-tick enqueue should be visible on next tick");
    ASSERT_EQUAL(22, run.finalBlackboard.GetOr(Keys::SecondMessageValue, -1), "next-tick visible message should be consumed successfully");
}

FACT(M3_Mailbox_PeekDoesNotConsume_ConsumeRemoves)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 7 }
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxPeekThenConsumeComplete,
        8,
        mailboxInput);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "peek-then-consume scenario should complete");
    ASSERT_EQUAL(7, run.finalBlackboard.GetOr(Keys::SeenPeekValue, -1), "peek should observe the front message value");
    ASSERT_TRUE(run.visibleMailboxByTick.size() >= 2, "scenario should execute two mailbox ticks");
    ASSERT_EQUAL(1, static_cast<int>(run.visibleMailboxByTick[0].size()), "message should be visible on peek tick");
    ASSERT_EQUAL(1, static_cast<int>(run.visibleMailboxByTick[1].size()), "message should remain visible next tick before consumption");
}

FACT(M3_Mailbox_UnconsumedMessagesRemainDeterministicAcrossTicks)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 9 }
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxPeekThenConsumeComplete,
        8,
        mailboxInput);

    ASSERT_TRUE(run.visibleMailboxByTick.size() >= 2, "scenario should execute enough ticks for unconsumed carryover");
    ASSERT_TRUE(run.visibleMailboxByTick[0][0] == run.visibleMailboxByTick[1][0], "unconsumed front message should remain identical on next tick");
}

FACT(M3_Mailbox_RepeatedRuns_NoTraceOrMailboxDrift)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 22 }
    };

    const dragongod::FrameRunResult firstRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        mailboxInput);
    const dragongod::FrameRunResult secondRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxConsumeFifoComplete,
        8,
        mailboxInput);

    ASSERT_TRUE(firstRun.finalOutcome == secondRun.finalOutcome, "repeated mailbox runs should match final outcome");
    ASSERT_SEQUENCE_EQUAL(SerializeTrace(firstRun.trace), SerializeTrace(secondRun.trace), "repeated mailbox runs should match trace");
    ASSERT_SEQUENCE_EQUAL(
        SerializeVisibleMailbox(firstRun.visibleMailboxByTick),
        SerializeVisibleMailbox(secondRun.visibleMailboxByTick),
        "repeated mailbox runs should match visible mailbox snapshots");
}

FACT(M3_Mailbox_StackPushPopRemainsIntactDuringMailboxConsumption)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 13 },
        dragongod::Message{ .kind = dragongod::MessageKind::Alert, .value = 17 }
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxParentChildConsumeComplete,
        8,
        mailboxInput);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "mailbox parent/child scenario should complete");
    ASSERT_EQUAL(13, run.finalBlackboard.GetOr(Keys::FirstMessageValue, -1), "parent should consume first message");
    ASSERT_EQUAL(17, run.finalBlackboard.GetOr(Keys::SecondMessageValue, -1), "child should consume second message");
}

FACT(M3_Mailbox_DrivesBlackboardWrites_WithoutBreakingDirtyTracking)
{
    const dragongod::StackFrameRuntime runtime;
    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 11 }
    };

    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::MailboxEnqueueDuringTickComplete,
        8,
        mailboxInput);

    ASSERT_TRUE(run.finalBlackboard.GetOr(Keys::MailboxTriggered, false), "mailbox-driven branch should write blackboard state");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[0], Keys::FirstMessageValue.slot), "first mailbox consume tick should mark first value dirty");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[0], Keys::MailboxTriggered.slot), "mailbox-triggered bool write should be dirty in same tick");
    ASSERT_TRUE(ContainsSlot(run.dirtySlotsByTick[1], Keys::SecondMessageValue.slot), "second consume tick should mark second value dirty");
}
