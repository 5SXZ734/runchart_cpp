#include "stream_http.h"

#include "catalog.h"
#include "session_auth.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

void closeSocket(SocketHandle s) {
#ifdef _WIN32
    if (s != INVALID_SOCKET) closesocket(s);
#else
    if (s >= 0) close(s);
#endif
}

std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool sendAll(SocketHandle client, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        const int n = send(client, data + sent, static_cast<int>(size - sent), 0);
#else
        const ssize_t n = send(client, data + sent, size - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}
}  // namespace

HttpStreamServer::HttpStreamServer(std::string bindAddress, int port, MetricsFn metricsFn, const Catalog* catalog, const SessionAuth* auth)
    : bindAddress_(std::move(bindAddress)), port_(port), metricsFn_(std::move(metricsFn)), catalog_(catalog), auth_(auth) {}

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

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || listen(serverSocket, 8) < 0) {
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

        char buffer[4096] = {0};
#ifdef _WIN32
        const int n = recv(client, buffer, static_cast<int>(sizeof(buffer) - 1), 0);
#else
        const ssize_t n = read(client, buffer, sizeof(buffer) - 1);
#endif
        const std::string req = n > 0 ? std::string(buffer, static_cast<std::size_t>(n)) : "";
        std::string body;
        std::string status = "200 OK";

        const std::size_t firstLineEnd = req.find("\r\n");
        const std::string firstLine = firstLineEnd == std::string::npos ? req : req.substr(0, firstLineEnd);
        const bool isGet = firstLine.rfind("GET ", 0) == 0;
        const std::size_t pathStart = isGet ? 4 : std::string::npos;
        const std::size_t pathEnd = isGet ? firstLine.find(' ', pathStart) : std::string::npos;
        const std::string path = (isGet && pathEnd != std::string::npos) ? firstLine.substr(pathStart, pathEnd - pathStart) : "";

        if (path == "/health") {
            body = "ok";
        } else if (path == "/metrics") {
            body = metricsFn_ ? metricsFn_() : "";
        } else if (path.rfind("/stream/", 0) == 0 && catalog_ != nullptr && auth_ != nullptr) {
            std::string token;
            const std::size_t authPos = req.find("x-auth-token:");
            if (authPos != std::string::npos) {
                const std::size_t tokenEnd = req.find("\r\n", authPos);
                token = trim(req.substr(authPos + 13, tokenEnd - authPos - 13));
            }
            if (!auth_->isAuthorizedToken(token)) {
                status = "401 Unauthorized";
                body = "unauthorized\n";
            } else {
                TrackRecord track{};
                bool badId = false;
                std::int64_t trackId = 0;
                try { trackId = std::stoll(path.substr(8)); } catch (...) { badId = true; }
                if (badId || !catalog_->findTrackById(trackId, &track)) {
                    status = "404 Not Found";
                    body = "track not found\n";
                } else {
                    std::ifstream file(track.filePath, std::ios::binary | std::ios::ate);
                    if (!file.is_open()) {
                        status = "404 Not Found";
                        body = "audio file missing\n";
                    } else {
                        const std::size_t fileSize = static_cast<std::size_t>(file.tellg());
                        std::size_t start = 0;
                        std::size_t end = fileSize > 0 ? fileSize - 1 : 0;
                        bool partial = false;
                        const std::size_t rangePos = req.find("Range: bytes=");
                        if (rangePos != std::string::npos) {
                            const std::size_t lineEnd = req.find("\r\n", rangePos);
                            const std::string range = trim(req.substr(rangePos + 13, lineEnd - rangePos - 13));
                            const std::size_t dash = range.find('-');
                            if (dash != std::string::npos) {
                                const std::string s0 = trim(range.substr(0, dash));
                                const std::string s1 = trim(range.substr(dash + 1));
                                if (!s0.empty()) start = static_cast<std::size_t>(std::stoull(s0));
                                if (!s1.empty()) end = static_cast<std::size_t>(std::stoull(s1));
                                partial = true;
                            }
                        }
                        if (fileSize == 0 || start >= fileSize || end < start) {
                            const std::string err = "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */" + std::to_string(fileSize) + "\r\nConnection: close\r\n\r\n";
                            sendAll(client, err.c_str(), err.size());
                            closeSocket(client);
                            continue;
                        }
                        end = std::min(end, fileSize - 1);
                        const std::size_t bytesToSend = end - start + 1;

                        std::ostringstream headers;
                        headers << "HTTP/1.1 " << (partial ? "206 Partial Content" : "200 OK") << "\r\n"
                                << "Content-Type: audio/mpeg\r\n"
                                << "Accept-Ranges: bytes\r\n";
                        if (partial) headers << "Content-Range: bytes " << start << "-" << end << "/" << fileSize << "\r\n";
                        headers << "Content-Length: " << bytesToSend << "\r\nConnection: close\r\n\r\n";

                        const std::string h = headers.str();
                        if (!sendAll(client, h.c_str(), h.size())) {
                            closeSocket(client);
                            continue;
                        }

                        file.seekg(static_cast<std::streamoff>(start), std::ios::beg);
                        constexpr std::size_t kChunk = 64 * 1024;
                        std::vector<char> chunk(kChunk);
                        std::size_t remaining = bytesToSend;
                        while (remaining > 0) {
                            const std::size_t next = std::min(remaining, chunk.size());
                            file.read(chunk.data(), static_cast<std::streamsize>(next));
                            const std::size_t got = static_cast<std::size_t>(file.gcount());
                            if (got == 0) break;
                            if (!sendAll(client, chunk.data(), got)) break;
                            remaining -= got;
                        }
                        closeSocket(client);
                        continue;
                    }
                }
            }
        } else {
            status = "404 Not Found";
            body = "not found\n";
        }

        const std::string resp = "HTTP/1.1 " + status + "\r\nContent-Type: text/plain\r\nContent-Length: " +
                                 std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
        sendAll(client, resp.c_str(), resp.size());
        closeSocket(client);
    }

    closeSocket(serverSocket);
    serverFd_ = -1;
#ifdef _WIN32
    WSACleanup();
#endif
}
