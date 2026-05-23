#pragma once

#include <atomic>
#include <string>

struct Metrics {
    std::atomic<std::size_t> snapshot_requests{0};
    std::atomic<std::size_t> send_measurements_requests{0};
    std::atomic<std::size_t> monitor_requests{0};
    std::atomic<std::size_t> send_and_check_requests{0};
    std::atomic<std::size_t> health_requests{0};

    std::string toPrometheusText() const;
};
