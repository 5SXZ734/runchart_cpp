#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "run_chart_service.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const std::string address = "0.0.0.0:3030";

    // Handle SIGINT on a dedicated thread using sigwait().
    // This avoids calling non-signal-safe APIs from a signal handler.
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &signal_set, nullptr);

    RunChartService service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

    if (!server) {
        std::cerr << "Failed to start server on " << address << '\n';
        return 1;
    }

    std::thread signal_thread([&server, signal_set]() mutable {
        int received_signal = 0;
        if (sigwait(&signal_set, &received_signal) == 0 && received_signal == SIGINT) {
            std::cout << "\n\nShutdown requested (Ctrl+C)...\n";
            std::cout << "Shutting down gRPC server...\n";
            server->Shutdown();
        }
    });

    std::cout << "Server listening on " << address << std::endl;
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;

    server->Wait();

    if (signal_thread.joinable()) {
        signal_thread.join();
    }

    std::cout << "Server shutdown complete.\n";
    return 0;
}
