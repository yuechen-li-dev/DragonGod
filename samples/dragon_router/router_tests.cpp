#include "router_model.h"

int main()
{
    const dragongod_samples::dragon_router::RouterSampleConfig config{};
    const dragongod_samples::dragon_router::RouterSampleSmokeResult result =
        dragongod_samples::dragon_router::RunRouterSampleSmoke(config);

    if (!dragongod_samples::dragon_router::RouterSampleSmokeSucceeded(result)) {
        return 1;
    }

    return 0;
}
