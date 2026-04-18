#pragma once

#include "runtime.h"

namespace dragongod
{
    class UtilityMemoryStore
    {
    public:
        struct Entry
        {
            FrameId frame = FrameId::RootPushChild;
            bool hasCommitted = false;
            FrameId committed = FrameId::RootPushChild;
            std::uint32_t age = 0;
        };

        [[nodiscard]] Entry* Find(FrameId frame);
        [[nodiscard]] const Entry* Find(FrameId frame) const;
        [[nodiscard]] Entry& Ensure(FrameId frame);
        [[nodiscard]] std::vector<UtilityMemoryChunkEntry> ExportChunk() const;
        void ImportChunk(const std::vector<UtilityMemoryChunkEntry>& chunk);

    private:
        std::vector<Entry> entries_;
    };

    class ActRuntime
    {
    public:
        ActRuntime() = default;

        void BeginTick(TickIndex tick);
        void EmitImmediate(ActId id);
        void ScheduleDeferred(ActId id, std::uint32_t delayTicks);
        void FlushMatured();
        [[nodiscard]] const std::vector<ActRequest>& EmittedNow() const;
        [[nodiscard]] const std::vector<ActRequest>& Pending() const;
        [[nodiscard]] std::vector<DeferredActChunkEntry> ExportDeferredChunk() const;
        void ImportDeferredChunk(const std::vector<DeferredActChunkEntry>& chunk);

    private:
        TickIndex currentTick_ = 0;
        std::vector<ActRequest> emittedNow_;
        std::vector<ActRequest> pending_;
    };

}
