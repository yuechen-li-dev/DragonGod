#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/runtime_compat.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{
    namespace author_a
    {
        inline constexpr std::uint64_t ActDomain = 0xA11CE;
        enum class Local : std::uint32_t { SharedLocal = 1 };
        [[nodiscard]] constexpr dragongod::ActId Act(Local id)
        {
            return dragongod::ActId{ .domain = ActDomain, .local = static_cast<std::uint32_t>(id) };
        }
    }

    namespace author_b
    {
        inline constexpr std::uint64_t ActDomain = 0xB0B;
        enum class Local : std::uint32_t { SharedLocal = 1 };
        [[nodiscard]] constexpr dragongod::ActId Act(Local id)
        {
            return dragongod::ActId{ .domain = ActDomain, .local = static_cast<std::uint32_t>(id) };
        }
    }

    namespace author_frames
    {
        inline constexpr dragongod::FrameId Root = { .domain = 0xD00D, .local = 1 };
        [[nodiscard]] dragongod::FrameControl EmitTwoDomains(dragongod::FrameCtx& ctx)
        {
            ctx.Act().Immediate(author_a::Act(author_a::Local::SharedLocal));
            ctx.Act().Deferred(author_b::Act(author_b::Local::SharedLocal), 1);
            return dragongod::Dg::Complete();
        }
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

FACT(M7_CanonicalActuationAuthorShape_ImmediateAndDeferredAreReal)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::ActImmediateDeferredComplete,
        8,
        dragongod::RuntimeMailboxInput{});

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "canonical actuation scenario should complete");

    bool sawImmediate = false;
    bool sawDeferredMatured = false;
    for (const std::vector<dragongod::ActRequest>& tickActs : run.actuationByTick) {
        for (const dragongod::ActRequest& request : tickActs) {
            if (!request.deferred && request.id == dragongod::CanonicalActIds::PlayBark) {
                sawImmediate = true;
            }

            if (request.deferred && request.id == dragongod::CanonicalActIds::RaiseAlarm) {
                sawDeferredMatured = true;
            }
        }
    }

    ASSERT_TRUE(sawImmediate, "frame-authored ctx.Act().Immediate(...) should emit explicit request evidence");
    ASSERT_TRUE(sawDeferredMatured, "frame-authored ctx.Act().Deferred(...) should mature into explicit request evidence");
}

FACT(M7_ImmediateAndDeferredTiming_AreDeterministicAndExplicit)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::ActImmediateDeferredComplete,
        8,
        dragongod::RuntimeMailboxInput{});

    ASSERT_TRUE(run.tickTrace.size() >= static_cast<std::size_t>(6), "scenario should run long enough to observe deferred maturity window");
    if (run.tickTrace.size() < static_cast<std::size_t>(6)) {
        return;
    }

    ASSERT_EQUAL(static_cast<std::size_t>(1), run.tickTrace[0].emittedActuation.size(), "tick 0 should emit immediate request only");
    ASSERT_TRUE(!run.tickTrace[0].emittedActuation[0].deferred, "tick 0 request should be immediate mode");
    ASSERT_EQUAL(static_cast<std::size_t>(0), run.tickTrace[1].emittedActuation.size(), "tick 1 should have no actuation emissions");
    ASSERT_EQUAL(static_cast<std::size_t>(1), run.tickTrace[2].pendingDeferredActuation.size(), "tick 2 should include one pending deferred request");
    ASSERT_EQUAL(static_cast<std::size_t>(0), run.tickTrace[3].emittedActuation.size(), "tick 3 should not mature deferred request early");
    ASSERT_EQUAL(static_cast<std::size_t>(1), run.tickTrace[5].emittedActuation.size(), "tick 5 should emit matured deferred request");
    ASSERT_TRUE(run.tickTrace[5].emittedActuation[0].deferred, "tick 5 emission should be deferred-mode request");
    ASSERT_EQUAL(static_cast<std::size_t>(0), run.tickTrace[5].pendingDeferredActuation.size(), "matured deferred request should leave pending queue");
}

