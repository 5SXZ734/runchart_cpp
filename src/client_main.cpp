#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "run_chart_client.h"

void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " <server_address> <data.json>\n";
    std::cerr << "Example: " << programName << " localhost:3030 data.json\n";
    std::cerr << "Example: " << programName << " 192.168.1.4:3030 data.json\n";
}

int main(int argc, char** argv) {
    if (argc != 3) {
        printUsage(argv[0]);
        return 1;
    }

    const std::string serverAddress = argv[1];
    const std::string dataPath = argv[2];

    try {
        auto channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());
        RunChartClient client(channel);

        std::cout << "Connecting to server at " << serverAddress << std::endl;
        std::cout << "Loading measurements from " << dataPath << std::endl;

        // Run sendAndCheck which both sends measurements and receives warnings
        client.sendAndCheck(dataPath);

        std::cout << "Demo completed successfully.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
