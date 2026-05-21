#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "ParallelScheduler.h"

#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>
#include <cstdlib>

using namespace parallel;
using Catch::Matchers::WithinAbs;

namespace fs = std::filesystem;

static PluginTask make_task(const std::string& name, size_t mem = 0, int gpu = 0) {
    return {name, "in.csv", "out.csv", mem, gpu};
}

static ParallelBlock make_block(
    std::vector<PluginTask> tasks,
    int workers = 8,
    size_t memory = 32ULL * 1024 * 1024 * 1024,
    int gpu = 0,
    FailMode fail = FailMode::Fast)
{
    ParallelBlock block;
    block.options.workers   = workers;
    block.options.memory    = memory;
    block.options.gpu       = gpu;
    block.options.fail_mode = fail;
    block.tasks = std::move(tasks);
    return block;
}

// ---------------------------------------------------------------------------
// Basic execution
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: single plugin succeeds", "[scheduler]") {
    auto block = make_block({make_task("A")});

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) { return 0; });

    REQUIRE(result.completed.size() == 1);
    REQUIRE(result.failed.empty());
    REQUIRE(result.completed[0].name == "A");
    REQUIRE(result.completed[0].exit_code == 0);
}

TEST_CASE("Scheduler: multiple plugins all succeed", "[scheduler]") {
    auto block = make_block({
        make_task("A"), make_task("B"), make_task("C")
    });

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) { return 0; });

    REQUIRE(result.completed.size() == 3);
    REQUIRE(result.failed.empty());
}

TEST_CASE("Scheduler: empty block is a no-op", "[scheduler]") {
    auto block = make_block({});

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) { return 0; });

    REQUIRE(result.completed.empty());
    REQUIRE(result.failed.empty());
    REQUIRE(result.total_elapsed_seconds < 1.0);
}

// ---------------------------------------------------------------------------
// Concurrency verification via wall clock
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: concurrent execution faster than sequential", "[scheduler][timing]") {
    std::vector<PluginTask> tasks;
    for (int i = 0; i < 4; i++) {
        tasks.push_back(make_task("T" + std::to_string(i)));
    }
    auto block = make_block(std::move(tasks), /*workers=*/4);

    ParallelScheduler scheduler;
    auto start = std::chrono::steady_clock::now();
    auto result = scheduler.run(block, [](const PluginTask&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return 0;
    });
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    REQUIRE(result.completed.size() == 4);
    REQUIRE(result.failed.empty());
    // 4 tasks * 0.5s each sequential = 2s; parallel with 4 workers ≈ 0.5s
    // Allow generous margin for fork overhead
    REQUIRE(elapsed < 1.5);
}

TEST_CASE("Scheduler: workers=1 forces sequential execution", "[scheduler][timing]") {
    std::vector<PluginTask> tasks;
    for (int i = 0; i < 3; i++) {
        tasks.push_back(make_task("T" + std::to_string(i)));
    }
    auto block = make_block(std::move(tasks), /*workers=*/1);

    ParallelScheduler scheduler;
    auto start = std::chrono::steady_clock::now();
    auto result = scheduler.run(block, [](const PluginTask&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return 0;
    });
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    REQUIRE(result.completed.size() == 3);
    // 3 * 0.2s = 0.6s minimum; should be >= 0.5s (allowing small tolerance)
    REQUIRE(elapsed >= 0.5);
}

// ---------------------------------------------------------------------------
// Memory budget limits concurrency
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: memory budget throttles concurrency", "[scheduler][resources]") {
    // 4G total budget, each task wants 2G → at most 2 concurrent
    std::vector<PluginTask> tasks;
    for (int i = 0; i < 4; i++) {
        tasks.push_back(make_task("T" + std::to_string(i), 2ULL * 1024 * 1024 * 1024));
    }
    auto block = make_block(
        std::move(tasks),
        /*workers=*/4,
        /*memory=*/4ULL * 1024 * 1024 * 1024
    );

    ParallelScheduler scheduler;
    auto start = std::chrono::steady_clock::now();
    auto result = scheduler.run(block, [](const PluginTask&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return 0;
    });
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    REQUIRE(result.completed.size() == 4);
    // 4 tasks, max 2 concurrent → 2 rounds of 0.3s = 0.6s minimum
    REQUIRE(elapsed >= 0.5);
    // But should be less than fully sequential (4 * 0.3s = 1.2s)
    REQUIRE(elapsed < 1.1);
}

// ---------------------------------------------------------------------------
// GPU budget limits concurrency
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: GPU budget throttles concurrency", "[scheduler][resources]") {
    // 1 GPU available, 2 tasks each need 1 GPU → sequential
    auto block = make_block(
        {make_task("G1", 0, 1), make_task("G2", 0, 1)},
        /*workers=*/4,
        /*memory=*/32ULL * 1024 * 1024 * 1024,
        /*gpu=*/1
    );

    ParallelScheduler scheduler;
    auto start = std::chrono::steady_clock::now();
    auto result = scheduler.run(block, [](const PluginTask&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return 0;
    });
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    REQUIRE(result.completed.size() == 2);
    // Forced sequential by GPU → 2 * 0.3s = 0.6s minimum
    REQUIRE(elapsed >= 0.5);
}

