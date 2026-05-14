#ifndef PARALLEL_TYPES_H
#define PARALLEL_TYPES_H

#include <cstddef>
#include <string>
#include <vector>

namespace parallel {

struct PluginTask {
    std::string name;
    std::string inputfile;
    std::string outputfile;
    size_t memory_hint = 0;  // bytes; 0 = use default allocation
    int gpu_hint = 0;        // GPU slots required; 0 = no GPU
};

enum class FailMode { Fast, Continue };

struct ParallelBlockOptions {
    int workers = 0;         // 0 = use system default (nproc / 2)
    size_t memory = 0;       // 0 = use system default (80% RAM)
    int gpu = 0;             // 0 = use all detected GPUs
    FailMode fail_mode = FailMode::Fast;
};

struct ParallelBlock {
    ParallelBlockOptions options;
    std::vector<PluginTask> tasks;
    int source_line = -1;    // line number in config file for error reporting
};

struct PluginResult {
    std::string name;
    int exit_code = 0;
    double elapsed_seconds = 0.0;
};

struct SchedulerResult {
    std::vector<PluginResult> completed;
    std::vector<PluginResult> failed;
    double total_elapsed_seconds = 0.0;
};

enum class ConfigStepKind { Sequential, Parallel };

struct SequentialStep {
    std::string keyword;     // "Plugin", "Prefix", "Kitty", "Pipeline"
    std::string raw_line;
};

struct ConfigStep {
    ConfigStepKind kind;
    SequentialStep sequential;  // valid when kind == Sequential
    ParallelBlock parallel;     // valid when kind == Parallel
};

struct ParseError {
    int line;
    std::string message;
};

struct ParseResult {
    std::vector<ConfigStep> steps;
    std::vector<ParseError> errors;
    std::vector<std::string> warnings;
};

} // namespace parallel

#endif
