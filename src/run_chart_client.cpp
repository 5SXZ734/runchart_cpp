#include "run_chart_client.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

RunChartClient::RunChartClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(runchart::RunChartService::NewStub(std::move(channel))) {}

Measurement RunChartClient::getSnapShot() {
    grpc::ClientContext context;
    runchart::Empty request;
    runchart::DataPoint response;
    auto status = stub_->SnapShot(&context, request, &response);
    if (!status.ok()) {
        throw std::runtime_error("SnapShot failed: " + status.error_message());
    }
    return Measurement(response);
}

void RunChartClient::sendMeasurements(const std::string& jsonPath) {
    auto measurements = importMeasurements(jsonPath);
    grpc::ClientContext context;
    runchart::Empty response;
    auto writer = stub_->SendMeasurements(&context, &response);

    for (const auto& m : measurements) {
        writer->Write(m.toDataPoint());
    }
    writer->WritesDone();
    auto status = writer->Finish();
    if (!status.ok()) {
        std::cerr << "SendMeasurements failed: " << status.error_message() << '\n';
    }
}

void RunChartClient::monitor() {
    grpc::ClientContext context;
    runchart::Empty request;
    auto reader = stub_->Monitor(&context, request);

    runchart::DataPoint point;
    while (reader->Read(&point)) {
        std::cout << "Measurement received by client:\n" << Measurement(point).toString() << std::endl;
    }
    auto status = reader->Finish();
    if (!status.ok()) {
        std::cerr << "Monitor finished with error: " << status.error_message() << '\n';
    }
}

void RunChartClient::sendAndCheck(const std::string& jsonPath) {
    auto measurements = importMeasurements(jsonPath);
    grpc::ClientContext context;
    auto stream = stub_->SendAndCheck(&context);

    std::thread reader([&] {
        runchart::Warning warning;
        while (stream->Read(&warning)) {
            std::cout << "Warning received: " << warning.warning() << '\n';
        }
    });

    for (const auto& m : measurements) {
        stream->Write(m.toDataPoint());
    }
    stream->WritesDone();
    reader.join();

    auto status = stream->Finish();
    if (!status.ok()) {
        std::cerr << "SendAndCheck failed: " << status.error_message() << '\n';
    }
}

std::vector<Measurement> RunChartClient::importMeasurements(const std::string& jsonPath) {
    std::ifstream input(jsonPath);
    if (!input) {
        throw std::runtime_error("Could not open " + jsonPath);
    }

    json data;
    input >> data;

    std::vector<Measurement> measurements;

    for (const auto& item : data) {
        measurements.emplace_back(
            item.at("timestamp").get<std::string>(),
            item.at("part_number").get<std::string>(),
            item.at("nominal").get<double>(),
            item.at("tolerance").get<double>(),
            item.at("measurement").get<double>()
        );
    }

    return measurements;
}
