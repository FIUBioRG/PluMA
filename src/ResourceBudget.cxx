#include "ResourceBudget.h"

namespace parallel {

ResourceBudget::ResourceBudget(const ParallelBlockOptions& opts)
    : total_memory_(opts.memory)
    , total_gpu_(opts.gpu)
    , max_workers_(opts.workers)
{
}

size_t ResourceBudget::default_memory_per_worker() const {
    if (max_workers_ <= 0) return total_memory_;
    return total_memory_ / static_cast<size_t>(max_workers_);
}

bool ResourceBudget::can_dispatch(const PluginTask& task) const {
    if (active_workers_ >= max_workers_) return false;

    size_t mem = task.memory_hint > 0 ? task.memory_hint : default_memory_per_worker();
    if (used_memory_ + mem > total_memory_) return false;

    if (used_gpu_ + task.gpu_hint > total_gpu_) return false;

    return true;
}

void ResourceBudget::acquire(const PluginTask& task) {
    size_t mem = task.memory_hint > 0 ? task.memory_hint : default_memory_per_worker();
    active_workers_++;
    used_memory_ += mem;
    used_gpu_ += task.gpu_hint;
}

void ResourceBudget::release(const PluginTask& task) {
    size_t mem = task.memory_hint > 0 ? task.memory_hint : default_memory_per_worker();
    active_workers_--;
    used_memory_ -= mem;
    used_gpu_ -= task.gpu_hint;
}

size_t ResourceBudget::total_memory()   const { return total_memory_; }
size_t ResourceBudget::used_memory()    const { return used_memory_; }
int    ResourceBudget::total_gpu()      const { return total_gpu_; }
int    ResourceBudget::used_gpu()       const { return used_gpu_; }
int    ResourceBudget::max_workers()    const { return max_workers_; }
int    ResourceBudget::active_workers() const { return active_workers_; }

} // namespace parallel