FACT(M7_ImmediateDeferredOrdering_IsDeterministic)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(
        dragongod::StackScriptScenario::ActOrderedDeferredComplete,
        5,
        dragongod::RuntimeMailboxInput{});

    ASSERT_TRUE(run.tickTrace.size() >= static_cast<std::size_t>(3), "ordered scenario should provide maturity ticks");
    if (run.tickTrace.size() < static_cast<std::size_t>(3)) {
        return;
    }
    ASSERT_EQUAL(static_cast<std::size_t>(1), run.tickTrace[0].emittedActuation.size(), "tick 0 should emit one immediate request");
    ASSERT_TRUE(
        run.tickTrace[0].emittedActuation[0].id == dragongod::CanonicalActIds::OpenDoor,
        "immediate request ordering should preserve authored order");

    ASSERT_EQUAL(static_cast<std::size_t>(2), run.tickTrace[2].emittedActuation.size(), "tick 2 should emit both matured deferred requests");
    ASSERT_TRUE(
        run.tickTrace[2].emittedActuation[0].id == dragongod::CanonicalActIds::PlayBark,
        "first deferred emission order should match scheduling order");
    ASSERT_TRUE(
        run.tickTrace[2].emittedActuation[1].id == dragongod::CanonicalActIds::RaiseAlarm,
        "second deferred emission order should match scheduling order");
}

