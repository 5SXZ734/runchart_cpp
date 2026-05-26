#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "measurement.h"

struct ArtistRecord {
    std::int64_t id;
    std::string name;
};

struct AlbumRecord {
    std::int64_t id;
    std::string title;
    std::string artistName;
};

struct TrackRecord {
    std::int64_t id;
    std::string title;
    std::string artistName;
    std::string albumTitle;
    int trackNumber;
    std::string filePath;
};

class Catalog {
public:
    explicit Catalog(const std::string& dbPath = "library.db");

    std::size_t scanFromNasPath(const std::string& nasPath);
    std::vector<ArtistRecord> listArtists() const;
    std::vector<AlbumRecord> listAlbums() const;
    std::vector<TrackRecord> listTracks() const;
    bool findTrackById(std::int64_t trackId, TrackRecord* out) const;
    std::vector<ArtistRecord> searchArtists(const std::string& query) const;
    std::vector<AlbumRecord> searchAlbums(const std::string& query) const;
    std::vector<TrackRecord> searchTracks(const std::string& query) const;

    void addMeasurement(const Measurement& measurement);
    Measurement latestOrDefault() const;
    std::vector<Measurement> since(std::size_t index) const;
    std::size_t size() const;
    void waitForUpdate(std::size_t nextIndex, std::atomic<bool>* stopFlag) const;

private:
    void initDb();

    std::string dbPath_;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::vector<Measurement> measurements_;
};
