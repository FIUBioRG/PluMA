#include "ConfigParser.h"
#include <cstdint>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string input(reinterpret_cast<const char*>(data), size);
    std::istringstream stream(input);

    auto result = parallel::parse_config(stream);

    for (auto& step : result.steps) {
        if (step.kind == parallel::ConfigStepKind::Parallel) {
            parallel::validate_parallel_block(step.parallel);
        }
    }

    return 0;
}
