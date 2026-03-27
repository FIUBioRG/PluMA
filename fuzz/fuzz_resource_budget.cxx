#include "ResourceBudget.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 8) return 0;

    int workers = (data[0] % 8) + 1;
    size_t memory = static_cast<size_t>(data[1] | (data[2] << 8)) * 1024ULL * 1024;
    int gpu = data[3] % 5;

    parallel::ParallelBlockOptions opts;
    opts.workers = workers;
    opts.memory = memory;
    opts.gpu = gpu;

    parallel::ResourceBudget budget(opts);
    std::vector<parallel::PluginTask> acquired;

    for (size_t i = 4; i < size; i++) {
        uint8_t op = data[i] % 3;

        if (op == 0 && i + 2 < size) {
            parallel::PluginTask task;
            task.name = "T" + std::to_string(i);
            task.memory_hint = static_cast<size_t>(data[i + 1]) * 1024ULL * 1024;
            task.gpu_hint = data[i + 2] % 3;
            i += 2;

            if (budget.can_dispatch(task)) {
                budget.acquire(task);
                acquired.push_back(task);
            }
        } else if (op == 1 && !acquired.empty()) {
            size_t idx = data[i] % acquired.size();
            budget.release(acquired[idx]);
            acquired.erase(acquired.begin() + static_cast<long>(idx));
        } else {
            if (!acquired.empty()) {
                parallel::PluginTask probe;
                probe.memory_hint = static_cast<size_t>(data[i]) * 1024ULL * 1024;
                probe.gpu_hint = 0;
                budget.can_dispatch(probe);
            }
        }

        assert(budget.active_workers() >= 0);
        assert(budget.active_workers() <= budget.max_workers());
        assert(budget.used_memory() <= budget.total_memory());
        assert(budget.used_gpu() <= budget.total_gpu());
    }

    return 0;
}
