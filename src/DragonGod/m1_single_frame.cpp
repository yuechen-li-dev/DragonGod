#include "m1_single_frame.h"

namespace dragongod
{
    namespace
    {
        struct FrameInstance
        {
            FrameId id = FrameId::RootPushChild;
            std::uint32_t pc = 0;
            bool entered = false;
            std::uint32_t remainingWaitTicks = 0;
        };

        [[nodiscard]] FrameId ScenarioRootFrame(StackScriptScenario scenario)
        {
            if (scenario == StackScriptScenario::PushPopComplete) {
                return FrameId::RootPushChild;
            }

            if (scenario == StackScriptScenario::ReplaceComplete) {
                return FrameId::RootReplace;
            }

            if (scenario == StackScriptScenario::WaitPushPopComplete) {
                return FrameId::RootWaitThenPush;
            }

            if (scenario == StackScriptScenario::PushChildFail) {
                return FrameId::RootPushFailingChild;
            }

            if (scenario == StackScriptScenario::BlackboardSetReadComplete) {
                return FrameId::RootSetThenReadBlackboard;
            }

            if (scenario == StackScriptScenario::BlackboardFallbackComplete) {
                return FrameId::RootFallbackBranch;
            }

            if (scenario == StackScriptScenario::BlackboardParentChildComplete) {
                return FrameId::RootParentChildBlackboard;
            }

            if (scenario == StackScriptScenario::MailboxConsumeFifoComplete) {
                return FrameId::RootMailboxConsumeFifo;
            }

            if (scenario == StackScriptScenario::MailboxPeekThenConsumeComplete) {
                return FrameId::RootMailboxPeekThenConsume;
            }

            if (scenario == StackScriptScenario::MailboxParentChildConsumeComplete) {
                return FrameId::RootMailboxParentPushChildConsume;
            }

            if (scenario == StackScriptScenario::MailboxEnqueueDuringTickComplete) {
                return FrameId::RootMailboxEnqueueDuringTick;
            }

            return FrameId::RootContinueThenComplete;
        }

        void EmitTrace(
            FrameRunResult& result,
            TickIndex tick,
            FrameTraceKind kind,
            const FrameInstance& active,
            FrameControlKind control,
            FrameId targetFrame,
            std::size_t stackDepth)
        {
            result.trace.push_back(FrameTraceEvent{
                .tick = tick,
                .kind = kind,
                .activeFrame = active.id,
                .framePc = active.pc,
                .control = control,
                .targetFrame = targetFrame,
                .stackDepth = stackDepth
            });
        }

        namespace nodes
        {
            namespace Keys
            {
                constexpr BbKey<bool> Alerted{ .name = "Alerted", .slot = 1 };
                constexpr BbKey<bool> ChildSawAlerted{ .name = "ChildSawAlerted", .slot = 2 };
                constexpr BbKey<int> Counter{ .name = "Counter", .slot = 3 };
                constexpr BbKey<int> FirstMessageValue{ .name = "FirstMessageValue", .slot = 4 };
                constexpr BbKey<int> SecondMessageValue{ .name = "SecondMessageValue", .slot = 5 };
                constexpr BbKey<int> SeenPeekValue{ .name = "SeenPeekValue", .slot = 6 };
                constexpr BbKey<bool> MailboxTriggered{ .name = "MailboxTriggered", .slot = 7 };
            }

            [[nodiscard]] FrameControl RootPushChild(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Push(FrameId::ChildPop, 1);
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(100);
                }
            }

            [[nodiscard]] FrameControl RootReplace(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Replace(FrameId::RecoveryComplete);
                default:
                    return Dg::Fail(101);
                }
            }

