#include "ConfigParser.h"

#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace parallel {

static std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    return tokens;
}

size_t parse_size(const std::string& s) {
    if (s.empty()) throw std::invalid_argument("empty size string");

    char suffix = s.back();
    if (std::isdigit(static_cast<unsigned char>(suffix))) {
        size_t pos;
        long long val;
        try { val = std::stoll(s, &pos); }
        catch (...) { throw std::invalid_argument("invalid size: " + s); }
        if (pos != s.size()) throw std::invalid_argument("invalid size: " + s);
        if (val < 0) throw std::invalid_argument("negative size: " + s);
        return static_cast<size_t>(val);
    }

    std::string num_part = s.substr(0, s.size() - 1);
    if (num_part.empty()) throw std::invalid_argument("no numeric part: " + s);

    size_t pos;
    long long val;
    try { val = std::stoll(num_part, &pos); }
    catch (...) { throw std::invalid_argument("invalid size: " + s); }
    if (pos != num_part.size()) throw std::invalid_argument("invalid size: " + s);
    if (val < 0) throw std::invalid_argument("negative size: " + s);

    size_t multiplier;
    switch (std::toupper(static_cast<unsigned char>(suffix))) {
        case 'K': multiplier = 1024ULL; break;
        case 'M': multiplier = 1024ULL * 1024; break;
        case 'G': multiplier = 1024ULL * 1024 * 1024; break;
        case 'T': multiplier = 1024ULL * 1024 * 1024 * 1024; break;
        default:  throw std::invalid_argument("unknown size suffix: " + s);
    }
    return static_cast<size_t>(val) * multiplier;
}

ParallelBlockOptions parse_parallel_options(const std::string& line) {
    ParallelBlockOptions opts;
    auto tokens = tokenize(line);

    for (size_t i = 1; i < tokens.size(); i++) {
        auto eq = tokens[i].find('=');
        if (eq == std::string::npos) continue;

        std::string key = tokens[i].substr(0, eq);
        std::string val = tokens[i].substr(eq + 1);

        try {
            if (key == "workers")     opts.workers = std::stoi(val);
            else if (key == "memory") opts.memory = parse_size(val);
            else if (key == "gpu")    opts.gpu = std::stoi(val);
            else if (key == "fail")   opts.fail_mode = (val == "continue") ? FailMode::Continue : FailMode::Fast;
        } catch (const std::exception&) {
            // malformed value — skip this option, keep defaults
        }
    }
    return opts;
}

PluginTask parse_plugin_task(const std::string& line, const std::string& prefix) {
    PluginTask task;
    auto tokens = tokenize(line);
    if (tokens.size() < 6) return task;

    task.name = tokens[1];

    std::string input_raw, output_raw;
    for (size_t i = 2; i < tokens.size(); i++) {
        if (tokens[i] == "inputfile" && i + 1 < tokens.size()) {
            input_raw = tokens[i + 1];
        } else if (tokens[i] == "outputfile" && i + 1 < tokens.size()) {
            output_raw = tokens[i + 1];
        }
    }

    task.inputfile  = (!input_raw.empty() && input_raw[0] == '/') ? input_raw : prefix + input_raw;
    task.outputfile = (!output_raw.empty() && output_raw[0] == '/') ? output_raw : prefix + output_raw;

    for (size_t i = 2; i < tokens.size(); i++) {
        auto eq = tokens[i].find('=');
        if (eq == std::string::npos) continue;
        std::string key = tokens[i].substr(0, eq);
        std::string val = tokens[i].substr(eq + 1);

        try {
            if (key == "memory")    task.memory_hint = parse_size(val);
            else if (key == "gpu")  task.gpu_hint = std::stoi(val);
        } catch (const std::exception&) {
        }
    }
    return task;
}

ParseResult parse_config(std::istream& input) {
    ParseResult result;
    std::string line;
    int line_num = 0;
    std::string prefix;
    bool in_parallel = false;
    ParallelBlock current_block;
    int parallel_start_line = 0;

    while (std::getline(input, line)) {
        line_num++;
        auto trimmed = trim(line);
        if (trimmed.empty()) continue;
        if (trimmed[0] == '#') continue;

        auto tokens = tokenize(trimmed);
        if (tokens.empty()) continue;
        auto keyword = tokens[0];

        if (keyword == "Parallel") {
            if (in_parallel) {
                result.errors.push_back({line_num, "nested Parallel blocks are not allowed"});
                continue;
            }
            in_parallel = true;
            parallel_start_line = line_num;
            current_block = {};
            current_block.source_line = line_num;
            current_block.options = parse_parallel_options(trimmed);
            continue;
        }

        if (keyword == "EndParallel") {
            if (!in_parallel) {
                result.errors.push_back({line_num, "EndParallel without matching Parallel"});
                continue;
            }
            in_parallel = false;
            ConfigStep step;
            step.kind = ConfigStepKind::Parallel;
            step.parallel = std::move(current_block);
            result.steps.push_back(std::move(step));
            continue;
        }

        if (in_parallel) {
            if (keyword == "Prefix") {
                result.errors.push_back({line_num, "Prefix directive not allowed inside Parallel block"});
                continue;
            }
            if (keyword == "Kitty") {
                result.errors.push_back({line_num, "Kitty directive not allowed inside Parallel block"});
                continue;
            }
            if (keyword == "Pipeline") {
                result.errors.push_back({line_num, "Pipeline directive not allowed inside Parallel block"});
                continue;
            }
            if (keyword == "Plugin") {
                current_block.tasks.push_back(parse_plugin_task(trimmed, prefix));
            }
            continue;
        }

        if (keyword == "Prefix" && tokens.size() > 1) {
            prefix = tokens[1] + "/";
        }

        ConfigStep step;
        step.kind = ConfigStepKind::Sequential;
        step.sequential.keyword = keyword;
        step.sequential.raw_line = trimmed;
        result.steps.push_back(std::move(step));
    }

    if (in_parallel) {
        result.errors.push_back({parallel_start_line, "missing EndParallel for Parallel block"});
    }

    return result;
}

std::vector<std::string> validate_parallel_block(const ParallelBlock& block) {
    std::vector<std::string> warnings;

    for (size_t i = 0; i < block.tasks.size(); i++) {
        for (size_t j = i + 1; j < block.tasks.size(); j++) {
            if (block.tasks[i].outputfile == block.tasks[j].outputfile) {
                warnings.push_back(
                    "plugins '" + block.tasks[i].name + "' and '" + block.tasks[j].name +
                    "' share output path '" + block.tasks[i].outputfile + "'");
            }
        }
    }

    for (size_t i = 0; i < block.tasks.size(); i++) {
        for (size_t j = 0; j < block.tasks.size(); j++) {
            if (i == j) continue;
            if (block.tasks[j].inputfile == block.tasks[i].outputfile) {
                warnings.push_back(
                    "plugin '" + block.tasks[j].name + "' reads '" + block.tasks[j].inputfile +
                    "' which is written by '" + block.tasks[i].name + "' in the same parallel block");
            }
        }
    }

    return warnings;
}

} // namespace parallel
