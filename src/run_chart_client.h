#pragma once

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <grpcpp/grpcpp.h>
#include "measurement.h"
#include "runchart.grpc.pb.h"

struct ClientArtist {
    std::int64_t id;
    std::string name;
};

struct ClientAlbum {
    std::int64_t id;
    std::string title;
    std::string artistName;
};

struct ClientTrack {
    std::int64_t id;
    std::string title;
    std::string artistName;
    std::string albumTitle;
    int trackNumber;
    std::string filePath;
};

class RunChartClient {
public:
    explicit RunChartClient(std::shared_ptr<grpc::Channel> channel);

    Measurement getSnapShot();
    void sendMeasurements(const std::string& jsonPath);
    void monitor();
    void sendAndCheck(const std::string& jsonPath);
    std::uint32_t scanLibrary(const std::string& rootPath);
    std::vector<ClientArtist> listArtistsData();
    std::vector<ClientAlbum> listAlbumsData();
    std::vector<ClientTrack> listTracksData();
    void listArtists();
    void listAlbums();
    void listTracks();
    ClientTrack getTrack(std::int64_t trackId);
    void search(const std::string& query);

private:
    static std::vector<Measurement> importMeasurements(const std::string& jsonPath);

    std::unique_ptr<runchart::RunChartService::Stub> stub_;
};
