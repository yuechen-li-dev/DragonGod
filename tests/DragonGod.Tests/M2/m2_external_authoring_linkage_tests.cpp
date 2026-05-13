#include "../../Marionette/test_harness.h"
#include "../../../src/DragonGod/runtime_compat.h"

#include <cstdint>

namespace
{
    namespace author_domain
    {
        inline constexpr std::uint64_t FrameDomain = 1700;

        enum class FrameLocalId : std::uint32_t
        {
            Root = 1,
            Child = 2
        };

        [[nodiscard]] constexpr dragongod::FrameId Frame(FrameLocalId id)
        {
            return dragongod::FrameId{
                .domain = FrameDomain,
                .local = static_cast<std::uint32_t>(id)
            };
        }

        namespace Keys
        {
            constexpr dragongod::BbKey<bool> BoolFlag{ .name = "BoolFlag", .slot = 101 };
            constexpr dragongod::BbKey<int> IntValue{ .name = "IntValue", .slot = 102 };
            constexpr dragongod::BbKey<int> ChildObserved{ .name = "ChildObserved", .slot = 103 };
            constexpr dragongod::BbKey<float> FloatSignal{ .name = "FloatSignal", .slot = 104 };
        }

        enum class RootPhase : std::uint32_t
        {
            Start = 0,
            AfterChild = 1
        };

        [[nodiscard]] dragongod::FrameControl Child(dragongod::FrameCtx& ctx)
        {
            bool sawFlag = false;
            const bool hadFlag = ctx.Bb().TryGet(Keys::BoolFlag, sawFlag);
            const int value = ctx.Bb().GetOr(Keys::IntValue, -1);
            const float signal = ctx.Bb().GetOr(Keys::FloatSignal, 0.0f);

            if (!hadFlag || !sawFlag || value != 42 || signal != 0.75f) {
                return dragongod::Dg::Fail(1701);
            }

            ctx.Bb().Set(Keys::ChildObserved, value);
            return dragongod::Dg::Pop();
        }

        [[nodiscard]] dragongod::FrameControl Root(dragongod::FrameCtx& ctx)
        {
            switch (ctx.PcAs<RootPhase>()) {
            case RootPhase::Start:
                ctx.Bb().Set(Keys::BoolFlag, true);
                ctx.Bb().Set(Keys::IntValue, 42);
                ctx.Bb().Set(Keys::FloatSignal, 0.75f);
                return dragongod::Dg::Push(
                    Frame(FrameLocalId::Child),
                    static_cast<std::uint32_t>(RootPhase::AfterChild));
            case RootPhase::AfterChild:
                return dragongod::Dg::Complete();
            default:
                return dragongod::Dg::Fail(1702);
            }
        }
    }
}

FACT(M2d_ExternalAuthoring_BbTemplatesAndDomainFrameHelper_WorkAcrossSessionBoundary)
{
    dragongod::FrameRegistry registry;
    registry.Add(author_domain::Frame(author_domain::FrameLocalId::Root), &author_domain::Root, "author_root");
    registry.Add(author_domain::Frame(author_domain::FrameLocalId::Child), &author_domain::Child, "author_child");

    dragongod::StackFrameSessionInit init{
        .registry = registry,
        .rootFrame = author_domain::Frame(author_domain::FrameLocalId::Root)
    };

    dragongod::StackFrameRuntime runtime;
    const dragongod::FrameRunResult run = runtime.RunForTicks(init, 4);

    ASSERT_TRUE(run.finalOutcome == dragongod::StackRunOutcome::Completed, "author domain flow should complete");
    ASSERT_TRUE(run.finalBlackboard.GetOr(author_domain::Keys::BoolFlag, false), "bool blackboard set/get should work from author-owned TU");
    ASSERT_EQUAL(42, run.finalBlackboard.GetOr(author_domain::Keys::IntValue, 0), "int blackboard set/get should work from author-owned TU");
    ASSERT_EQUAL(0.75f, run.finalBlackboard.GetOr(author_domain::Keys::FloatSignal, 0.0f), "float blackboard set/get should work from author-owned TU");

    int observed = 0;
    ASSERT_TRUE(run.finalBlackboard.TryGet(author_domain::Keys::ChildObserved, observed), "TryGet should read author-owned child write");
    ASSERT_EQUAL(42, observed, "child should persist observed value in blackboard");
}
