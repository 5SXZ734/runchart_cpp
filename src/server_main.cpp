#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "run_chart_service.h"

int main(int argc, char** argv) {
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

    std::cout << "Server listening on " << address << std::endl;
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;

    server->Wait();

    return 0;
}
