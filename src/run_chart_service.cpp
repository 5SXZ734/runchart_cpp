#include "run_chart_service.h"

#include <iostream>
#include <random>
#include <string>

#include "structured_logger.h"

using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

namespace {
std::string extractClientIp(const grpc::ServerContext* context) {
    const std::string peer = context ? context->peer() : "";
    const auto schemeSep = peer.find(':');
    if (schemeSep == std::string::npos) return peer;
    const std::string addressPort = peer.substr(schemeSep + 1);
    if (addressPort.empty()) return "";
    if (addressPort.front() == '[') {
        const auto bracketEnd = addressPort.find(']');
        if (bracketEnd != std::string::npos) return addressPort.substr(1, bracketEnd - 1);
        return addressPort;
    }
    const auto lastColon = addressPort.rfind(':');
    if (lastColon == std::string::npos) return addressPort;
    return addressPort.substr(0, lastColon);
}
std::string buildRequestId(const grpc::ServerContext* context) {
    if (context != nullptr) {
        const auto& metadata = context->client_metadata();
        const auto it = metadata.find("x-request-id");
        if (it != metadata.end()) return std::string(it->second.data(), it->second.length());
    }
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<unsigned long long> dist;
    return std::to_string(dist(rng));
}
}

RunChartService::RunChartService(Catalog* catalog, SessionAuth* auth, Metrics* metrics)
    : catalog_(catalog), auth_(auth), metrics_(metrics) {}

Status RunChartService::SnapShot(ServerContext* context, const runchart::Empty*, runchart::DataPoint* response) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    metrics_->snapshot_requests.fetch_add(1);
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    *response = catalog_->latestOrDefault().toDataPoint();
    StructuredLogger::instance().log("INFO", "snapshot", clientIp, requestId);
    return Status::OK;
}

Status RunChartService::SendMeasurements(ServerContext* context, ServerReader<runchart::DataPoint>* reader, runchart::Empty*) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    metrics_->send_measurements_requests.fetch_add(1);
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    runchart::DataPoint point;
    while (reader->Read(&point)) addMeasurement(point, clientIp, requestId);
    return Status::OK;
}

Status RunChartService::Monitor(ServerContext* context, const runchart::Empty*, ServerWriter<runchart::DataPoint>* writer) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    metrics_->monitor_requests.fetch_add(1);
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    StructuredLogger::instance().log("INFO", "monitor_start", clientIp, requestId);
    std::size_t nextIndex = 0;
    while (!context->IsCancelled() && !stopping_.load()) {
        catalog_->waitForUpdate(nextIndex, &stopping_);
        for (const auto& m : catalog_->since(nextIndex)) {
            ++nextIndex;
            if (!writer->Write(m.toDataPoint())) return Status::OK;
        }
    }
    StructuredLogger::instance().log("INFO", "monitor_end", clientIp, requestId);
    return Status::OK;
}

Status RunChartService::SendAndCheck(ServerContext* context, ServerReaderWriter<runchart::Warning, runchart::DataPoint>* stream) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    metrics_->send_and_check_requests.fetch_add(1);
    const std::string clientIp = extractClientIp(context);
    const std::string requestId = buildRequestId(context);
    runchart::DataPoint point;
    while (stream->Read(&point)) {
        addMeasurement(point, clientIp, requestId);
        Measurement m(point);
        runchart::Warning warning;
        if (buildWarning(m, &warning)) {
            StructuredLogger::instance().log("WARNING", "spec_warning", clientIp, requestId, {{"part_number", m.partNumber}, {"warning", warning.warning()}});
            stream->Write(warning);
        }
    }
    return Status::OK;
}

void RunChartService::addMeasurement(const runchart::DataPoint& point, const std::string& clientIp, const std::string& requestId) {
    Measurement m(point);
    StructuredLogger::instance().log("INFO", "measurement_received", clientIp, requestId, {{"part_number", m.partNumber}, {"measurement", std::to_string(m.measurement)}});
    std::cout << "Measurement received by server:\n" << m.toString() << std::endl;
    catalog_->addMeasurement(m);
}

bool RunChartService::buildWarning(const Measurement& m, runchart::Warning* warning) {
    std::string text;
    if (m.measurement > m.nominal + m.tolerance) text = "Measurement is above spec";
    else if (m.measurement < m.nominal - m.tolerance) text = "Measurement is below spec";
    else return false;
    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    warning->mutable_timestamp()->set_seconds(seconds);
    warning->set_part_number(m.partNumber);
    warning->set_spec_nominal(m.nominal);
    warning->set_spec_tolerance(m.tolerance);
    warning->set_warning(text);
    return true;
}

void RunChartService::stop() { stopping_.store(true); }


Status RunChartService::ScanLibrary(ServerContext* context, const runchart::ScanRequest* request, runchart::ScanResponse* response) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    const std::size_t scanned = catalog_->scanFromNasPath(request->root_path().empty() ? "./" : request->root_path());
    response->set_tracks_scanned(static_cast<std::uint32_t>(scanned));
    return Status::OK;
}

Status RunChartService::ListArtists(ServerContext* context, const runchart::ListArtistsRequest*, runchart::ListArtistsResponse* response) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    for (const auto& a : catalog_->listArtists()) { auto* out = response->add_artists(); out->set_id(a.id); out->set_name(a.name);}
    return Status::OK;
}

Status RunChartService::ListAlbums(ServerContext* context, const runchart::ListAlbumsRequest*, runchart::ListAlbumsResponse* response) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    for (const auto& a : catalog_->listAlbums()) { auto* out = response->add_albums(); out->set_id(a.id); out->set_title(a.title); out->set_artist_name(a.artistName);}
    return Status::OK;
}

Status RunChartService::ListTracks(ServerContext* context, const runchart::ListTracksRequest*, runchart::ListTracksResponse* response) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    for (const auto& t : catalog_->listTracks()) { auto* out = response->add_tracks(); out->set_id(t.id); out->set_title(t.title); out->set_artist_name(t.artistName); out->set_album_title(t.albumTitle); out->set_track_number(t.trackNumber); out->set_file_path(t.filePath);}
    return Status::OK;
}

Status RunChartService::Search(ServerContext* context, const runchart::SearchRequest* request, runchart::SearchResponse* response) {
    if (!auth_->isAuthorized(context)) return {grpc::StatusCode::UNAUTHENTICATED, "Invalid auth token"};
    const std::string q = request->query();
    for (const auto& a : catalog_->searchArtists(q)) { auto* out = response->add_artists(); out->set_id(a.id); out->set_name(a.name);}
    for (const auto& a : catalog_->searchAlbums(q)) { auto* out = response->add_albums(); out->set_id(a.id); out->set_title(a.title); out->set_artist_name(a.artistName);}
    for (const auto& t : catalog_->searchTracks(q)) { auto* out = response->add_tracks(); out->set_id(t.id); out->set_title(t.title); out->set_artist_name(t.artistName); out->set_album_title(t.albumTitle); out->set_track_number(t.trackNumber); out->set_file_path(t.filePath);}
    return Status::OK;
}
