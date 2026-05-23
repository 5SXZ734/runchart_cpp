#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "run_chart_client.h"

void printUsage(const char* p) {
    std::cerr << "Usage:\n";
    std::cerr << "  " << p << " <server> send_and_check <data.json>\n";
    std::cerr << "  " << p << " <server> scan <music_root>\n";
    std::cerr << "  " << p << " <server> list_artists\n";
    std::cerr << "  " << p << " <server> list_albums\n";
    std::cerr << "  " << p << " <server> list_tracks\n";
    std::cerr << "  " << p << " <server> search <query>\n";
}

int main(int argc, char** argv) {
    if (argc < 3) { printUsage(argv[0]); return 1; }
    const std::string serverAddress = argv[1];
    const std::string cmd = argv[2];

    try {
        auto channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());
        RunChartClient client(channel);
        if (cmd == "send_and_check" && argc == 4) client.sendAndCheck(argv[3]);
        else if (cmd == "scan" && argc == 4) std::cout << "Scanned tracks: " << client.scanLibrary(argv[3]) << "\n";
        else if (cmd == "list_artists") client.listArtists();
        else if (cmd == "list_albums") client.listAlbums();
        else if (cmd == "list_tracks") client.listTracks();
        else if (cmd == "search" && argc == 4) client.search(argv[3]);
        else { printUsage(argv[0]); return 1; }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
