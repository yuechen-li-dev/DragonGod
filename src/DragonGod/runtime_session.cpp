#include "runtime_internal.h"

namespace dragongod
{
    namespace
    {
        void EmitTrace(
            FrameRunResult& result,
            TickIndex tick,
            FrameTraceKind kind,
            const StackFrameChunkEntry& active,
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

        [[nodiscard]] TickTraceEntry MakeTickTraceEntry(
            TickIndex tick,
            StackRunOutcome outcome,
            const std::vector<StackFrameChunkEntry>& stack,
            const std::vector<std::uint32_t>& dirtySlots,
            const std::vector<Message>& visibleMailbox,
            const std::vector<UtilityDecisionTraceEntry>& utilityDecisions,
            const std::vector<ActRequest>& emittedActuation,
            const std::vector<ActRequest>& pendingDeferredActuation)
        {
            return TickTraceEntry{
                .tick = tick,
                .outcome = outcome,
                .stack = stack,
                .dirtySlots = dirtySlots,
                .visibleMailbox = visibleMailbox,
                .utilityDecisions = utilityDecisions,
                .emittedActuation = emittedActuation,
                .pendingDeferredActuation = pendingDeferredActuation
            };
        }
    }

    StackFrameRuntimeSession::StackFrameRuntimeSession(
        StackScriptScenario scenario,
        const RuntimeMailboxInput& mailboxInput)
        : scenario_(scenario)
        , registry_(BuildRegistry())
        , scheduledMessages_(mailboxInput.scheduledMessages)
        , actRuntime_(std::make_unique<ActRuntime>())
        , utilityMemory_(std::make_unique<UtilityMemoryStore>())
    {
        stack_.push_back(StackFrameChunkEntry{ .id = ScenarioRootFrame(scenario_) });
        for (const Message& message : mailboxInput.initialMessages) {
            mailbox_.Enqueue(message);
        }
    }

    StackFrameRuntimeSession::StackFrameRuntimeSession(const RuntimeChunk& chunk)
        : scenario_(chunk.scenario)
        , nextTick_(chunk.nextTick)
        , lastOutcome_(chunk.lastOutcome)
        , registry_(BuildRegistry())
        , actRuntime_(std::make_unique<ActRuntime>())
        , utilityMemory_(std::make_unique<UtilityMemoryStore>())
    {
        stack_ = chunk.stack.frames;
        actRuntime_->ImportDeferredChunk(chunk.deferredActuation);
        utilityMemory_->ImportChunk(chunk.utilityMemory);
        blackboard_.ImportChunk(chunk.blackboard);
        mailbox_.ImportChunk(chunk.mailbox);
        scheduledMessages_ = chunk.scheduledMessages;
    }

    StackFrameRuntimeSession::~StackFrameRuntimeSession() = default;

    [[nodiscard]] TickIndex StackFrameRuntimeSession::NextTick() const
    {
        return nextTick_;
    }

    [[nodiscard]] StackRunOutcome StackFrameRuntimeSession::LastOutcome() const
    {
        return lastOutcome_;
    }

    [[nodiscard]] bool StackFrameRuntimeSession::IsTerminal() const
    {
        if (lastOutcome_ == StackRunOutcome::Failed) {
            return true;
        }

        if (lastOutcome_ == StackRunOutcome::Completed) {
            return actRuntime_->Pending().empty();
        }

        return false;
    }

    [[nodiscard]] RuntimeChunk StackFrameRuntimeSession::Save() const
    {
        // M4 persistence boundary:
        // Save() is valid between ticks after all effects of tick N are complete and before tick N+1 starts.
        return RuntimeChunk{
            .scenario = scenario_,
            .nextTick = nextTick_,
            .lastOutcome = lastOutcome_,
            .scheduledMessages = scheduledMessages_,
            .stack = StackChunk{ .frames = stack_ },
            .utilityMemory = utilityMemory_->ExportChunk(),
            .deferredActuation = actRuntime_->ExportDeferredChunk(),
            .blackboard = blackboard_.ExportChunk(),
            .mailbox = mailbox_.ExportChunk()
        };
    }

