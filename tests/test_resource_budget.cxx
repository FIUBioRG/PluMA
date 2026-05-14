#include <catch2/catch_test_macros.hpp>

#include "ResourceBudget.h"

using namespace parallel;

static ParallelBlockOptions make_opts(int workers, size_t memory, int gpu) {
    ParallelBlockOptions opts;
    opts.workers = workers;
    opts.memory  = memory;
    opts.gpu     = gpu;
    return opts;
}

static PluginTask make_task(const std::string& name, size_t mem = 0, int gpu = 0) {
    PluginTask t;
    t.name        = name;
    t.inputfile   = "in.csv";
    t.outputfile  = "out.csv";
    t.memory_hint = mem;
    t.gpu_hint    = gpu;
    return t;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("ResourceBudget: constructed from options", "[budget]") {
    constexpr size_t MEM = 32ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 2));

    REQUIRE(budget.max_workers()    == 4);
    REQUIRE(budget.total_memory()   == MEM);
    REQUIRE(budget.total_gpu()      == 2);
    REQUIRE(budget.active_workers() == 0);
    REQUIRE(budget.used_memory()    == 0);
    REQUIRE(budget.used_gpu()       == 0);
}

// ---------------------------------------------------------------------------
// default_memory_per_worker
// ---------------------------------------------------------------------------

TEST_CASE("ResourceBudget: default memory per worker divides total by workers", "[budget]") {
    constexpr size_t MEM = 32ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 0));
    REQUIRE(budget.default_memory_per_worker() == MEM / 4);
}

TEST_CASE("ResourceBudget: default memory per worker with 1 worker", "[budget]") {
    constexpr size_t MEM = 8ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(1, MEM, 0));
    REQUIRE(budget.default_memory_per_worker() == MEM);
}

// ---------------------------------------------------------------------------
// can_dispatch: worker slots
// ---------------------------------------------------------------------------

TEST_CASE("ResourceBudget: can_dispatch true when resources available", "[budget][dispatch]") {
    ResourceBudget budget(make_opts(4, 32ULL * 1024 * 1024 * 1024, 2));
    auto task = make_task("A", 2ULL * 1024 * 1024 * 1024, 1);
    REQUIRE(budget.can_dispatch(task));
}

TEST_CASE("ResourceBudget: can_dispatch false when workers exhausted", "[budget][dispatch]") {
    ResourceBudget budget(make_opts(2, 32ULL * 1024 * 1024 * 1024, 0));
    auto t1 = make_task("A", 1ULL * 1024 * 1024 * 1024);
    auto t2 = make_task("B", 1ULL * 1024 * 1024 * 1024);
    auto t3 = make_task("C", 1ULL * 1024 * 1024 * 1024);

    budget.acquire(t1);
    budget.acquire(t2);
    REQUIRE_FALSE(budget.can_dispatch(t3));
}

TEST_CASE("ResourceBudget: can_dispatch false when memory exhausted", "[budget][dispatch]") {
    constexpr size_t MEM = 4ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(8, MEM, 0));
    auto heavy = make_task("Heavy", 3ULL * 1024 * 1024 * 1024);
    auto second = make_task("Second", 2ULL * 1024 * 1024 * 1024);

    budget.acquire(heavy);
    REQUIRE_FALSE(budget.can_dispatch(second));
}

TEST_CASE("ResourceBudget: can_dispatch false when GPU exhausted", "[budget][dispatch]") {
    ResourceBudget budget(make_opts(8, 32ULL * 1024 * 1024 * 1024, 1));
    auto gpu_task  = make_task("GPU1", 0, 1);
    auto gpu_task2 = make_task("GPU2", 0, 1);

    budget.acquire(gpu_task);
    REQUIRE_FALSE(budget.can_dispatch(gpu_task2));
}

TEST_CASE("ResourceBudget: task with zero memory hint uses default", "[budget][dispatch]") {
    constexpr size_t MEM = 8ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 0));
    auto task = make_task("NoHint", 0, 0);

    budget.acquire(task);
    REQUIRE(budget.used_memory() == budget.default_memory_per_worker());
}

// ---------------------------------------------------------------------------
// acquire / release
// ---------------------------------------------------------------------------

TEST_CASE("ResourceBudget: acquire increments counters", "[budget][acquire]") {
    constexpr size_t MEM = 32ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 2));
    auto task = make_task("A", 4ULL * 1024 * 1024 * 1024, 1);

    budget.acquire(task);
    REQUIRE(budget.active_workers() == 1);
    REQUIRE(budget.used_memory()    == 4ULL * 1024 * 1024 * 1024);
    REQUIRE(budget.used_gpu()       == 1);
}

TEST_CASE("ResourceBudget: release decrements counters", "[budget][release]") {
    constexpr size_t MEM = 32ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 2));
    auto task = make_task("A", 4ULL * 1024 * 1024 * 1024, 1);

    budget.acquire(task);
    budget.release(task);
    REQUIRE(budget.active_workers() == 0);
    REQUIRE(budget.used_memory()    == 0);
    REQUIRE(budget.used_gpu()       == 0);
}

TEST_CASE("ResourceBudget: multiple acquire and release cycle", "[budget][lifecycle]") {
    constexpr size_t MEM = 16ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 2));

    auto t1 = make_task("A", 4ULL * 1024 * 1024 * 1024, 1);
    auto t2 = make_task("B", 8ULL * 1024 * 1024 * 1024, 1);
    auto t3 = make_task("C", 2ULL * 1024 * 1024 * 1024, 0);

    budget.acquire(t1);
    budget.acquire(t2);
    REQUIRE(budget.active_workers() == 2);
    REQUIRE(budget.used_memory()    == 12ULL * 1024 * 1024 * 1024);
    REQUIRE(budget.used_gpu()       == 2);

    REQUIRE(budget.can_dispatch(t3));

    budget.acquire(t3);
    REQUIRE(budget.active_workers() == 3);

    budget.release(t1);
    REQUIRE(budget.active_workers() == 2);
    REQUIRE(budget.used_memory()    == 10ULL * 1024 * 1024 * 1024);
    REQUIRE(budget.used_gpu()       == 1);

    budget.release(t2);
    budget.release(t3);
    REQUIRE(budget.active_workers() == 0);
    REQUIRE(budget.used_memory()    == 0);
    REQUIRE(budget.used_gpu()       == 0);
}

TEST_CASE("ResourceBudget: cannot dispatch when exactly at memory limit", "[budget][edge]") {
    constexpr size_t MEM = 4ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 0));
    auto t1 = make_task("Full", 4ULL * 1024 * 1024 * 1024);
    auto t2 = make_task("One", 1ULL * 1024 * 1024 * 1024);

    budget.acquire(t1);
    REQUIRE_FALSE(budget.can_dispatch(t2));
}

TEST_CASE("ResourceBudget: can dispatch when exactly fitting remaining budget", "[budget][edge]") {
    constexpr size_t MEM = 4ULL * 1024 * 1024 * 1024;
    ResourceBudget budget(make_opts(4, MEM, 0));
    auto t1 = make_task("Half", 2ULL * 1024 * 1024 * 1024);
    auto t2 = make_task("Other", 2ULL * 1024 * 1024 * 1024);

    budget.acquire(t1);
    REQUIRE(budget.can_dispatch(t2));
}
