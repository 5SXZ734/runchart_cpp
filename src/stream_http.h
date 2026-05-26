#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

class Catalog;
class SessionAuth;

namespace httplib {
class Server;
}

class HttpStreamServer {
public:
    using MetricsFn = std::function<std::string()>;

    HttpStreamServer(std::string bindAddress, int port, MetricsFn metricsFn, const Catalog* catalog, const SessionAuth* auth);
    ~HttpStreamServer();

    bool start();
    void stop();

private:
    std::string bindAddress_;
    int port_;
    MetricsFn metricsFn_;
    const Catalog* catalog_ = nullptr;
    const SessionAuth* auth_ = nullptr;
    std::atomic<bool> running_{false};
    std::thread worker_;
    httplib::Server* server_ = nullptr;
};
