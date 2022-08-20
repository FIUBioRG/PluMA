#include "utils.hxx"

namespace utils {
    std::vector<std::string> split(std::string str, const std::string delim) {
        std::vector<std::string> tokens;
        size_t pos = 0;

        while ((pos = str.find(delim)) != std::string::npos) {
            tokens.push_back(str.substr(0, pos));
            str.erase(0, pos + delim.length());
        }

        return tokens;
    }
}
