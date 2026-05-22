#include "run_chart_service.h"
#include <iostream>

using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

Status RunChartService::SnapShot(ServerContext*, const runchart::Empty*, runchart::DataPoint* response) {
    std::lock_guard<std::mutex> lock(mutex_);
    *response = measurements_.empty() ? Measurement::defaultMeasurement().toDataPoint()
                                    : measurements_.back().toDataPoint();
    return Status::OK;
}

Status RunChartService::SendMeasurements(ServerContext*, ServerReader<runchart::DataPoint>* reader, runchart::Empty*) {
    runchart::DataPoint point;
    while (reader->Read(&point)) {
        addMeasurement(point);
    }
    return Status::OK;
}

Status RunChartService::Monitor(ServerContext* context, const runchart::Empty*, ServerWriter<runchart::DataPoint>* writer) {
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
    return Status::OK;
}

Status RunChartService::SendAndCheck(ServerContext*, ServerReaderWriter<runchart::Warning, runchart::DataPoint>* stream) {
    runchart::DataPoint point;
    while (stream->Read(&point)) {
        addMeasurement(point);
        Measurement m(point);
        runchart::Warning warning;
        if (buildWarning(m, &warning)) {
            stream->Write(warning);
        }
    }
    return Status::OK;
}

void RunChartService::addMeasurement(const runchart::DataPoint& point) {
    Measurement m(point);
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