    [[nodiscard]] FrameRunResult StackFrameRuntimeSession::RunForTicks(TickIndex tickCount)
    {
        FrameRunResult result;
        for (TickIndex tick = 0; tick < tickCount; ++tick) {
            if (!RunSingleTick(result)) {
                break;
            }
        }

        result.finalOutcome = lastOutcome_;
        result.finalBlackboard = blackboard_;
        return result;
    }

    [[nodiscard]] bool StackFrameRuntimeSession::RunSingleTick(FrameRunResult& result)
    {
        if (IsTerminal()) {
            return false;
        }

        // M2b + M4 dirty persistence contract:
        // dirty slots represent writes in the currently executing tick only.
        // Save() at the boundary persists the exact dirty slots from the most recently completed tick.
        blackboard_.ClearDirty();

        for (const ScheduledMessage& scheduled : scheduledMessages_) {
            if (scheduled.tick == nextTick_) {
                mailbox_.Enqueue(scheduled.message);
            }
        }

        mailbox_.BeginTick();
        actRuntime_->BeginTick(nextTick_);
        actRuntime_->FlushMatured();
        result.visibleMailboxByTick.push_back(mailbox_.VisibleMessages());
        std::vector<UtilityDecisionTraceEntry> tickUtilityDecisions;

        if (stack_.empty()) {
            lastOutcome_ = StackRunOutcome::Completed;
            result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
            result.actuationByTick.push_back(actRuntime_->EmittedNow());
            result.tickTrace.push_back(MakeTickTraceEntry(
                nextTick_,
                lastOutcome_,
                stack_,
                blackboard_.DirtySlots(),
                mailbox_.VisibleMessages(),
                tickUtilityDecisions,
                actRuntime_->EmittedNow(),
                actRuntime_->Pending()));
            ++nextTick_;
            return !IsTerminal();
        }

        StackFrameChunkEntry& frame = stack_.back();
        EmitTrace(result, nextTick_, FrameTraceKind::Tick, frame, FrameControlKind::Continue, frame.id, stack_.size());

        if (!frame.entered) {
            frame.entered = true;
            EmitTrace(result, nextTick_, FrameTraceKind::Enter, frame, FrameControlKind::Continue, frame.id, stack_.size());
        }

        if (frame.remainingWaitTicks > 0) {
            --frame.remainingWaitTicks;
            EmitTrace(result, nextTick_, FrameTraceKind::Step, frame, FrameControlKind::Wait, frame.id, stack_.size());
            lastOutcome_ = StackRunOutcome::Wait;
            result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
            result.actuationByTick.push_back(actRuntime_->EmittedNow());
            result.tickTrace.push_back(MakeTickTraceEntry(
                nextTick_,
                lastOutcome_,
                stack_,
                blackboard_.DirtySlots(),
                mailbox_.VisibleMessages(),
                tickUtilityDecisions,
                actRuntime_->EmittedNow(),
                actRuntime_->Pending()));
            ++nextTick_;
            return true;
        }

        FrameCtx ctx(frame.id, nextTick_, frame.pc, frame.entered, blackboard_, mailbox_, *actRuntime_, *utilityMemory_, tickUtilityDecisions);
        const FrameFn frameFunction = registry_.Find(frame.id);
        if (frameFunction == nullptr) {
            EmitTrace(result, nextTick_, FrameTraceKind::ExitFailed, frame, FrameControlKind::Fail, frame.id, stack_.size());
            EmitTrace(result, nextTick_, FrameTraceKind::TerminalFailed, frame, FrameControlKind::Fail, frame.id, stack_.size());
            lastOutcome_ = StackRunOutcome::Failed;
            result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
            result.actuationByTick.push_back(actRuntime_->EmittedNow());
            result.tickTrace.push_back(MakeTickTraceEntry(
                nextTick_,
                lastOutcome_,
                stack_,
                blackboard_.DirtySlots(),
                mailbox_.VisibleMessages(),
                tickUtilityDecisions,
                actRuntime_->EmittedNow(),
                actRuntime_->Pending()));
            ++nextTick_;
            return false;
        }

        const FrameControl control = frameFunction(ctx);
        EmitTrace(result, nextTick_, FrameTraceKind::Step, frame, control.kind, control.target, stack_.size());

        if (control.kind == FrameControlKind::Continue) {
            if (!control.stayOnCurrentPc) {
                frame.pc = control.resumePc;
            }
            lastOutcome_ = StackRunOutcome::Continue;
        } else if (control.kind == FrameControlKind::Wait) {
            frame.pc = control.resumePc;
            frame.remainingWaitTicks = control.waitTicks > 0 ? control.waitTicks - 1 : 0;
            lastOutcome_ = StackRunOutcome::Wait;
        } else if (control.kind == FrameControlKind::Push) {
            frame.pc = control.resumePc;
            EmitTrace(result, nextTick_, FrameTraceKind::Push, frame, control.kind, control.target, stack_.size());
            stack_.push_back(StackFrameChunkEntry{ .id = control.target });
            lastOutcome_ = StackRunOutcome::Continue;
        } else if (control.kind == FrameControlKind::Replace) {
            const std::size_t depthBeforeReplace = stack_.size();
            const StackFrameChunkEntry replaced = frame;
            EmitTrace(result, nextTick_, FrameTraceKind::Replace, replaced, control.kind, control.target, depthBeforeReplace);
            stack_.pop_back();
            stack_.push_back(StackFrameChunkEntry{ .id = control.target });
            lastOutcome_ = StackRunOutcome::Continue;
        } else if (control.kind == FrameControlKind::Pop || control.kind == FrameControlKind::Complete) {
            const std::size_t depthBeforePop = stack_.size();
            const StackFrameChunkEntry completedFrame = frame;
            EmitTrace(result, nextTick_, FrameTraceKind::ExitCompleted, completedFrame, control.kind, completedFrame.id, depthBeforePop);
            stack_.pop_back();
            EmitTrace(result, nextTick_, FrameTraceKind::Pop, completedFrame, control.kind, completedFrame.id, depthBeforePop);

            if (stack_.empty()) {
                lastOutcome_ = StackRunOutcome::Completed;
                EmitTrace(result, nextTick_, FrameTraceKind::TerminalCompleted, completedFrame, control.kind, completedFrame.id, 0);
            } else {
                lastOutcome_ = StackRunOutcome::Continue;
            }
        } else {
            const std::size_t depthBeforeFail = stack_.size();
            const StackFrameChunkEntry failedFrame = frame;
            EmitTrace(result, nextTick_, FrameTraceKind::ExitFailed, failedFrame, control.kind, failedFrame.id, depthBeforeFail);
            EmitTrace(result, nextTick_, FrameTraceKind::TerminalFailed, failedFrame, control.kind, failedFrame.id, depthBeforeFail);
            lastOutcome_ = StackRunOutcome::Failed;
        }

        result.dirtySlotsByTick.push_back(blackboard_.DirtySlots());
        result.actuationByTick.push_back(actRuntime_->EmittedNow());
        result.tickTrace.push_back(MakeTickTraceEntry(
            nextTick_,
            lastOutcome_,
            stack_,
            blackboard_.DirtySlots(),
            mailbox_.VisibleMessages(),
            tickUtilityDecisions,
            actRuntime_->EmittedNow(),
            actRuntime_->Pending()));
        ++nextTick_;
        return !IsTerminal();
    }

