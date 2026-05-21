#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "ParallelTypes.h"

#include <istream>
#include <string>

namespace parallel {

size_t parse_size(const std::string& s);

ParallelBlockOptions parse_parallel_options(const std::string& line);

PluginTask parse_plugin_task(const std::string& line, const std::string& prefix);

ParseResult parse_config(std::istream& input);

std::vector<std::string> validate_parallel_block(const ParallelBlock& block);

} // namespace parallel

#endif