FACT(M7_DeferredActuation_PersistsAcrossChunkSaveRestore)
{
    dragongod::StackFrameRuntimeSession uninterrupted(
        dragongod::StackScriptScenario::ActImmediateDeferredComplete,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult uninterruptedRun = uninterrupted.RunForTicks(8);

    dragongod::StackFrameRuntimeSession split(
        dragongod::StackScriptScenario::ActImmediateDeferredComplete,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult firstLeg = split.RunForTicks(3);
    const dragongod::RuntimeChunk snapshot = split.Save();
    dragongod::StackFrameRuntimeSession restored(snapshot);
    const dragongod::FrameRunResult secondLeg = restored.RunForTicks(5);

    ASSERT_EQUAL(static_cast<std::size_t>(1), snapshot.deferredActuation.size(), "snapshot should persist one pending deferred request");
    ASSERT_TRUE(snapshot.deferredActuation[0].id == dragongod::CanonicalActIds::RaiseAlarm, "snapshot should preserve canonical domain/local act identity");

    std::vector<dragongod::TickTraceEntry> restoredTrace;
    AppendTickTrace(restoredTrace, firstLeg.tickTrace);
    AppendTickTrace(restoredTrace, secondLeg.tickTrace);

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(uninterruptedRun.tickTrace, restoredTrace);
    ASSERT_TRUE(comparison.matches, "save/restore should preserve deferred actuation behavior and ordering");
}

FACT(M7_AuthorOwnedActDomains_CanShareLocalIdsWithoutCollision)
{
    dragongod::FrameRegistry registry;
    registry.Add(author_frames::Root, &author_frames::EmitTwoDomains, "author_emit_two_domains");

    dragongod::StackFrameRuntimeSession session(dragongod::StackFrameSessionInit{
        .registry = registry,
        .rootFrame = author_frames::Root,
        .mailboxInput = dragongod::RuntimeMailboxInput{}
    });

    const dragongod::FrameRunResult run = session.RunForTicks(2);
    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "author-owned root should complete");
    ASSERT_EQUAL(static_cast<std::size_t>(2), run.actuationByTick.size(), "two ticks should include immediate and matured deferred emissions");
    ASSERT_EQUAL(static_cast<std::size_t>(1), run.actuationByTick[0].size(), "tick 0 should include immediate author act");
    ASSERT_EQUAL(static_cast<std::size_t>(1), run.actuationByTick[1].size(), "tick 1 should include matured deferred author act");
    ASSERT_TRUE(run.actuationByTick[0][0].id == author_a::Act(author_a::Local::SharedLocal), "domain A act id should be preserved");
    ASSERT_TRUE(run.actuationByTick[1][0].id == author_b::Act(author_b::Local::SharedLocal), "domain B act id should be preserved");

    const std::vector<std::string> serialized = dragongod::SerializeTickTrace(run.tickTrace);
    ASSERT_TRUE(serialized.size() >= static_cast<std::size_t>(2), "serialized trace should include both ticks");
    if (serialized.size() >= static_cast<std::size_t>(2)) {
        ASSERT_TRUE(serialized[0].find("659918:1") != std::string::npos, "unknown domain act should serialize as domain:local");
        ASSERT_TRUE(serialized[1].find("2827:1") != std::string::npos, "second unknown domain act should serialize as domain:local");
    }
}

FACT(M7_TraceReplayIncludesActuation_AndRepeatedRunsDoNotDrift)
{
    const dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult runA = runtime.RunForTicks(
        dragongod::StackScriptScenario::ActOrderedDeferredComplete,
        5,
        dragongod::RuntimeMailboxInput{});
    const dragongod::FrameRunResult runB = runtime.RunForTicks(
        dragongod::StackScriptScenario::ActOrderedDeferredComplete,
        5,
        dragongod::RuntimeMailboxInput{});

    const dragongod::TraceComparisonResult comparison = dragongod::CompareTickTraces(runA.tickTrace, runB.tickTrace);
    ASSERT_TRUE(comparison.matches, "actuation should participate in deterministic replay-comparable tick trace");
}

FACT(M7_ActuationCoexistsWithStackBlackboardMailboxAndUtility)
{
    const dragongod::StackFrameRuntime runtime;

    dragongod::RuntimeMailboxInput mailboxInput;
    mailboxInput.initialMessages = {
        dragongod::Message{ .kind = dragongod::MessageKind::Signal, .value = 44 }
    };

    const dragongod::FrameRunResult stackMailboxRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::ActParentPushChildComplete,
        8,
        mailboxInput);
    const dragongod::FrameRunResult utilityRun = runtime.RunForTicks(
        dragongod::StackScriptScenario::ActUtilityDrivenComplete,
        4,
        dragongod::RuntimeMailboxInput{});

    ASSERT_TRUE(stackMailboxRun.finalOutcome == dragongod::StackRunOutcome::Completed, "parent/child actuation scenario should complete");
    ASSERT_TRUE(utilityRun.finalOutcome == dragongod::StackRunOutcome::Completed, "utility-driven actuation scenario should complete");

    bool sawPush = false;
    bool sawPop = false;
    for (const dragongod::FrameTraceEvent& event : stackMailboxRun.trace) {
        if (event.kind == dragongod::FrameTraceKind::Push && event.targetFrame == dragongod::CanonicalFrameIds::ChildActImmediate) {
            sawPush = true;
        }

        if (event.kind == dragongod::FrameTraceKind::Pop && event.activeFrame == dragongod::CanonicalFrameIds::ChildActImmediate) {
            sawPop = true;
        }
    }

    ASSERT_TRUE(sawPush, "actuation frames should preserve ordinary stack push semantics");
    ASSERT_TRUE(sawPop, "actuation frames should preserve ordinary stack pop semantics");
    ASSERT_TRUE(!stackMailboxRun.dirtySlotsByTick.empty(), "actuation should still preserve dirty evidence");
    ASSERT_TRUE(!stackMailboxRun.visibleMailboxByTick.empty(), "actuation should coexist with mailbox visibility snapshots");
    ASSERT_TRUE(stackMailboxRun.finalBlackboard.GetOr(dragongod::BbKey<bool>{ .name = "ActMailboxSeen", .slot = 12 }, false), "mailbox consumption should remain explicit in frame logic");
    ASSERT_TRUE(!utilityRun.actuationByTick.empty(), "utility-driven frame should still emit explicit actuation requests through ctx.Act()");
}
