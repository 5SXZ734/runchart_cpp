#include "run_chart_service.h"
#include <iostream>
#include <random>
#include <string>

#include "structured_logger.h"

using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

namespace {

std::string extractClientIp(const grpc::ServerContext* context) {
    const std::string peer = context ? context->peer() : "";
    const auto colon = peer.rfind(':');
    if (colon == std::string::npos) {
        return peer;
    }
    return peer.substr(colon + 1);
}

std::string buildRequestId(const grpc::ServerContext* context) {
    if (context != nullptr) {
        const auto& metadata = context->client_metadata();
        const auto it = metadata.find("x-request-id");
        if (it != metadata.end()) {
            return std::string(it->second.data(), it->second.length());
        }
    }

    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<unsigned long long> dist;
    return std::to_string(dist(rng));
}

}  // namespace


Status RunChartService::SnapShot(ServerContext* context, const runchart::Empty*, runchart::DataPoint* response) {
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    std::lock_guard<std::mutex> lock(mutex_);
    *response = measurements_.empty() ? Measurement::defaultMeasurement().toDataPoint()
                                    : measurements_.back().toDataPoint();
    StructuredLogger::instance().log("INFO", "snapshot", clientIp, requestId);
    return Status::OK;
}

Status RunChartService::SendMeasurements(ServerContext* context, ServerReader<runchart::DataPoint>* reader, runchart::Empty*) {
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    runchart::DataPoint point;
    while (reader->Read(&point)) {
        addMeasurement(point, clientIp, requestId);
    }
    return Status::OK;
}

Status RunChartService::Monitor(ServerContext* context, const runchart::Empty*, ServerWriter<runchart::DataPoint>* writer) {
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    StructuredLogger::instance().log("INFO", "monitor_start", clientIp, requestId);
    std::size_t nextIndex = 0;
    std::unique_lock<std::mutex> lock(mutex_);

    while (!context->IsCancelled() && !stopping_) {
        cv_.wait(lock, [&] { return stopping_ || nextIndex < measurements_.size() || context->IsCancelled(); });
        while (nextIndex < measurements_.size()) {
            auto point = measurements_[nextIndex++].toDataPoint();
            lock.unlock();
            if (!writer->Write(point)) {
                return Status::OK;
            }
            lock.lock();
        }
    }
    StructuredLogger::instance().log("INFO", "monitor_end", clientIp, requestId);
    return Status::OK;
}

Status RunChartService::SendAndCheck(ServerContext* context, ServerReaderWriter<runchart::Warning, runchart::DataPoint>* stream) {
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    runchart::DataPoint point;
    while (stream->Read(&point)) {
        addMeasurement(point, clientIp, requestId);
        Measurement m(point);
    StructuredLogger::instance().log("INFO", "measurement_received", clientIp, requestId, {{"part_number", m.partNumber}, {"measurement", std::to_string(m.measurement)}});
        runchart::Warning warning;
        if (buildWarning(m, &warning)) {
            StructuredLogger::instance().log("WARNING", "spec_warning", clientIp, requestId, {{"part_number", m.partNumber}, {"warning", warning.warning()}});
            stream->Write(warning);
        }
    }
    return Status::OK;
}

void RunChartService::addMeasurement(const runchart::DataPoint& point, const std::string& clientIp, const std::string& requestId) {
    Measurement m(point);
    StructuredLogger::instance().log("INFO", "measurement_received", clientIp, requestId, {{"part_number", m.partNumber}, {"measurement", std::to_string(m.measurement)}});
    std::cout << "Measurement received by server:\n" << m.toString() << std::endl;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        measurements_.push_back(m);
        ++version_;
    }
    cv_.notify_all();
}

bool RunChartService::buildWarning(const Measurement& m, runchart::Warning* warning) {
    std::string text;
    if (m.measurement > m.nominal + m.tolerance) {
        text = "Measurement is above spec";
    } else if (m.measurement < m.nominal - m.tolerance) {
        text = "Measurement is below spec";
    } else {
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    warning->mutable_timestamp()->set_seconds(seconds);
    warning->set_part_number(m.partNumber);
    warning->set_spec_nominal(m.nominal);
    warning->set_spec_tolerance(m.tolerance);
    warning->set_warning(text);
    return true;
}

void RunChartService::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();
}
