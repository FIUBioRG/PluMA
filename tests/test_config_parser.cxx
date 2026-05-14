#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "ConfigParser.h"

#include <sstream>

using namespace parallel;
using Catch::Matchers::ContainsSubstring;

// ---------------------------------------------------------------------------
// parse_size
// ---------------------------------------------------------------------------

TEST_CASE("parse_size: gigabytes", "[config][size]") {
    REQUIRE(parse_size("1G")  == 1ULL * 1024 * 1024 * 1024);
    REQUIRE(parse_size("32G") == 32ULL * 1024 * 1024 * 1024);
}

TEST_CASE("parse_size: megabytes", "[config][size]") {
    REQUIRE(parse_size("512M") == 512ULL * 1024 * 1024);
    REQUIRE(parse_size("1M")   == 1ULL * 1024 * 1024);
}

TEST_CASE("parse_size: terabytes", "[config][size]") {
    REQUIRE(parse_size("1T") == 1ULL * 1024 * 1024 * 1024 * 1024);
}

TEST_CASE("parse_size: raw bytes (no suffix)", "[config][size]") {
    REQUIRE(parse_size("4096") == 4096);
    REQUIRE(parse_size("0")    == 0);
}

TEST_CASE("parse_size: case insensitive suffix", "[config][size]") {
    REQUIRE(parse_size("2g") == 2ULL * 1024 * 1024 * 1024);
    REQUIRE(parse_size("4m") == 4ULL * 1024 * 1024);
    REQUIRE(parse_size("1t") == 1ULL * 1024 * 1024 * 1024 * 1024);
}

TEST_CASE("parse_size: invalid input throws", "[config][size]") {
    REQUIRE_THROWS(parse_size(""));
    REQUIRE_THROWS(parse_size("abc"));
    REQUIRE_THROWS(parse_size("G"));
    REQUIRE_THROWS(parse_size("-1G"));
}

// ---------------------------------------------------------------------------
// parse_parallel_options
// ---------------------------------------------------------------------------

TEST_CASE("parse_parallel_options: defaults on bare Parallel line", "[config][options]") {
    auto opts = parse_parallel_options("Parallel");
    REQUIRE(opts.workers   == 0);
    REQUIRE(opts.memory    == 0);
    REQUIRE(opts.gpu       == 0);
    REQUIRE(opts.fail_mode == FailMode::Fast);
}

TEST_CASE("parse_parallel_options: workers only", "[config][options]") {
    auto opts = parse_parallel_options("Parallel workers=4");
    REQUIRE(opts.workers == 4);
    REQUIRE(opts.memory  == 0);
    REQUIRE(opts.gpu     == 0);
}

TEST_CASE("parse_parallel_options: all options", "[config][options]") {
    auto opts = parse_parallel_options("Parallel workers=4 memory=32G gpu=2 fail=continue");
    REQUIRE(opts.workers   == 4);
    REQUIRE(opts.memory    == 32ULL * 1024 * 1024 * 1024);
    REQUIRE(opts.gpu       == 2);
    REQUIRE(opts.fail_mode == FailMode::Continue);
}

TEST_CASE("parse_parallel_options: fail=fast explicit", "[config][options]") {
    auto opts = parse_parallel_options("Parallel fail=fast");
    REQUIRE(opts.fail_mode == FailMode::Fast);
}

TEST_CASE("parse_parallel_options: options in any order", "[config][options]") {
    auto opts = parse_parallel_options("Parallel gpu=1 workers=8 memory=16G");
    REQUIRE(opts.workers == 8);
    REQUIRE(opts.memory  == 16ULL * 1024 * 1024 * 1024);
    REQUIRE(opts.gpu     == 1);
}

TEST_CASE("parse_parallel_options: unknown option is ignored", "[config][options]") {
    auto opts = parse_parallel_options("Parallel workers=2 color=blue");
    REQUIRE(opts.workers == 2);
}

// ---------------------------------------------------------------------------
// parse_plugin_task
// ---------------------------------------------------------------------------

