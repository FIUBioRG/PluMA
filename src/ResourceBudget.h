#ifndef RESOURCE_BUDGET_H
#define RESOURCE_BUDGET_H

#include "ParallelTypes.h"

#include <cstddef>

namespace parallel {

class ResourceBudget {
public:
    ResourceBudget(const ParallelBlockOptions& opts);

    bool can_dispatch(const PluginTask& task) const;
    void acquire(const PluginTask& task);
    void release(const PluginTask& task);

    size_t default_memory_per_worker() const;

    size_t total_memory() const;
    size_t used_memory() const;
    int total_gpu() const;
    int used_gpu() const;
    int max_workers() const;
    int active_workers() const;

private:
    size_t total_memory_;
    size_t used_memory_ = 0;
    int total_gpu_;
    int used_gpu_ = 0;
    int max_workers_;
    int active_workers_ = 0;
};

} // namespace parallel

#endif
