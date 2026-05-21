#include "ConfigParser.h"
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 2) return 0;

    uint8_t split = data[0] % size;
    std::string line(reinterpret_cast<const char*>(data + 1), std::min<size_t>(split, size - 1));
    std::string prefix(reinterpret_cast<const char*>(data + 1 + std::min<size_t>(split, size - 1)),
                       size - 1 - std::min<size_t>(split, size - 1));

    parallel::parse_plugin_task(line, prefix);
    return 0;
}
