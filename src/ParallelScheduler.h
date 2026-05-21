#ifndef PARALLEL_SCHEDULER_H
#define PARALLEL_SCHEDULER_H

#include "ParallelTypes.h"
#include "ResourceBudget.h"

#include <functional>

namespace parallel {

class ParallelScheduler {
public:
    using WorkerFunction = std::function<int(const PluginTask&)>;

    SchedulerResult run(const ParallelBlock& block, WorkerFunction fn);
};

} // namespace parallel

#endif
