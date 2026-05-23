#pragma once

#include <atomic>
#include <grpcpp/grpcpp.h>

#include "catalog.h"
#include "measurement.h"
#include "metrics.h"
#include "runchart.grpc.pb.h"
#include "session_auth.h"

class RunChartService final : public runchart::RunChartService::Service {
public:
    RunChartService(Catalog* catalog, SessionAuth* auth, Metrics* metrics);

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

    Catalog* catalog_;
    SessionAuth* auth_;
    Metrics* metrics_;
    std::atomic<bool> stopping_{false};
};
