#include "run_chart_client.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {
void addAuthToken(grpc::ClientContext* context) {
    if (context == nullptr) return;
    const char* token = std::getenv("RUNCHART_AUTH_TOKEN");
    if (token == nullptr || *token == '\0') token = std::getenv("RUNCHART_AUTH_SECRET");
    if (token != nullptr && *token != '\0') context->AddMetadata("x-auth-token", token);
}
}

RunChartClient::RunChartClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(runchart::RunChartService::NewStub(std::move(channel))) {}

Measurement RunChartClient::getSnapShot() {
    grpc::ClientContext context;
    runchart::Empty request;
    addAuthToken(&context);
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
    addAuthToken(&context);
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
    addAuthToken(&context);
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
    addAuthToken(&context);
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


std::uint32_t RunChartClient::scanLibrary(const std::string& rootPath) {
    grpc::ClientContext context;
    runchart::ScanRequest req;
    addAuthToken(&context);
    runchart::ScanResponse resp;
    req.set_root_path(rootPath);
    auto status = stub_->ScanLibrary(&context, req, &resp);
    if (!status.ok()) throw std::runtime_error("ScanLibrary failed: " + status.error_message());
    return resp.tracks_scanned();
}

void RunChartClient::listArtists() {
    grpc::ClientContext context;
    runchart::ListArtistsRequest req;
    addAuthToken(&context);
    runchart::ListArtistsResponse resp;
    auto status = stub_->ListArtists(&context, req, &resp);
    if (!status.ok()) throw std::runtime_error("ListArtists failed: " + status.error_message());
    for (const auto& a : resp.artists()) std::cout << a.id() << " | " << a.name() << "\n";
}

void RunChartClient::listAlbums() {
    grpc::ClientContext context; runchart::ListAlbumsRequest req; runchart::ListAlbumsResponse resp;
    addAuthToken(&context);
    auto status = stub_->ListAlbums(&context, req, &resp);
    if (!status.ok()) throw std::runtime_error("ListAlbums failed: " + status.error_message());
    for (const auto& a : resp.albums()) std::cout << a.id() << " | " << a.artist_name() << " | " << a.title() << "\n";
}

void RunChartClient::listTracks() {
    grpc::ClientContext context; runchart::ListTracksRequest req; runchart::ListTracksResponse resp;
    addAuthToken(&context);
    auto status = stub_->ListTracks(&context, req, &resp);
    if (!status.ok()) throw std::runtime_error("ListTracks failed: " + status.error_message());
    for (const auto& t : resp.tracks()) std::cout << t.id() << " | " << t.artist_name() << " | " << t.album_title() << " | " << t.track_number() << " | " << t.title() << " | " << t.file_path() << "\n";
}

ClientTrack RunChartClient::getTrack(std::int64_t trackId) {
    grpc::ClientContext context;
    runchart::GetTrackRequest req;
    runchart::GetTrackResponse resp;
    req.set_id(trackId);
    addAuthToken(&context);
    auto status = stub_->GetTrack(&context, req, &resp);
    if (!status.ok()) throw std::runtime_error("GetTrack failed: " + status.error_message());
    const auto& t = resp.track();
    return ClientTrack{t.id(), t.title(), t.artist_name(), t.album_title(), t.track_number(), t.file_path()};
}

void RunChartClient::search(const std::string& query) {
    grpc::ClientContext context; runchart::SearchRequest req; runchart::SearchResponse resp; req.set_query(query);
    addAuthToken(&context);
    auto status = stub_->Search(&context, req, &resp);
    if (!status.ok()) throw std::runtime_error("Search failed: " + status.error_message());
    std::cout << "Artists:\n"; for (const auto& a : resp.artists()) std::cout << "  - " << a.name() << "\n";
    std::cout << "Albums:\n"; for (const auto& a : resp.albums()) std::cout << "  - " << a.artist_name() << " / " << a.title() << "\n";
    std::cout << "Tracks:\n"; for (const auto& t : resp.tracks()) std::cout << "  - " << t.id() << " | " << t.artist_name() << " / " << t.album_title() << " / " << t.title() << " | " << t.file_path() << "\n";
}
