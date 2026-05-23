#include "structured_logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

StructuredLogger& StructuredLogger::instance() {
    static StructuredLogger logger;
    return logger;
}

void StructuredLogger::setLogPath(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    logPath_ = path;
}

void StructuredLogger::log(const std::string& level,
                           const std::string& event,
                           const std::string& clientIp,
                           const std::string& requestId,
                           std::initializer_list<std::pair<std::string, std::string>> fields) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream out(logPath_, std::ios::app);
    if (!out.is_open()) {
        return;
    }

    out << "{"
        << "\"timestamp\":\"" << escapeJson(timestampUtc()) << "\"," 
        << "\"level\":\"" << escapeJson(level) << "\"," 
        << "\"event\":\"" << escapeJson(event) << "\"," 
        << "\"client_ip\":\"" << escapeJson(clientIp) << "\"," 
        << "\"request_id\":\"" << escapeJson(requestId) << "\"";

    for (const auto& [key, value] : fields) {
        out << ",\"" << escapeJson(key) << "\":\"" << escapeJson(value) << "\"";
    }
    out << "}\n";
}

std::string StructuredLogger::timestampUtc() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string StructuredLogger::escapeJson(const std::string& value) {
    std::ostringstream oss;
    for (char c : value) {
        switch (c) {
            case '"': oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default: oss << c; break;
        }
    }
    return oss.str();
}
