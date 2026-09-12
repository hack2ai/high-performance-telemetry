#pragma once

#include <string_view>

namespace telemetry {

enum class LogLevel { info, warning, error };

void log(LogLevel level, std::string_view event, std::string_view message);

} // namespace telemetry
