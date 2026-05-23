#pragma once

#include <atomic>
#include <grpcpp/grpcpp.h>

#include "catalog.h"
#include "measurement.h"
#include "metrics.h"
#include "runchart.grpc.pb.h"
#include "session_auth.h"

class RunChartService final : public runchart::RunChartService::Service {
public:
    RunChartService(Catalog* catalog, SessionAuth* auth, Metrics* metrics);

    grpc::Status SnapShot(grpc::ServerContext* context,
                          const runchart::Empty* request,
                          runchart::DataPoint* response) override;

    grpc::Status SendMeasurements(grpc::ServerContext* context,
                                  grpc::ServerReader<runchart::DataPoint>* reader,
                                  runchart::Empty* response) override;

    grpc::Status Monitor(grpc::ServerContext* context,
                         const runchart::Empty* request,
                         grpc::ServerWriter<runchart::DataPoint>* writer) override;

    grpc::Status SendAndCheck(grpc::ServerContext* context,
                              grpc::ServerReaderWriter<runchart::Warning, runchart::DataPoint>* stream) override;

    grpc::Status ScanLibrary(grpc::ServerContext* context,
                             const runchart::ScanRequest* request,
                             runchart::ScanResponse* response) override;
    grpc::Status ListArtists(grpc::ServerContext* context,
                             const runchart::ListArtistsRequest* request,
                             runchart::ListArtistsResponse* response) override;
    grpc::Status ListAlbums(grpc::ServerContext* context,
                            const runchart::ListAlbumsRequest* request,
                            runchart::ListAlbumsResponse* response) override;
    grpc::Status ListTracks(grpc::ServerContext* context,
                            const runchart::ListTracksRequest* request,
                            runchart::ListTracksResponse* response) override;
    grpc::Status Search(grpc::ServerContext* context,
                        const runchart::SearchRequest* request,
                        runchart::SearchResponse* response) override;

    void stop();

private:
    void addMeasurement(const runchart::DataPoint& point, const std::string& clientIp, const std::string& requestId);
    static bool buildWarning(const Measurement& m, runchart::Warning* warning);

    Catalog* catalog_;
    SessionAuth* auth_;
    Metrics* metrics_;
    std::atomic<bool> stopping_{false};
};
