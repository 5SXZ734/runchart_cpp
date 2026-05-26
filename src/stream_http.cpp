#include "stream_http.h"

#include "catalog.h"
#include "session_auth.h"

#if __has_include("third_party/httplib.h")
#include "third_party/httplib.h"
#elif __has_include("httplib.h")
#include "httplib.h"
#else
#error "cpp-httplib header not found. Place it at third_party/httplib.h"
#endif

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

bool parseTrackId(const httplib::Request& req, std::int64_t* trackId) {
    if (trackId == nullptr) return false;
    const auto idText = req.path_params.at("track_id");
    try {
        *trackId = std::stoll(idText);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseRangeHeader(const httplib::Request& req, std::size_t fileSize, std::size_t* start, std::size_t* end, bool* partial) {
    if (start == nullptr || end == nullptr || partial == nullptr) return false;
    *start = 0;
    *end = fileSize > 0 ? fileSize - 1 : 0;
    *partial = false;

    const auto range = req.get_header_value("Range");
    if (range.empty()) return true;
    constexpr const char* prefix = "bytes=";
    if (range.rfind(prefix, 0) != 0) return false;

    const std::string body = range.substr(6);
    const auto dash = body.find('-');
    if (dash == std::string::npos) return false;

    const std::string startText = body.substr(0, dash);
    const std::string endText = body.substr(dash + 1);

    try {
        if (!startText.empty()) *start = static_cast<std::size_t>(std::stoull(startText));
        if (!endText.empty()) *end = static_cast<std::size_t>(std::stoull(endText));
    } catch (...) {
        return false;
    }

    *partial = true;
    return true;
}

}  // namespace

HttpStreamServer::HttpStreamServer(std::string bindAddress, int port, MetricsFn metricsFn, const Catalog* catalog, const SessionAuth* auth)
    : bindAddress_(std::move(bindAddress)), port_(port), metricsFn_(std::move(metricsFn)), catalog_(catalog), auth_(auth) {}

HttpStreamServer::~HttpStreamServer() { stop(); }

bool HttpStreamServer::start() {
    if (running_.exchange(true)) return true;

    auto server = std::make_unique<httplib::Server>();

    server->Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("ok", "text/plain");
    });

    server->Get("/metrics", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(metricsFn_ ? metricsFn_() : std::string{}, "text/plain");
    });

    server->Get(R"(/stream/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
        if (catalog_ == nullptr || auth_ == nullptr) {
            res.status = 500;
            res.set_content("server misconfigured\n", "text/plain");
            return;
        }

        const auto token = req.get_header_value("x-auth-token");
        if (!auth_->isAuthorizedToken(token)) {
            res.status = 401;
            res.set_content("unauthorized\n", "text/plain");
            return;
        }

        std::int64_t trackId = 0;
        if (!parseTrackId(req, &trackId)) {
            res.status = 404;
            res.set_content("track not found\n", "text/plain");
            return;
        }

        TrackRecord track{};
        if (!catalog_->findTrackById(trackId, &track)) {
            res.status = 404;
            res.set_content("track not found\n", "text/plain");
            return;
        }

        std::ifstream file(track.filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            res.status = 404;
            res.set_content("audio file missing\n", "text/plain");
            return;
        }

        const std::size_t fileSize = static_cast<std::size_t>(file.tellg());
        std::size_t start = 0;
        std::size_t end = fileSize > 0 ? fileSize - 1 : 0;
        bool partial = false;

        if (!parseRangeHeader(req, fileSize, &start, &end, &partial)) {
            res.status = 416;
            res.set_header("Content-Range", "bytes */" + std::to_string(fileSize));
            return;
        }

        if (fileSize == 0 || start >= fileSize || end < start) {
            res.status = 416;
            res.set_header("Content-Range", "bytes */" + std::to_string(fileSize));
            return;
        }

        end = std::min(end, fileSize - 1);
        const std::size_t contentLength = end - start + 1;
        file.seekg(static_cast<std::streamoff>(start), std::ios::beg);

        std::vector<char> payload(contentLength);
        file.read(payload.data(), static_cast<std::streamsize>(contentLength));
        const std::size_t got = static_cast<std::size_t>(file.gcount());

        res.status = partial ? 206 : 200;
        res.set_header("Accept-Ranges", "bytes");
        if (partial) {
            res.set_header("Content-Range", "bytes " + std::to_string(start) + "-" + std::to_string(start + got - 1) + "/" + std::to_string(fileSize));
        }
        res.set_content(std::string(payload.data(), got), "audio/mpeg");
    });

    server_ = server.release();
    worker_ = std::thread([this]() {
        server_->listen(bindAddress_.c_str(), port_);
        running_.store(false);
    });
    return true;
}

void HttpStreamServer::stop() {
    if (!running_.exchange(false)) return;
    if (server_ != nullptr) server_->stop();
    if (worker_.joinable()) worker_.join();
    delete server_;
    server_ = nullptr;
}
