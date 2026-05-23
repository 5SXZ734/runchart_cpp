#pragma once

#include <initializer_list>
#include <mutex>
#include <string>

class StructuredLogger {
public:
    static StructuredLogger& instance();

    void setLogPath(const std::string& path);
    void log(const std::string& level,
             const std::string& event,
             const std::string& clientIp,
             const std::string& requestId,
             std::initializer_list<std::pair<std::string, std::string>> fields = {});

private:
    StructuredLogger() = default;

    static std::string timestampUtc();
    static std::string escapeJson(const std::string& value);

    std::mutex mutex_;
    std::string logPath_ = "runchart_server.log";
};
