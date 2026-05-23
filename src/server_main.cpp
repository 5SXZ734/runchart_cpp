#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "catalog.h"
#include "config.h"
#include "metrics.h"
#include "run_chart_service.h"
#include "session_auth.h"
#include "stream_http.h"
#include "structured_logger.h"

namespace {
std::atomic<bool> g_shutdown_requested{false};
extern "C" void HandleSigInt(int signal) {
    if (signal == SIGINT) g_shutdown_requested.store(true, std::memory_order_relaxed);
}
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    const ServerConfig config = ServerConfig::load();
    StructuredLogger::instance().setEnabled(config.structured_logging);
    StructuredLogger::instance().setLogPath("runchart_server.log");

#if defined(_WIN32)
    std::signal(SIGINT, HandleSigInt);
#else
    sigset_t signal_set;
    sigemptyset(&signal_set);
    sigaddset(&signal_set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &signal_set, nullptr);
#endif

    Catalog catalog;
    catalog.scanFromNasPath(config.nas_scan_path);
    SessionAuth auth(config.auth_secret);
    Metrics metrics;
    RunChartService service(&catalog, &auth, &metrics);

    HttpStreamServer httpServer(config.http_bind_address, config.http_port, [&metrics]() {
        metrics.health_requests.fetch_add(1);
        return metrics.toPrometheusText();
    });
    httpServer.start();

    grpc::ServerBuilder builder;
    builder.AddListeningPort(config.grpc_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<grpc::Server> server = builder.BuildAndStart();

    if (!server) {
        std::cerr << "Failed to start server on " << config.grpc_address << '\n';
        return 1;
    }

#if defined(_WIN32)
    std::thread signal_thread([&server]() {
        while (!g_shutdown_requested.load(std::memory_order_relaxed)) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        server->Shutdown();
    });
#else
    std::thread signal_thread([&server, signal_set]() mutable {
        int received_signal = 0;
        if (sigwait(&signal_set, &received_signal) == 0 && received_signal == SIGINT) server->Shutdown();
    });
#endif

    StructuredLogger::instance().log("INFO", "server_start", "", "", {{"address", config.grpc_address}});
    std::cout << "Server listening on " << config.grpc_address << std::endl;

    server->Wait();

    service.stop();
    httpServer.stop();
    if (signal_thread.joinable()) signal_thread.join();

    StructuredLogger::instance().log("INFO", "server_shutdown", "", "");
    return 0;
}