    [[nodiscard]] FrameRegistry StackFrameRuntimeSession::BuildRegistry()
    {
        return BuildFrameRegistry();
    }

    [[nodiscard]] TraceComparisonResult CompareTickTraces(
        const std::vector<TickTraceEntry>& expected,
        const std::vector<TickTraceEntry>& actual)
    {
        TraceComparisonResult comparison;
        comparison.expectedEntryCount = expected.size();
        comparison.actualEntryCount = actual.size();

        const std::size_t minSize = expected.size() < actual.size() ? expected.size() : actual.size();
        for (std::size_t index = 0; index < minSize; ++index) {
            if (!(expected[index] == actual[index])) {
                comparison.matches = false;
                comparison.firstMismatchIndex = index;
                comparison.mismatchReason = "entry content mismatch";
                comparison.expected = expected[index];
                comparison.actual = actual[index];
                return comparison;
            }
        }

        if (expected.size() != actual.size()) {
            comparison.matches = false;
            comparison.firstMismatchIndex = minSize;
            comparison.mismatchReason = "entry count mismatch";
        }

        return comparison;
    }

    [[nodiscard]] std::vector<std::string> SerializeTickTrace(const std::vector<TickTraceEntry>& trace)
    {
        std::vector<std::string> serialized;
        serialized.reserve(trace.size());

        for (const TickTraceEntry& entry : trace) {
            std::string line;
            line += "tick=" + std::to_string(entry.tick);
            line += "|outcome=" + std::to_string(static_cast<int>(entry.outcome));
            line += "|stack=";

            for (const StackFrameChunkEntry& frame : entry.stack) {
                line += std::to_string(static_cast<int>(frame.id));
                line += ",";
                line += std::to_string(frame.pc);
                line += ",";
                line += frame.entered ? "1" : "0";
                line += ",";
                line += std::to_string(frame.remainingWaitTicks);
                line += ";";
            }

            line += "|dirty=";
            for (const std::uint32_t slot : entry.dirtySlots) {
                line += std::to_string(slot);
                line += ";";
            }

            line += "|mailbox=";
            for (const Message& message : entry.visibleMailbox) {
                line += std::to_string(static_cast<int>(message.kind));
                line += ",";
                line += std::to_string(message.value);
                line += ";";
            }

            line += "|utility=";
            for (const UtilityDecisionTraceEntry& decision : entry.utilityDecisions) {
                line += std::to_string(static_cast<int>(decision.decisionFrame));
                line += ">";
                line += std::to_string(static_cast<int>(decision.chosen));
                line += ":";
                line += decision.minCommitBlocked ? "m1" : "m0";
                line += ",";
                line += decision.hysteresisBlocked ? "h1" : "h0";
                line += ",";
                line += decision.tieBreakUsed ? "t1" : "t0";
                line += "[";
                for (const UtilityDecisionCandidateTrace& candidate : decision.candidates) {
                    line += std::to_string(static_cast<int>(candidate.target));
                    line += "=";
                    line += std::to_string(candidate.score);
                    line += ";";
                }
                line += "]";
            }

            line += "|act=";
            for (const ActRequest& request : entry.emittedActuation) {
                line += std::to_string(static_cast<int>(request.id));
                line += ",";
                line += request.deferred ? "d" : "i";
                line += ",";
                line += std::to_string(request.emittedTick);
                line += ",";
                line += std::to_string(request.dueTick);
                line += ",";
                line += std::to_string(request.delayTicks);
                line += ";";
            }

            line += "|pendingAct=";
            for (const ActRequest& request : entry.pendingDeferredActuation) {
                line += std::to_string(static_cast<int>(request.id));
                line += ",";
                line += std::to_string(request.dueTick);
                line += ",";
                line += std::to_string(request.delayTicks);
                line += ";";
            }

            serialized.push_back(line);
        }

        return serialized;
    }

    [[nodiscard]] std::string FormatTraceComparison(const TraceComparisonResult& comparison)
    {
        std::string text = "matches=";
        text += comparison.matches ? "true" : "false";
        text += "\nexpectedCount=" + std::to_string(comparison.expectedEntryCount);
        text += "\nactualCount=" + std::to_string(comparison.actualEntryCount);

        if (!comparison.matches) {
            text += "\nfirstMismatchIndex=" + std::to_string(comparison.firstMismatchIndex);
            text += "\nreason=" + comparison.mismatchReason;
            const std::vector<std::string> expectedEntry = SerializeTickTrace({ comparison.expected });
            const std::vector<std::string> actualEntry = SerializeTickTrace({ comparison.actual });
            text += "\nexpectedEntry=" + (expectedEntry.empty() ? std::string("<none>") : expectedEntry[0]);
            text += "\nactualEntry=" + (actualEntry.empty() ? std::string("<none>") : actualEntry[0]);
        }

        return text;
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
        StackFrameRuntimeSession session(scenario, mailboxInput);
        return session.RunForTicks(tickCount);
    }
}