            [[nodiscard]] FrameControl RootWaitThenPush(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::WaitTicks(1, 1);
                case 1:
                    return Dg::Push(FrameId::ChildPop, 2);
                case 2:
                    return Dg::Complete();
                default:
                    return Dg::Fail(102);
                }
            }

            [[nodiscard]] FrameControl RootPushFailingChild(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Push(FrameId::ChildFail, 1);
                default:
                    return Dg::Fail(103);
                }
            }

            [[nodiscard]] FrameControl RootContinueThenComplete(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Continue(1);
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(104);
                }
            }

            [[nodiscard]] FrameControl RootSetThenReadBlackboard(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::Alerted, true);
                    return Dg::Continue(1);
                case 1:
                    if (ctx.Bb().GetOr(Keys::Alerted, false)) {
                        return Dg::Complete();
                    }

                    return Dg::Fail(300);
                default:
                    return Dg::Fail(301);
                }
            }

            [[nodiscard]] FrameControl RootFallbackBranch(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Bb().GetOr(Keys::Alerted, false)) {
                        return Dg::Complete();
                    }

                    return Dg::Fail(302);
                default:
                    return Dg::Fail(303);
                }
            }

            [[nodiscard]] FrameControl RootParentChildBlackboard(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::Alerted, true);
                    ctx.Bb().Set(Keys::Counter, 1);
                    return Dg::Push(FrameId::ChildReadParentBool, 1);
                case 1:
                    if (!ctx.Bb().GetOr(Keys::ChildSawAlerted, false)) {
                        return Dg::Fail(304);
                    }

                    return Dg::Push(FrameId::ChildWriteParentCounter, 2);
                case 2:
                    if (ctx.Bb().GetOr(Keys::Counter, 0) == 5) {
                        return Dg::Complete();
                    }

                    return Dg::Fail(305);
                default:
                    return Dg::Fail(306);
                }
            }

            [[nodiscard]] FrameControl ChildPop(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Pop();
                default:
                    return Dg::Fail(105);
                }
            }

            [[nodiscard]] FrameControl ChildFail(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Fail(201);
                default:
                    return Dg::Fail(202);
                }
            }

            [[nodiscard]] FrameControl ChildReadParentBool(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    if (ctx.Bb().GetOr(Keys::Alerted, false)) {
                        ctx.Bb().Set(Keys::ChildSawAlerted, true);
                        return Dg::Pop();
                    }

                    return Dg::Fail(307);
                default:
                    return Dg::Fail(308);
                }
            }

            [[nodiscard]] FrameControl ChildWriteParentCounter(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    ctx.Bb().Set(Keys::Counter, 5);
                    return Dg::Pop();
                default:
                    return Dg::Fail(309);
                }
            }

            [[nodiscard]] FrameControl RecoveryComplete(FrameCtx& ctx)
            {
                switch (ctx.Pc()) {
                case 0:
                    return Dg::Complete();
                default:
                    return Dg::Fail(106);
                }
            }

            [[nodiscard]] FrameControl RootMailboxConsumeFifo(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::FirstMessageValue, message.value);
                    return Dg::Continue(1);
                case 1:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 1);
                    }

                    ctx.Bb().Set(Keys::SecondMessageValue, message.value);
                    return Dg::Complete();
                default:
                    return Dg::Fail(400);
                }
            }

            [[nodiscard]] FrameControl RootMailboxPeekThenConsume(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().PeekFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::SeenPeekValue, message.value);
                    return Dg::Continue(1);
                case 1:
                    if (!ctx.Mb().PeekFront(message)) {
                        return Dg::Fail(401);
                    }

                    if (ctx.Bb().GetOr(Keys::SeenPeekValue, -1) != message.value) {
                        return Dg::Fail(402);
                    }

                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::Fail(403);
                    }

                    return Dg::Complete();
                default:
                    return Dg::Fail(404);
                }
            }

            [[nodiscard]] FrameControl RootMailboxParentPushChildConsume(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::FirstMessageValue, message.value);
                    return Dg::Push(FrameId::ChildMailboxConsumeAndPop, 1);
                case 1:
                    return Dg::Complete();
                default:
                    return Dg::Fail(405);
                }
            }

            [[nodiscard]] FrameControl RootMailboxEnqueueDuringTick(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::FirstMessageValue, message.value);
                    ctx.Bb().Set(Keys::MailboxTriggered, true);
                    ctx.Mb().Enqueue(Message{
                        .kind = MessageKind::Alert,
                        .value = 22
                    });
                    return Dg::Continue(1);
                case 1:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 1);
                    }

                    if (message.kind != MessageKind::Alert || message.value != 22) {
                        return Dg::Fail(406);
                    }

                    ctx.Bb().Set(Keys::SecondMessageValue, message.value);
                    return Dg::Complete();
                default:
                    return Dg::Fail(407);
                }
            }

            [[nodiscard]] FrameControl ChildMailboxConsumeAndPop(FrameCtx& ctx)
            {
                Message message;
                switch (ctx.Pc()) {
                case 0:
                    if (!ctx.Mb().ConsumeFront(message)) {
                        return Dg::WaitTicks(1, 0);
                    }

                    ctx.Bb().Set(Keys::SecondMessageValue, message.value);
                    return Dg::Pop();
                default:
                    return Dg::Fail(408);
                }
            }
        }
    }

    FrameCtx::FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard)
        : frameId_(frameId)
        , tick_(tick)
        , pc_(pc)
        , entered_(entered)
        , blackboard_(&blackboard)
    {
    }

    FrameCtx::FrameCtx(FrameId frameId, TickIndex tick, std::uint32_t pc, bool entered, Blackboard& blackboard, Mailbox& mailbox)
        : frameId_(frameId)
        , tick_(tick)
        , pc_(pc)
        , entered_(entered)
        , blackboard_(&blackboard)
        , mailbox_(&mailbox)
    {
    }

    [[nodiscard]] FrameId FrameCtx::Id() const
    {
        return frameId_;
    }

    [[nodiscard]] TickIndex FrameCtx::Tick() const
    {
        return tick_;
    }

    [[nodiscard]] std::uint32_t FrameCtx::Pc() const
    {
        return pc_;
    }

    [[nodiscard]] bool FrameCtx::Entered() const
    {
        return entered_;
    }

    [[nodiscard]] Blackboard& FrameCtx::Bb()
    {
        return *blackboard_;
    }

    [[nodiscard]] Mailbox& FrameCtx::Mb()
    {
        return *mailbox_;
    }

    void Mailbox::Enqueue(const Message& message)
    {
        staged_.push_back(message);
    }

    void Mailbox::BeginTick()
    {
        // M3 visibility rule:
        // - Messages enqueued before BeginTick are visible during this tick.
        // - Messages enqueued during frame execution are staged and become visible next tick.
        for (const Message& message : staged_) {
            visible_.push_back(message);
        }

        staged_.clear();
    }

    [[nodiscard]] bool Mailbox::HasMessage() const
    {
        return !visible_.empty();
    }

    [[nodiscard]] bool Mailbox::PeekFront(Message& message) const
    {
        if (visible_.empty()) {
            return false;
        }

        message = visible_.front();
        return true;
    }

    [[nodiscard]] bool Mailbox::ConsumeFront(Message& message)
    {
        if (visible_.empty()) {
            return false;
        }

        message = visible_.front();
        visible_.erase(visible_.begin());
        return true;
    }

    [[nodiscard]] const std::vector<Message>& Mailbox::VisibleMessages() const
    {
        return visible_;
    }

    template <>
    [[nodiscard]] const bool* Blackboard::FindValue<bool>(std::uint32_t slot) const
    {
        for (const BoolEntry& entry : boolEntries_) {
            if (entry.slot == slot) {
                return &entry.value;
            }
        }

        return nullptr;
    }

    template <>
    [[nodiscard]] const int* Blackboard::FindValue<int>(std::uint32_t slot) const
    {
        for (const IntEntry& entry : intEntries_) {
            if (entry.slot == slot) {
                return &entry.value;
            }
        }

        return nullptr;
    }

    template <>
    void Blackboard::UpsertValue<bool>(std::uint32_t slot, const bool& value)
    {
        for (BoolEntry& entry : boolEntries_) {
            if (entry.slot == slot) {
                entry.value = value;
                return;
            }
        }

        boolEntries_.push_back(BoolEntry{
            .slot = slot,
            .value = value
        });
    }

    template <>
    void Blackboard::UpsertValue<int>(std::uint32_t slot, const int& value)
    {
        for (IntEntry& entry : intEntries_) {
            if (entry.slot == slot) {
                entry.value = value;
                return;
            }
        }

        intEntries_.push_back(IntEntry{
            .slot = slot,
            .value = value
        });
    }

    [[nodiscard]] const std::vector<std::uint32_t>& Blackboard::DirtySlots() const
    {
        return dirtySlots_;
    }

    void Blackboard::ClearDirty()
    {
        dirtySlots_.clear();
    }

    void Blackboard::MarkDirty(std::uint32_t slot)
    {
        if (HasDirtySlot(slot)) {
            return;
        }

        dirtySlots_.push_back(slot);
    }

    [[nodiscard]] bool Blackboard::HasDirtySlot(std::uint32_t slot) const
    {
        for (const std::uint32_t dirtySlot : dirtySlots_) {
            if (dirtySlot == slot) {
                return true;
            }
        }

        return false;
    }

    void FrameRegistry::Add(FrameId id, FrameFn function)
    {
        definitions_.push_back(FrameDef{
            .id = id,
            .function = function
        });
    }

    [[nodiscard]] FrameFn FrameRegistry::Find(FrameId id) const
    {
        for (const FrameDef& definition : definitions_) {
            if (definition.id == id) {
                return definition.function;
            }
        }

        return nullptr;
    }

    namespace Dg
    {
        [[nodiscard]] FrameControl Continue(std::uint32_t resumePc)
        {
            return FrameControl{
                .kind = FrameControlKind::Continue,
                .resumePc = resumePc
            };
        }

        [[nodiscard]] FrameControl WaitTicks(std::uint32_t ticks, std::uint32_t resumePc)
        {
            return FrameControl{
                .kind = FrameControlKind::Wait,
                .resumePc = resumePc,
                .waitTicks = ticks
            };
        }

        [[nodiscard]] FrameControl Push(FrameId target, std::uint32_t resumePc)
        {
            return FrameControl{
                .kind = FrameControlKind::Push,
                .resumePc = resumePc,
                .target = target
            };
        }

        [[nodiscard]] FrameControl Pop()
        {
            return FrameControl{
                .kind = FrameControlKind::Pop
            };
        }

        [[nodiscard]] FrameControl Replace(FrameId target)
        {
            return FrameControl{
                .kind = FrameControlKind::Replace,
                .target = target
            };
        }

        [[nodiscard]] FrameControl Complete()
        {
            return FrameControl{
                .kind = FrameControlKind::Complete
            };
        }

        [[nodiscard]] FrameControl Fail(int reason)
        {
            return FrameControl{
                .kind = FrameControlKind::Fail,
                .failReason = reason
            };
        }
    }

    [[nodiscard]] FrameRunResult StackFrameRuntime::RunForTicks(StackScriptScenario scenario, TickIndex tickCount) const
    {
        return RunForTicks(scenario, tickCount, RuntimeMailboxInput{});
    }

    [[nodiscard]] FrameRunResult StackFrameRuntime::RunForTicks(
        StackScriptScenario scenario,
        TickIndex tickCount,
        const RuntimeMailboxInput& mailboxInput) const
    {
        const FrameRegistry registry = BuildRegistry();
        FrameRunResult result;
        Blackboard blackboard;
        Mailbox mailbox;
        for (const Message& message : mailboxInput.initialMessages) {
            mailbox.Enqueue(message);
        }

        std::vector<FrameInstance> stack;
        stack.push_back(FrameInstance{ .id = ScenarioRootFrame(scenario) });

        for (TickIndex tick = 0; tick < tickCount; ++tick) {
            // M2b rule: dirty slots always represent writes performed in the current tick only.
            // They are cleared at the start of each tick before any frame executes.
            blackboard.ClearDirty();

            const auto recordDirtyTick = [&result, &blackboard]() {
                result.dirtySlotsByTick.push_back(blackboard.DirtySlots());
            };

            for (const ScheduledMessage& scheduled : mailboxInput.scheduledMessages) {
                if (scheduled.tick == tick) {
                    mailbox.Enqueue(scheduled.message);
                }
            }

            mailbox.BeginTick();
            result.visibleMailboxByTick.push_back(mailbox.VisibleMessages());

            if (stack.empty()) {
                result.finalOutcome = StackRunOutcome::Completed;
                break;
            }

            FrameInstance& frame = stack.back();
            EmitTrace(result, tick, FrameTraceKind::Tick, frame, FrameControlKind::Continue, frame.id, stack.size());

            if (!frame.entered) {
                frame.entered = true;
                EmitTrace(result, tick, FrameTraceKind::Enter, frame, FrameControlKind::Continue, frame.id, stack.size());
            }

            if (frame.remainingWaitTicks > 0) {
                --frame.remainingWaitTicks;
                EmitTrace(result, tick, FrameTraceKind::Step, frame, FrameControlKind::Wait, frame.id, stack.size());
                result.finalOutcome = StackRunOutcome::Wait;
                recordDirtyTick();
                continue;
            }

            FrameCtx ctx(frame.id, tick, frame.pc, frame.entered, blackboard, mailbox);
            const FrameFn frameFunction = registry.Find(frame.id);
            if (frameFunction == nullptr) {
                EmitTrace(result, tick, FrameTraceKind::ExitFailed, frame, FrameControlKind::Fail, frame.id, stack.size());
                EmitTrace(result, tick, FrameTraceKind::TerminalFailed, frame, FrameControlKind::Fail, frame.id, stack.size());
                result.finalOutcome = StackRunOutcome::Failed;
                recordDirtyTick();
                break;
            }

            const FrameControl control = frameFunction(ctx);
            EmitTrace(result, tick, FrameTraceKind::Step, frame, control.kind, control.target, stack.size());

            if (control.kind == FrameControlKind::Continue) {
                frame.pc = control.resumePc;
                result.finalOutcome = StackRunOutcome::Continue;
                recordDirtyTick();
                continue;
            }

            if (control.kind == FrameControlKind::Wait) {
                frame.pc = control.resumePc;
                if (control.waitTicks > 0) {
                    frame.remainingWaitTicks = control.waitTicks - 1;
                } else {
                    frame.remainingWaitTicks = 0;
                }
                result.finalOutcome = StackRunOutcome::Wait;
                recordDirtyTick();
                continue;
            }

            if (control.kind == FrameControlKind::Push) {
                frame.pc = control.resumePc;
                EmitTrace(result, tick, FrameTraceKind::Push, frame, control.kind, control.target, stack.size());
                stack.push_back(FrameInstance{ .id = control.target });
                result.finalOutcome = StackRunOutcome::Continue;
                recordDirtyTick();
                continue;
            }

            if (control.kind == FrameControlKind::Replace) {
                const std::size_t depthBeforeReplace = stack.size();
                const FrameInstance replaced = frame;
                EmitTrace(result, tick, FrameTraceKind::Replace, replaced, control.kind, control.target, depthBeforeReplace);
                stack.pop_back();
                stack.push_back(FrameInstance{ .id = control.target });
                result.finalOutcome = StackRunOutcome::Continue;
                recordDirtyTick();
                continue;
            }

            if (control.kind == FrameControlKind::Pop || control.kind == FrameControlKind::Complete) {
                const std::size_t depthBeforePop = stack.size();
                const FrameInstance completedFrame = frame;
                EmitTrace(result, tick, FrameTraceKind::ExitCompleted, completedFrame, control.kind, completedFrame.id, depthBeforePop);
                stack.pop_back();
                EmitTrace(result, tick, FrameTraceKind::Pop, completedFrame, control.kind, completedFrame.id, depthBeforePop);

                if (stack.empty()) {
                    result.finalOutcome = StackRunOutcome::Completed;
                    EmitTrace(result, tick, FrameTraceKind::TerminalCompleted, completedFrame, control.kind, completedFrame.id, 0);
                    recordDirtyTick();
                    break;
                }

                result.finalOutcome = StackRunOutcome::Continue;
                recordDirtyTick();
                continue;
            }

            const std::size_t depthBeforeFail = stack.size();
            const FrameInstance failedFrame = frame;
            EmitTrace(result, tick, FrameTraceKind::ExitFailed, failedFrame, control.kind, failedFrame.id, depthBeforeFail);
            EmitTrace(result, tick, FrameTraceKind::TerminalFailed, failedFrame, control.kind, failedFrame.id, depthBeforeFail);
            result.finalOutcome = StackRunOutcome::Failed;
            recordDirtyTick();
            break;
        }

        result.finalBlackboard = blackboard;
        return result;
    }

    [[nodiscard]] FrameRegistry StackFrameRuntime::BuildRegistry()
    {
        FrameRegistry registry;
        registry.Add(FrameId::RootPushChild, &nodes::RootPushChild);
        registry.Add(FrameId::RootReplace, &nodes::RootReplace);
        registry.Add(FrameId::RootWaitThenPush, &nodes::RootWaitThenPush);
        registry.Add(FrameId::RootPushFailingChild, &nodes::RootPushFailingChild);
        registry.Add(FrameId::RootContinueThenComplete, &nodes::RootContinueThenComplete);
        registry.Add(FrameId::RootSetThenReadBlackboard, &nodes::RootSetThenReadBlackboard);
        registry.Add(FrameId::RootFallbackBranch, &nodes::RootFallbackBranch);
        registry.Add(FrameId::RootParentChildBlackboard, &nodes::RootParentChildBlackboard);
        registry.Add(FrameId::ChildPop, &nodes::ChildPop);
        registry.Add(FrameId::ChildFail, &nodes::ChildFail);
        registry.Add(FrameId::ChildReadParentBool, &nodes::ChildReadParentBool);
        registry.Add(FrameId::ChildWriteParentCounter, &nodes::ChildWriteParentCounter);
        registry.Add(FrameId::RecoveryComplete, &nodes::RecoveryComplete);
        registry.Add(FrameId::RootMailboxConsumeFifo, &nodes::RootMailboxConsumeFifo);
        registry.Add(FrameId::RootMailboxPeekThenConsume, &nodes::RootMailboxPeekThenConsume);
        registry.Add(FrameId::RootMailboxParentPushChildConsume, &nodes::RootMailboxParentPushChildConsume);
        registry.Add(FrameId::RootMailboxEnqueueDuringTick, &nodes::RootMailboxEnqueueDuringTick);
        registry.Add(FrameId::ChildMailboxConsumeAndPop, &nodes::ChildMailboxConsumeAndPop);
        return registry;
    }
}
