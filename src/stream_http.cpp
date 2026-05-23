#include "stream_http.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

HttpStreamServer::HttpStreamServer(std::string bindAddress, int port, MetricsFn metricsFn)
    : bindAddress_(std::move(bindAddress)), port_(port), metricsFn_(std::move(metricsFn)) {}

HttpStreamServer::~HttpStreamServer() { stop(); }

bool HttpStreamServer::start() {
    if (running_.exchange(true)) {
        return true;
    }
    worker_ = std::thread(&HttpStreamServer::run, this);
    return true;
}

void HttpStreamServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (serverFd_ >= 0) {
        shutdown(serverFd_, SHUT_RDWR);
        close(serverFd_);
        serverFd_ = -1;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void HttpStreamServer::run() {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        return;
    }
    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (bindAddress_ == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, bindAddress_.c_str(), &addr.sin_addr);
    }

    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        return;
    }
    if (listen(serverFd_, 8) < 0) {
        return;
    }

    while (running_.load()) {
        int client = accept(serverFd_, nullptr, nullptr);
        if (client < 0) {
            continue;
        }

        char buffer[2048] = {0};
        const ssize_t n = read(client, buffer, sizeof(buffer) - 1);
        std::string req = n > 0 ? std::string(buffer, static_cast<std::size_t>(n)) : "";
        std::string body;
        std::string status = "200 OK";

        if (req.find("GET /health") != std::string::npos) {
            body = "ok\n";
        } else if (req.find("GET /metrics") != std::string::npos) {
            body = metricsFn_ ? metricsFn_() : "";
        } else {
            status = "404 Not Found";
            body = "not found\n";
        }

        const std::string resp = "HTTP/1.1 " + status + "\r\nContent-Type: text/plain\r\nContent-Length: " +
                                 std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        send(client, resp.c_str(), resp.size(), 0);
        close(client);
    }
}
