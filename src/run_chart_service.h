#pragma once

#include <condition_variable>
#include <mutex>
#include <vector>
#include <grpcpp/grpcpp.h>
#include <string>
#include "measurement.h"
#include "runchart.grpc.pb.h"

class RunChartService final : public runchart::RunChartService::Service {
public:
    grpc::Status SnapShot(grpc::ServerContext* context,
                          const runchart::Empty* request,
                          runchart::DataPoint* response) override;

    grpc::Status SendMeasurements(grpc::ServerContext* context,
                                  grpc::ServerReader<runchart::DataPoint>* reader,
                                  runchart::Empty* response) override;

    grpc::Status Monitor(grpc::ServerContext* context,
                         const runchart::Empty* request,
                         grpc::ServerWriter<runchart::DataPoint>* writer) override;

    grpc::Status SendAndCheck(grpc::ServerContext* context,
                              grpc::ServerReaderWriter<runchart::Warning, runchart::DataPoint>* stream) override;

    void stop();

private:
    void addMeasurement(const runchart::DataPoint& point, const std::string& clientIp, const std::string& requestId);
    static bool buildWarning(const Measurement& m, runchart::Warning* warning);

    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<Measurement> measurements_;
    std::size_t version_ = 0;
    bool stopping_ = false;
};
