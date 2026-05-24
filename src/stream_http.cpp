#include "stream_http.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <chrono>
#include <cstring>

namespace {
#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void closeSocket(SocketHandle s) {
    if (s != INVALID_SOCKET) {
        closesocket(s);
    }
}
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void closeSocket(SocketHandle s) {
    if (s >= 0) {
        close(s);
    }
}
#endif
}

HttpStreamServer::HttpStreamServer(std::string bindAddress, int port, MetricsFn metricsFn)
    : bindAddress_(std::move(bindAddress)), port_(port), metricsFn_(std::move(metricsFn)) {}

HttpStreamServer::~HttpStreamServer() { stop(); }

bool HttpStreamServer::start() {
    if (running_.exchange(true)) {
        return startup_done_.load() && startup_ok_;
    }

    {
        std::lock_guard<std::mutex> lock(startup_mutex_);
        startup_done_.store(false);
        startup_ok_ = false;
    }

    worker_ = std::thread(&HttpStreamServer::run, this);

    std::unique_lock<std::mutex> lock(startup_mutex_);
    startup_cv_.wait(lock, [this]() { return startup_done_.load(); });
    if (!startup_ok_) {
        running_.store(false);
        if (worker_.joinable()) {
            lock.unlock();
            worker_.join();
        }
        return false;
    }
    return true;
}

void HttpStreamServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (serverFd_ >= 0) {
#ifdef _WIN32
        shutdown(static_cast<SOCKET>(serverFd_), SD_BOTH);
#else
        shutdown(serverFd_, SHUT_RDWR);
#endif
        closeSocket(static_cast<SocketHandle>(serverFd_));
        serverFd_ = -1;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void HttpStreamServer::setStartupResult(bool started) {
    {
        std::lock_guard<std::mutex> lock(startup_mutex_);
        startup_ok_ = started;
        startup_done_.store(true);
    }
    startup_cv_.notify_all();
}

void HttpStreamServer::run() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        running_.store(false);
        setStartupResult(false);
        return;
    }
#endif

    SocketHandle serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == kInvalidSocket) {
#ifdef _WIN32
        WSACleanup();
#endif
        running_.store(false);
        setStartupResult(false);
        return;
    }
    serverFd_ = static_cast<int>(serverSocket);

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port_));
    if (bindAddress_ == "0.0.0.0") {
        addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, bindAddress_.c_str(), &addr.sin_addr);
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        listen(serverSocket, 8) < 0) {
        closeSocket(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        running_.store(false);
        serverFd_ = -1;
        setStartupResult(false);
        return;
    }

    setStartupResult(true);

    while (running_.load()) {
        SocketHandle client = accept(serverSocket, nullptr, nullptr);
        if (client == kInvalidSocket) {
            continue;
        }

        char buffer[2048] = {0};
#ifdef _WIN32
        const int n = recv(client, buffer, static_cast<int>(sizeof(buffer) - 1), 0);
#else
        const ssize_t n = read(client, buffer, sizeof(buffer) - 1);
#endif
        std::string req = n > 0 ? std::string(buffer, static_cast<std::size_t>(n)) : "";
        std::string body;
        std::string status = "200 OK";

        if (req.find("GET /health") != std::string::npos) {
            body = "ok";
        } else if (req.find("GET /metrics") != std::string::npos) {
            body = metricsFn_ ? metricsFn_() : "";
        } else {
            status = "404 Not Found";
            body = "not found\n";
        }

        const std::string resp = "HTTP/1.1 " + status + "\r\nContent-Type: text/plain\r\nContent-Length: " +
                                 std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
#ifdef _WIN32
        send(client, resp.c_str(), static_cast<int>(resp.size()), 0);
#else
        send(client, resp.c_str(), resp.size(), 0);
#endif
        closeSocket(client);
    }

    closeSocket(serverSocket);
    serverFd_ = -1;
#ifdef _WIN32
    WSACleanup();
#endif
}
