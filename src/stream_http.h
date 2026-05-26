#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <condition_variable>
#include <mutex>
#include <thread>

class Catalog;
class SessionAuth;

class HttpStreamServer {
public:
    using MetricsFn = std::function<std::string()>;

    HttpStreamServer(std::string bindAddress, int port, MetricsFn metricsFn, const Catalog* catalog, const SessionAuth* auth);
    ~HttpStreamServer();

    bool start();
    void stop();

private:
    void run();
    void setStartupResult(bool started);

    std::string bindAddress_;
    int port_;
    MetricsFn metricsFn_;
    std::atomic<bool> running_{false};
    std::atomic<bool> startup_done_{false};
    bool startup_ok_ = false;
    std::mutex startup_mutex_;
    std::condition_variable startup_cv_;
    int serverFd_ = -1;
    std::thread worker_;
    const Catalog* catalog_ = nullptr;
    const SessionAuth* auth_ = nullptr;
};
