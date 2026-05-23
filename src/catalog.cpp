#include "catalog.h"

#include <atomic>
#include <fstream>

void Catalog::scanFromNasPath(const std::string& nasPath) {
    (void)nasPath;
}

void Catalog::addMeasurement(const Measurement& measurement) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        measurements_.push_back(measurement);
    }
    cv_.notify_all();
}

Measurement Catalog::latestOrDefault() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return measurements_.empty() ? Measurement::defaultMeasurement() : measurements_.back();
}

std::vector<Measurement> Catalog::since(std::size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= measurements_.size()) {
        return {};
    }
    return std::vector<Measurement>(measurements_.begin() + static_cast<std::ptrdiff_t>(index), measurements_.end());
}

std::size_t Catalog::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return measurements_.size();
}

void Catalog::waitForUpdate(std::size_t nextIndex, std::atomic<bool>* stopFlag) const {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] {
        return (stopFlag != nullptr && stopFlag->load()) || nextIndex < measurements_.size();
    });
}
