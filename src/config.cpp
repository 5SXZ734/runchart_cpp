#include "config.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::unordered_map<std::string, std::string> loadEnvFile(const std::string& path) {
    std::unordered_map<std::string, std::string> values;
    std::ifstream in(path);
    if (!in.is_open()) {
        return values;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        values[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }
    return values;
}

std::string envOrFile(const char* key, const std::unordered_map<std::string, std::string>& fileValues, const std::string& fallback = "") {
    if (const char* env = std::getenv(key)) {
        return env;
    }
    auto it = fileValues.find(key);
    if (it != fileValues.end()) {
        return it->second;
    }
    return fallback;
}

}  // namespace

ServerConfig ServerConfig::load() {
    ServerConfig cfg;
    const char* configPath = std::getenv("RUNCHART_CONFIG_FILE");
    const auto values = loadEnvFile(configPath ? configPath : ".env");

    cfg.grpc_address = envOrFile("RUNCHART_GRPC_ADDRESS", values, cfg.grpc_address);
    cfg.http_bind_address = envOrFile("RUNCHART_HTTP_BIND_ADDRESS", values, cfg.http_bind_address);
    cfg.nas_scan_path = envOrFile("RUNCHART_NAS_SCAN_PATH", values, cfg.nas_scan_path);
    cfg.auth_secret = envOrFile("RUNCHART_AUTH_SECRET", values, cfg.auth_secret);
    cfg.structured_logging = envOrFile("RUNCHART_ENABLE_STRUCTURED_LOGGING", values, "0") == "1";

    const std::string httpPort = envOrFile("RUNCHART_HTTP_PORT", values, std::to_string(cfg.http_port));
    try { cfg.http_port = std::stoi(httpPort); } catch (...) {}

    return cfg;
}