// ---------------------------------------------------------------------------
// Failure modes
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: plugin failure reported in failed list", "[scheduler][failure]") {
    auto block = make_block({make_task("Bad")});

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) { return 1; });

    REQUIRE(result.completed.empty());
    REQUIRE(result.failed.size() == 1);
    REQUIRE(result.failed[0].name == "Bad");
    REQUIRE(result.failed[0].exit_code != 0);
}

TEST_CASE("Scheduler: fail=fast aborts on first failure", "[scheduler][failure]") {
    // 4 tasks, first one fails immediately, rest sleep 2s
    std::vector<PluginTask> tasks = {
        make_task("Fail"), make_task("Slow1"), make_task("Slow2"), make_task("Slow3")
    };
    auto block = make_block(std::move(tasks), /*workers=*/4);
    block.options.fail_mode = FailMode::Fast;

    ParallelScheduler scheduler;
    auto start = std::chrono::steady_clock::now();
    auto result = scheduler.run(block, [](const PluginTask& t) {
        if (t.name == "Fail") return 1;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return 0;
    });
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    REQUIRE_FALSE(result.failed.empty());
    // Should not wait 5s for the slow tasks — abort quickly after the failure
    REQUIRE(elapsed < 3.0);
}

TEST_CASE("Scheduler: fail=continue runs all plugins", "[scheduler][failure]") {
    std::vector<PluginTask> tasks = {
        make_task("Fail1"), make_task("Ok1"), make_task("Fail2"), make_task("Ok2")
    };
    auto block = make_block(std::move(tasks), /*workers=*/4);
    block.options.fail_mode = FailMode::Continue;

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask& t) {
        if (t.name.find("Fail") != std::string::npos) return 1;
        return 0;
    });

    REQUIRE(result.failed.size() == 2);
    REQUIRE(result.completed.size() == 2);
}

TEST_CASE("Scheduler: fail=continue collects all failure names", "[scheduler][failure]") {
    std::vector<PluginTask> tasks = {
        make_task("Bad1"), make_task("Bad2"), make_task("Good")
    };
    auto block = make_block(std::move(tasks), /*workers=*/4);
    block.options.fail_mode = FailMode::Continue;

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask& t) {
        return t.name.find("Bad") != std::string::npos ? 1 : 0;
    });

    REQUIRE(result.failed.size() == 2);
    bool found_bad1 = false, found_bad2 = false;
    for (auto& f : result.failed) {
        if (f.name == "Bad1") found_bad1 = true;
        if (f.name == "Bad2") found_bad2 = true;
    }
    REQUIRE(found_bad1);
    REQUIRE(found_bad2);
}

// ---------------------------------------------------------------------------
// Exit codes
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: exit code 0 is success", "[scheduler][exit]") {
    auto block = make_block({make_task("Ok")});

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) { return 0; });

    REQUIRE(result.completed.size() == 1);
    REQUIRE(result.completed[0].exit_code == 0);
}

TEST_CASE("Scheduler: non-zero exit code is failure", "[scheduler][exit]") {
    auto block = make_block({make_task("Bad")});

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) { return 42; });

    REQUIRE(result.failed.size() == 1);
    REQUIRE(result.failed[0].exit_code != 0);
}

// ---------------------------------------------------------------------------
// Elapsed time tracking
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: result tracks total elapsed time", "[scheduler][timing]") {
    auto block = make_block({make_task("A")});

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        return 0;
    });

    REQUIRE(result.total_elapsed_seconds >= 0.15);
    REQUIRE(result.total_elapsed_seconds < 2.0);
}

TEST_CASE("Scheduler: per-plugin elapsed time tracked", "[scheduler][timing]") {
    auto block = make_block({make_task("A")});

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return 0;
    });

    REQUIRE(result.completed.size() == 1);
    REQUIRE(result.completed[0].elapsed_seconds >= 0.25);
    REQUIRE(result.completed[0].elapsed_seconds < 2.0);
}

// ---------------------------------------------------------------------------
// Process isolation: worker function runs in forked child
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: worker runs in separate process (parent state untouched)", "[scheduler][isolation]") {
    auto block = make_block({make_task("A")});
    int parent_value = 42;

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [&parent_value](const PluginTask&) {
        parent_value = 999;  // modifies child's copy only
        return 0;
    });

    REQUIRE(result.completed.size() == 1);
    REQUIRE(parent_value == 42);  // parent's value unchanged
}

// ---------------------------------------------------------------------------
// Stress: many plugins
// ---------------------------------------------------------------------------

TEST_CASE("Scheduler: handles 20 plugins with worker pool", "[scheduler][stress]") {
    std::vector<PluginTask> tasks;
    for (int i = 0; i < 20; i++) {
        tasks.push_back(make_task("P" + std::to_string(i)));
    }
    auto block = make_block(std::move(tasks), /*workers=*/4);

    ParallelScheduler scheduler;
    auto result = scheduler.run(block, [](const PluginTask&) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 0;
    });

    REQUIRE(result.completed.size() == 20);
    REQUIRE(result.failed.empty());
}
