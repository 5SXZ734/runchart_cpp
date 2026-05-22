#include <iostream>
#include <memory>
#include <string>
#include <atomic>
#include <csignal>
#include <grpcpp/grpcpp.h>
#include "run_chart_service.h"

// Global server pointer for signal handler
std::unique_ptr<grpc::Server> g_server;
std::atomic<bool> g_shutdown_requested{false};

// Signal handler for Ctrl+C
void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\n\nShutdown requested (Ctrl+C)...\n";
        g_shutdown_requested = true;
        if (g_server) {
            std::cout << "Shutting down gRPC server...\n";
            g_server->Shutdown();
        }
    }
}

int main(int argc, char** argv) {
    const std::string address = "0.0.0.0:3030";

    // Register signal handler for Ctrl+C
    std::signal(SIGINT, signalHandler);

    RunChartService service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    g_server = builder.BuildAndStart();

    if (!g_server) {
        std::cerr << "Failed to start server on " << address << '\n';
        return 1;
    }

    std::cout << "Server listening on " << address << std::endl;
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;

    // Wait for server to shutdown (either by signal or explicit call)
    g_server->Wait();

    if (g_shutdown_requested) {
        std::cout << "Server shutdown complete.\n";
        return 0;
    }

    return 0;
}
