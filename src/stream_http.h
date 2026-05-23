#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

class HttpStreamServer {
public:
    using MetricsFn = std::function<std::string()>;

    HttpStreamServer(std::string bindAddress, int port, MetricsFn metricsFn);
    ~HttpStreamServer();

    bool start();
    void stop();

private:
    void run();

    std::string bindAddress_;
    int port_;
    MetricsFn metricsFn_;
    std::atomic<bool> running_{false};
    int serverFd_ = -1;
    std::thread worker_;
};