TEST_CASE("parse_plugin_task: basic plugin line without hints", "[config][task]") {
    auto task = parse_plugin_task(
        "Plugin GenomicsPreprocess inputfile data/raw/g.csv outputfile data/proc/g.csv",
        "pipelines/PD/");

    REQUIRE(task.name       == "GenomicsPreprocess");
    REQUIRE(task.inputfile  == "pipelines/PD/data/raw/g.csv");
    REQUIRE(task.outputfile == "pipelines/PD/data/proc/g.csv");
    REQUIRE(task.memory_hint == 0);
    REQUIRE(task.gpu_hint    == 0);
}

TEST_CASE("parse_plugin_task: with memory hint", "[config][task]") {
    auto task = parse_plugin_task(
        "Plugin Kallisto inputfile raw/rna/ outputfile proc/tx.csv memory=8G",
        "prefix/");

    REQUIRE(task.name        == "Kallisto");
    REQUIRE(task.memory_hint == 8ULL * 1024 * 1024 * 1024);
    REQUIRE(task.gpu_hint    == 0);
}

TEST_CASE("parse_plugin_task: with memory and gpu hints", "[config][task]") {
    auto task = parse_plugin_task(
        "Plugin QLoRA inputfile params.txt outputfile model/ memory=16G gpu=1",
        "");

    REQUIRE(task.name        == "QLoRA");
    REQUIRE(task.memory_hint == 16ULL * 1024 * 1024 * 1024);
    REQUIRE(task.gpu_hint    == 1);
}

TEST_CASE("parse_plugin_task: absolute paths bypass prefix", "[config][task]") {
    auto task = parse_plugin_task(
        "Plugin Abs inputfile /data/input.csv outputfile /data/output.csv",
        "prefix/");

    REQUIRE(task.inputfile  == "/data/input.csv");
    REQUIRE(task.outputfile == "/data/output.csv");
}

TEST_CASE("parse_plugin_task: empty prefix on relative paths", "[config][task]") {
    auto task = parse_plugin_task(
        "Plugin Foo inputfile in.csv outputfile out.csv",
        "");

    REQUIRE(task.inputfile  == "in.csv");
    REQUIRE(task.outputfile == "out.csv");
}

// ---------------------------------------------------------------------------
// parse_config: structural tests
// ---------------------------------------------------------------------------

