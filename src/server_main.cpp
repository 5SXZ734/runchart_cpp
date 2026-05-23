#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "run_chart_service.h"

namespace {

std::atomic<bool> g_shutdown_requested{false};

extern "C" void HandleSigInt(int signal) {
    if (signal == SIGINT) {
        g_shutdown_requested.store(true, std::memory_order_relaxed);
    }
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const std::string address = "0.0.0.0:3030";

    RunChartService service;
    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

    if (!server) {
        std::cerr << "Failed to start server on " << address << '\n';
        return 1;
    }

#if defined(_WIN32)
    // Portable fallback for non-POSIX toolchains.
    std::signal(SIGINT, HandleSigInt);

    std::thread signal_thread([&server]() {
        while (!g_shutdown_requested.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "\n\nShutdown requested (Ctrl+C)...\n";
        std::cout << "Shutting down gRPC server...\n";
        server->Shutdown();
    });
#else
    // Handle SIGINT on a dedicated thread using sigwait().
    // This avoids calling non-signal-safe APIs from a signal handler.
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &signal_set, nullptr);

    std::thread signal_thread([&server, signal_set]() mutable {
        int received_signal = 0;
        if (sigwait(&signal_set, &received_signal) == 0 && received_signal == SIGINT) {
            std::cout << "\n\nShutdown requested (Ctrl+C)...\n";
            std::cout << "Shutting down gRPC server...\n";
            server->Shutdown();
        }
    });
#endif

    std::cout << "Server listening on " << address << std::endl;
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;

    server->Wait();

    if (signal_thread.joinable()) {
        signal_thread.join();
    }

    std::cout << "Server shutdown complete.\n";
    return 0;
}
