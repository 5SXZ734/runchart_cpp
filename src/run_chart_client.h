#pragma once

#include <memory>
#include <string>
#include <vector>
#include <grpcpp/grpcpp.h>
#include "measurement.h"
#include "runchart.grpc.pb.h"

class RunChartClient {
public:
    explicit RunChartClient(std::shared_ptr<grpc::Channel> channel);

    Measurement getSnapShot();
    void sendMeasurements(const std::string& jsonPath);
    void monitor();
    void sendAndCheck(const std::string& jsonPath);

private:
    static std::vector<Measurement> importMeasurements(const std::string& jsonPath);

    std::unique_ptr<runchart::RunChartService::Stub> stub_;
};