TEST_CASE("parse_config: purely sequential config has no parallel blocks", "[config][parse]") {
    std::istringstream input(
        "Prefix pipelines/test\n"
        "Plugin A inputfile a.txt outputfile b.txt\n"
        "Plugin B inputfile b.txt outputfile c.txt\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());
    for (auto& step : result.steps) {
        REQUIRE(step.kind == ConfigStepKind::Sequential);
    }
}

TEST_CASE("parse_config: empty parallel block is a valid no-op", "[config][parse]") {
    std::istringstream input(
        "Parallel\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());

    bool found_parallel = false;
    for (auto& step : result.steps) {
        if (step.kind == ConfigStepKind::Parallel) {
            found_parallel = true;
            REQUIRE(step.parallel.tasks.empty());
        }
    }
    REQUIRE(found_parallel);
}

TEST_CASE("parse_config: single plugin in parallel block", "[config][parse]") {
    std::istringstream input(
        "Parallel workers=2\n"
        "  Plugin A inputfile in.csv outputfile out.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());

    bool found = false;
    for (auto& step : result.steps) {
        if (step.kind == ConfigStepKind::Parallel) {
            found = true;
            REQUIRE(step.parallel.tasks.size() == 1);
            REQUIRE(step.parallel.tasks[0].name == "A");
            REQUIRE(step.parallel.options.workers == 2);
        }
    }
    REQUIRE(found);
}

TEST_CASE("parse_config: multiple plugins in parallel block", "[config][parse]") {
    std::istringstream input(
        "Parallel workers=4 memory=32G\n"
        "  Plugin A inputfile a.csv outputfile a_out.csv memory=2G\n"
        "  Plugin B inputfile b.csv outputfile b_out.csv memory=8G\n"
        "  Plugin C inputfile c.csv outputfile c_out.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());

    bool found = false;
    for (auto& step : result.steps) {
        if (step.kind == ConfigStepKind::Parallel) {
            found = true;
            REQUIRE(step.parallel.tasks.size() == 3);
            REQUIRE(step.parallel.options.workers == 4);
            REQUIRE(step.parallel.options.memory == 32ULL * 1024 * 1024 * 1024);
            REQUIRE(step.parallel.tasks[0].memory_hint == 2ULL * 1024 * 1024 * 1024);
            REQUIRE(step.parallel.tasks[1].memory_hint == 8ULL * 1024 * 1024 * 1024);
            REQUIRE(step.parallel.tasks[2].memory_hint == 0);
        }
    }
    REQUIRE(found);
}

TEST_CASE("parse_config: sequential plugins before and after parallel block", "[config][parse]") {
    std::istringstream input(
        "Plugin Pre inputfile a.csv outputfile b.csv\n"
        "Parallel\n"
        "  Plugin X inputfile b.csv outputfile x.csv\n"
        "  Plugin Y inputfile b.csv outputfile y.csv\n"
        "EndParallel\n"
        "Plugin Post inputfile x.csv outputfile z.csv\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());
    REQUIRE(result.steps.size() == 3);
    REQUIRE(result.steps[0].kind == ConfigStepKind::Sequential);
    REQUIRE(result.steps[1].kind == ConfigStepKind::Parallel);
    REQUIRE(result.steps[2].kind == ConfigStepKind::Sequential);
}

TEST_CASE("parse_config: multiple parallel blocks", "[config][parse]") {
    std::istringstream input(
        "Parallel workers=3\n"
        "  Plugin A inputfile a.csv outputfile a2.csv\n"
        "  Plugin B inputfile b.csv outputfile b2.csv\n"
        "EndParallel\n"
        "Plugin Mid inputfile a2.csv outputfile mid.csv\n"
        "Parallel workers=2\n"
        "  Plugin C inputfile mid.csv outputfile c2.csv\n"
        "  Plugin D inputfile mid.csv outputfile d2.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());

    int parallel_count = 0;
    for (auto& step : result.steps) {
        if (step.kind == ConfigStepKind::Parallel) parallel_count++;
    }
    REQUIRE(parallel_count == 2);
}

TEST_CASE("parse_config: comments inside parallel block are ignored", "[config][parse]") {
    std::istringstream input(
        "Parallel\n"
        "  # This is a comment\n"
        "  Plugin A inputfile a.csv outputfile b.csv\n"
        "  # Another comment\n"
        "  Plugin B inputfile c.csv outputfile d.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());

    for (auto& step : result.steps) {
        if (step.kind == ConfigStepKind::Parallel) {
            REQUIRE(step.parallel.tasks.size() == 2);
        }
    }
}

TEST_CASE("parse_config: blank lines inside parallel block are ignored", "[config][parse]") {
    std::istringstream input(
        "Parallel\n"
        "\n"
        "  Plugin A inputfile a.csv outputfile b.csv\n"
        "\n"
        "  Plugin B inputfile c.csv outputfile d.csv\n"
        "\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());

    for (auto& step : result.steps) {
        if (step.kind == ConfigStepKind::Parallel) {
            REQUIRE(step.parallel.tasks.size() == 2);
        }
    }
}

TEST_CASE("parse_config: Prefix before parallel block sets plugin paths", "[config][parse]") {
    std::istringstream input(
        "Prefix pipelines/PD\n"
        "Parallel\n"
        "  Plugin A inputfile data/in.csv outputfile data/out.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE(result.errors.empty());

    for (auto& step : result.steps) {
        if (step.kind == ConfigStepKind::Parallel) {
            REQUIRE(step.parallel.tasks.size() == 1);
            REQUIRE(step.parallel.tasks[0].inputfile  == "pipelines/PD/data/in.csv");
            REQUIRE(step.parallel.tasks[0].outputfile == "pipelines/PD/data/out.csv");
        }
    }
}

// ---------------------------------------------------------------------------
// parse_config: error cases
// ---------------------------------------------------------------------------

TEST_CASE("parse_config: nested Parallel is a parse error", "[config][error]") {
    std::istringstream input(
        "Parallel\n"
        "  Plugin A inputfile a.csv outputfile b.csv\n"
        "  Parallel workers=2\n"
        "    Plugin B inputfile c.csv outputfile d.csv\n"
        "  EndParallel\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE_THAT(result.errors[0].message, ContainsSubstring("nest"));
}

TEST_CASE("parse_config: Prefix inside Parallel is a parse error", "[config][error]") {
    std::istringstream input(
        "Parallel\n"
        "  Prefix some/path\n"
        "  Plugin A inputfile a.csv outputfile b.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE_THAT(result.errors[0].message, ContainsSubstring("Prefix"));
}

TEST_CASE("parse_config: Kitty inside Parallel is a parse error", "[config][error]") {
    std::istringstream input(
        "Parallel\n"
        "  Kitty subdir\n"
        "  Plugin A inputfile a.csv outputfile b.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE_THAT(result.errors[0].message, ContainsSubstring("Kitty"));
}

TEST_CASE("parse_config: missing EndParallel is a parse error", "[config][error]") {
    std::istringstream input(
        "Parallel\n"
        "  Plugin A inputfile a.csv outputfile b.csv\n"
    );
    auto result = parse_config(input);
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE_THAT(result.errors[0].message, ContainsSubstring("EndParallel"));
}

TEST_CASE("parse_config: EndParallel without Parallel is a parse error", "[config][error]") {
    std::istringstream input(
        "Plugin A inputfile a.csv outputfile b.csv\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE_THAT(result.errors[0].message, ContainsSubstring("EndParallel"));
}

TEST_CASE("parse_config: Pipeline inside Parallel is a parse error", "[config][error]") {
    std::istringstream input(
        "Parallel\n"
        "  Pipeline subpipeline.txt\n"
        "EndParallel\n"
    );
    auto result = parse_config(input);
    REQUIRE_FALSE(result.errors.empty());
    REQUIRE_THAT(result.errors[0].message, ContainsSubstring("Pipeline"));
}

// ---------------------------------------------------------------------------
// validate_parallel_block: dependency / conflict detection
// ---------------------------------------------------------------------------

TEST_CASE("validate: two plugins with same output path emits warning", "[validation]") {
    ParallelBlock block;
    block.tasks.push_back({"A", "in1.csv", "shared_out.csv", 0, 0});
    block.tasks.push_back({"B", "in2.csv", "shared_out.csv", 0, 0});

    auto warnings = validate_parallel_block(block);
    REQUIRE_FALSE(warnings.empty());
    bool found = false;
    for (auto& w : warnings) {
        if (w.find("shared_out.csv") != std::string::npos) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("validate: plugin B input matches plugin A output emits warning", "[validation]") {
    ParallelBlock block;
    block.tasks.push_back({"A", "in.csv", "intermediate.csv", 0, 0});
    block.tasks.push_back({"B", "intermediate.csv", "out.csv", 0, 0});

    auto warnings = validate_parallel_block(block);
    REQUIRE_FALSE(warnings.empty());
    bool found = false;
    for (auto& w : warnings) {
        if (w.find("intermediate.csv") != std::string::npos) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("validate: independent plugins produce no warnings", "[validation]") {
    ParallelBlock block;
    block.tasks.push_back({"A", "in_a.csv", "out_a.csv", 0, 0});
    block.tasks.push_back({"B", "in_b.csv", "out_b.csv", 0, 0});

    auto warnings = validate_parallel_block(block);
    REQUIRE(warnings.empty());
}

TEST_CASE("validate: single plugin block has no warnings", "[validation]") {
    ParallelBlock block;
    block.tasks.push_back({"A", "in.csv", "out.csv", 0, 0});

    auto warnings = validate_parallel_block(block);
    REQUIRE(warnings.empty());
}

TEST_CASE("validate: empty block has no warnings", "[validation]") {
    ParallelBlock block;
    auto warnings = validate_parallel_block(block);
    REQUIRE(warnings.empty());
}
