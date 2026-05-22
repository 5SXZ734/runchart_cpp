#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "run_chart_client.h"
#include "run_chart_service.h"

int main(int argc, char** argv) {
    const std::string dataPath = argc > 1 ? argv[1] : "data.json";
    const std::string address = "0.0.0.0:3030";

    RunChartService service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());

    if (!server) {
        std::cerr << "Failed to start server on " << address << '\n';
        return 1;
    }
    std::cout << "server started..." << std::endl;

    auto channel = grpc::CreateChannel("localhost:3030", grpc::InsecureChannelCredentials());
    RunChartClient client(channel);

    std::thread monitorThread([&] { client.monitor(); });
    std::thread sendAndCheckThread([&] { client.sendAndCheck(dataPath); });

    sendAndCheckThread.join();
    service.stop();
    server->Shutdown();
    monitorThread.join();

    return 0;
}
