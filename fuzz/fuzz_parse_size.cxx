#include "ConfigParser.h"
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string input(reinterpret_cast<const char*>(data), size);
    try {
        parallel::parse_size(input);
    } catch (const std::exception&) {
    }
    return 0;
}
