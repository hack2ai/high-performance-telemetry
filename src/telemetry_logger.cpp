#include "telemetry_logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>

namespace telemetry {
namespace {

std::mutex& log_mutex() {
    static std::mutex mutex;
    return mutex;
}

const char* level_name(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::info: return "INFO";
        case LogLevel::warning: return "WARN";
        case LogLevel::error: return "ERROR";
    }
    return "UNKNOWN";
}

} // namespace

void log(LogLevel level, std::string_view event, std::string_view message) {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time{};
#if defined(_WIN32)
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    std::lock_guard<std::mutex> lock(log_mutex());
    std::cerr << '[' << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "] "
              << level_name(level) << " event=" << event << " message=" << message << '\n';
}

} // namespace telemetry
