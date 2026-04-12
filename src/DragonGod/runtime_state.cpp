#include "runtime_internal.h"

namespace dragongod
{
    namespace
    {
        [[nodiscard]] float ClampScore01(float value)
        {
            if (value < 0.0f) {
                return 0.0f;
            }

            if (value > 1.0f) {
                return 1.0f;
            }

            return value;
        }
    }

    [[nodiscard]] UtilityMemoryStore::Entry* UtilityMemoryStore::Find(FrameId frame)
    {
        for (Entry& entry : entries_) {
            if (entry.frame == frame) {
                return &entry;
            }
        }

        return nullptr;
    }

    [[nodiscard]] const UtilityMemoryStore::Entry* UtilityMemoryStore::Find(FrameId frame) const
    {
        for (const Entry& entry : entries_) {
            if (entry.frame == frame) {
                return &entry;
            }
        }

        return nullptr;
    }

    [[nodiscard]] UtilityMemoryStore::Entry& UtilityMemoryStore::Ensure(FrameId frame)
    {
        Entry* found = Find(frame);
        if (found != nullptr) {
            return *found;
        }

        entries_.push_back(Entry{ .frame = frame });
        return entries_.back();
    }

    [[nodiscard]] std::vector<UtilityMemoryChunkEntry> UtilityMemoryStore::ExportChunk() const
    {
        std::vector<UtilityMemoryChunkEntry> chunk;
        for (const Entry& entry : entries_) {
            chunk.push_back(UtilityMemoryChunkEntry{
                .frame = entry.frame,
                .hasCommitted = entry.hasCommitted,
                .committed = entry.committed,
                .age = entry.age
            });
        }

        return chunk;
    }

    void UtilityMemoryStore::ImportChunk(const std::vector<UtilityMemoryChunkEntry>& chunk)
    {
        entries_.clear();
        for (const UtilityMemoryChunkEntry& entry : chunk) {
            entries_.push_back(Entry{
                .frame = entry.frame,
                .hasCommitted = entry.hasCommitted,
                .committed = entry.committed,
                .age = entry.age
            });
        }
    }

    void ActRuntime::BeginTick(TickIndex tick)
    {
        currentTick_ = tick;
        emittedNow_.clear();
    }

    void ActRuntime::EmitImmediate(ActId id)
    {
        emittedNow_.push_back(ActRequest{
            .id = id,
            .deferred = false,
            .emittedTick = currentTick_,
            .dueTick = currentTick_,
            .delayTicks = 0
        });
    }

    void ActRuntime::ScheduleDeferred(ActId id, std::uint32_t delayTicks)
    {
        pending_.push_back(ActRequest{
            .id = id,
            .deferred = true,
            .emittedTick = currentTick_,
            .dueTick = currentTick_ + static_cast<TickIndex>(delayTicks),
            .delayTicks = delayTicks
        });
    }

    void ActRuntime::FlushMatured()
    {
        std::vector<ActRequest> stillPending;
        for (const ActRequest& request : pending_) {
            if (request.dueTick <= currentTick_) {
                emittedNow_.push_back(request);
            } else {
                stillPending.push_back(request);
            }
        }

        pending_ = stillPending;
    }

    [[nodiscard]] const std::vector<ActRequest>& ActRuntime::EmittedNow() const
    {
        return emittedNow_;
    }

    [[nodiscard]] const std::vector<ActRequest>& ActRuntime::Pending() const
    {
        return pending_;
    }

    [[nodiscard]] std::vector<DeferredActChunkEntry> ActRuntime::ExportDeferredChunk() const
    {
        std::vector<DeferredActChunkEntry> chunk;
        for (const ActRequest& request : pending_) {
            chunk.push_back(DeferredActChunkEntry{
                .id = request.id,
                .dueTick = request.dueTick,
                .emittedTick = request.emittedTick,
                .delayTicks = request.delayTicks
            });
        }

        return chunk;
    }

    void ActRuntime::ImportDeferredChunk(const std::vector<DeferredActChunkEntry>& chunk)
    {
        pending_.clear();
        emittedNow_.clear();
        for (const DeferredActChunkEntry& entry : chunk) {
            pending_.push_back(ActRequest{
                .id = entry.id,
                .deferred = true,
                .emittedTick = entry.emittedTick,
                .dueTick = entry.dueTick,
                .delayTicks = entry.delayTicks
            });
        }
    }
    ActCtx::ActCtx() = default;

