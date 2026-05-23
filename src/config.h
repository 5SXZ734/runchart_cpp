#pragma once

#include <string>

struct ServerConfig {
    std::string grpc_address = "0.0.0.0:3030";
    std::string http_bind_address = "0.0.0.0";
    int http_port = 8080;
    std::string nas_scan_path = "./";
    std::string auth_secret = "";
    bool structured_logging = false;

    static ServerConfig load();
};
