#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <vector>

#include "measurement.h"

class Catalog {
public:
    void scanFromNasPath(const std::string& nasPath);
    void addMeasurement(const Measurement& measurement);
    Measurement latestOrDefault() const;
    std::vector<Measurement> since(std::size_t index) const;
    std::size_t size() const;
    void waitForUpdate(std::size_t nextIndex, std::atomic<bool>* stopFlag) const;

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::vector<Measurement> measurements_;
};