    ActCtx::ActCtx(ActRuntime& runtime)
        : runtime_(&runtime)
    {
    }

    void ActCtx::Immediate(ActId id)
    {
        if (runtime_ == nullptr) {
            return;
        }

        runtime_->EmitImmediate(id);
    }

    void ActCtx::Deferred(ActId id, std::uint32_t delayTicks)
    {
        if (runtime_ == nullptr) {
            return;
        }

        runtime_->ScheduleDeferred(id, delayTicks);
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

    FrameCtx::FrameCtx(
        FrameId frameId,
        TickIndex tick,
        std::uint32_t pc,
        bool entered,
        Blackboard& blackboard,
        Mailbox& mailbox,
        ActRuntime& actRuntime,
        UtilityMemoryStore& utilityMemory,
        std::vector<UtilityDecisionTraceEntry>& utilityTrace)
        : frameId_(frameId)
        , tick_(tick)
        , pc_(pc)
        , entered_(entered)
        , blackboard_(&blackboard)
        , mailbox_(&mailbox)
        , act_(actRuntime)
        , utilityMemory_(&utilityMemory)
        , utilityTrace_(&utilityTrace)
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

    [[nodiscard]] const Blackboard& FrameCtx::Bb() const
    {
        return *blackboard_;
    }

    [[nodiscard]] Mailbox& FrameCtx::Mb()
    {
        return *mailbox_;
    }

    [[nodiscard]] const Mailbox& FrameCtx::Mb() const
    {
        return *mailbox_;
    }

    [[nodiscard]] ActCtx& FrameCtx::Act()
    {
        return act_;
    }

    [[nodiscard]] const ActCtx& FrameCtx::Act() const
    {
        return act_;
    }

    [[nodiscard]] std::uint32_t FrameCtx::ReadUtilityCommitAge(FrameId frameId) const
    {
        if (utilityMemory_ == nullptr) {
            return 0;
        }

        const UtilityMemoryStore::Entry* entry = utilityMemory_->Find(frameId);
        if (entry == nullptr || !entry->hasCommitted) {
            return 0;
        }

        return entry->age;
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

    [[nodiscard]] const std::vector<Message>& Mailbox::StagedMessages() const
    {
        return staged_;
    }

    [[nodiscard]] MailboxChunk Mailbox::ExportChunk() const
    {
        return MailboxChunk{
            .visibleMessages = visible_,
            .stagedMessages = staged_
        };
    }

    void Mailbox::ImportChunk(const MailboxChunk& chunk)
    {
        visible_ = chunk.visibleMessages;
        staged_ = chunk.stagedMessages;
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

    [[nodiscard]] Blackboard::Chunk Blackboard::ExportChunk() const
    {
        Chunk chunk;

        for (const BoolEntry& entry : boolEntries_) {
            chunk.boolEntries.push_back(BoolChunkEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        for (const IntEntry& entry : intEntries_) {
            chunk.intEntries.push_back(IntChunkEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        chunk.dirtySlots = dirtySlots_;
        return chunk;
    }

    void Blackboard::ImportChunk(const Chunk& chunk)
    {
        boolEntries_.clear();
        intEntries_.clear();
        dirtySlots_.clear();

        for (const BoolChunkEntry& entry : chunk.boolEntries) {
            boolEntries_.push_back(BoolEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        for (const IntChunkEntry& entry : chunk.intEntries) {
            intEntries_.push_back(IntEntry{
                .slot = entry.slot,
                .value = entry.value
            });
        }

        dirtySlots_ = chunk.dirtySlots;
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

    struct DecideAccess
    {
        [[nodiscard]] static UtilityMemoryStore* Memory(FrameCtx& ctx)
        {
            return ctx.utilityMemory_;
        }

        [[nodiscard]] static std::vector<UtilityDecisionTraceEntry>* Trace(FrameCtx& ctx)
        {
            return ctx.utilityTrace_;
        }
    };

    namespace Dg
    {
        [[nodiscard]] UtilityCandidate when(FrameId target, ConsiderationFn consideration)
        {
            return UtilityCandidate{
                .target = target,
                .consideration = consideration
            };
        }

        [[nodiscard]] FrameControl Decide(
            FrameCtx& ctx,
            std::initializer_list<UtilityCandidate> candidates,
            DecideOptions options)
        {
            if (candidates.size() == 0) {
                return Fail(600);
            }

            UtilityMemoryStore* memory = DecideAccess::Memory(ctx);
            std::vector<UtilityDecisionTraceEntry>* trace = DecideAccess::Trace(ctx);
            if (memory == nullptr || trace == nullptr) {
                return Fail(601);
            }

            UtilityDecisionTraceEntry traceEntry;
            traceEntry.decisionFrame = ctx.Id();
            UtilityMemoryStore::Entry& memoryEntry = memory->Ensure(ctx.Id());
            traceEntry.hadCommittedBefore = memoryEntry.hasCommitted;
            traceEntry.committedBefore = memoryEntry.committed;
            traceEntry.committedAgeBefore = memoryEntry.age;

            float bestScore = -1.0f;
            std::vector<std::size_t> bestIndices;
            std::size_t index = 0;
            for (const UtilityCandidate& candidate : candidates) {
                float score = 0.0f;
                if (candidate.consideration != nullptr) {
                    score = ClampScore01(candidate.consideration(ctx));
                }

                traceEntry.candidates.push_back(UtilityDecisionCandidateTrace{
                    .target = candidate.target,
                    .score = score
                });

                if (score > bestScore) {
                    bestScore = score;
                    bestIndices.clear();
                    bestIndices.push_back(index);
                } else if (score == bestScore) {
                    bestIndices.push_back(index);
                }

                ++index;
            }

            std::size_t winnerIndex = bestIndices.front();
            if (bestIndices.size() > 1) {
                traceEntry.tieBreakUsed = true;
                if (options.tieBreak == TieBreakPolicy::LastListed) {
                    winnerIndex = bestIndices.back();
                } else if (options.tieBreak == TieBreakPolicy::KeepCurrent && memoryEntry.hasCommitted) {
                    for (const std::size_t tiedIndex : bestIndices) {
                        if (traceEntry.candidates[tiedIndex].target == memoryEntry.committed) {
                            winnerIndex = tiedIndex;
                            break;
                        }
                    }
                }
            }

            FrameId winner = traceEntry.candidates[winnerIndex].target;
            const float winnerScore = traceEntry.candidates[winnerIndex].score;
            float committedScore = 0.0f;
            bool hasCommittedCandidate = false;

            if (memoryEntry.hasCommitted) {
                for (const UtilityDecisionCandidateTrace& candidate : traceEntry.candidates) {
                    if (candidate.target == memoryEntry.committed) {
                        committedScore = candidate.score;
                        hasCommittedCandidate = true;
                        break;
                    }
                }
            }

            if (memoryEntry.hasCommitted && hasCommittedCandidate && winner != memoryEntry.committed) {
                if (options.minCommitTicks > 0 && memoryEntry.age < static_cast<std::uint32_t>(options.minCommitTicks)) {
                    winner = memoryEntry.committed;
                    traceEntry.minCommitBlocked = true;
                } else if (winnerScore < committedScore + options.hysteresis) {
                    winner = memoryEntry.committed;
                    traceEntry.hysteresisBlocked = true;
                }
            }

            if (memoryEntry.hasCommitted && winner == memoryEntry.committed) {
                ++memoryEntry.age;
            } else {
                memoryEntry.hasCommitted = true;
                memoryEntry.committed = winner;
                memoryEntry.age = 0;
            }

            traceEntry.chosen = winner;
            trace->push_back(traceEntry);
            return Push(winner, ctx.Pc());
        }

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

        [[nodiscard]] FrameControl Stay()
        {
            return FrameControl{
                .kind = FrameControlKind::Continue,
                .stayOnCurrentPc = true
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

    namespace When
    {
        // Built-in proof/demo scorers read canonical fixture keys from the blackboard.
        [[nodiscard]] float Always(const FrameCtx&)
        {
            return 0.25f;
        }

        [[nodiscard]] float HighSignal(const FrameCtx& ctx)
        {
            constexpr BbKey<int> HighSignalScoreKey{ .name = "HighSignalScore", .slot = 8 };
            const int raw = ctx.Bb().GetOr(HighSignalScoreKey, 0);
            return ClampScore01(static_cast<float>(raw) / 100.0f);
        }

        [[nodiscard]] float ResourcePressure(const FrameCtx& ctx)
        {
            constexpr BbKey<int> ResourcePressureScoreKey{ .name = "ResourcePressureScore", .slot = 9 };
            const int raw = ctx.Bb().GetOr(ResourcePressureScoreKey, 0);
            return ClampScore01(static_cast<float>(raw) / 100.0f);
        }
    }
}
