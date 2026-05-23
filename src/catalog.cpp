#include "catalog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sqlite3.h>

namespace {

	std::string lowerAscii(std::string value) {
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
			});
		return value;
	}

	bool isSupportedAudioExtension(const std::filesystem::path& path) {
		std::string ext = path.extension().u8string();
		ext = lowerAscii(ext);
		return ext == ".mp3" || ext == ".flac" || ext == ".m4a";
	}

	std::string utf8PathString(const std::filesystem::path& path) {
		return path.u8string();
	}

}  // namespace

Catalog::Catalog(const std::string& dbPath) : dbPath_(dbPath) { initDb(); }

void Catalog::initDb() {
	sqlite3* db = nullptr;
	if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) return;
	const char* ddl =
		"CREATE TABLE IF NOT EXISTS artists(id INTEGER PRIMARY KEY, name TEXT UNIQUE);"
		"CREATE TABLE IF NOT EXISTS albums(id INTEGER PRIMARY KEY, title TEXT, artist_id INTEGER, UNIQUE(title, artist_id));"
		"CREATE TABLE IF NOT EXISTS tracks(id INTEGER PRIMARY KEY, title TEXT, album_id INTEGER, artist_id INTEGER, track_number INTEGER, file_path TEXT UNIQUE);";
	sqlite3_exec(db, ddl, nullptr, nullptr, nullptr);
	sqlite3_close(db);
}

std::size_t Catalog::scanFromNasPath(const std::string& nasPath) {
	namespace fs = std::filesystem;
	sqlite3* db = nullptr;
	if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK) return 0;
	sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr);

	std::size_t count = 0;
	std::error_code ec;
	fs::recursive_directory_iterator it(fs::path(nasPath), fs::directory_options::skip_permission_denied, ec);
	fs::recursive_directory_iterator end;

	while (it != end) {
		if (ec) {
			it.increment(ec);
			continue;
		}

		const fs::directory_entry entry = *it;
		it.increment(ec);

		std::error_code entryEc;
		if (!entry.is_regular_file(entryEc) || entryEc) {
			continue;
		}

		if (!isSupportedAudioExtension(entry.path())) {
			continue;
		}

		std::string pathUtf8;
		std::string title;
		try {
			pathUtf8 = utf8PathString(entry.path());
			title = utf8PathString(entry.path().stem());
		}
		catch (...) {
			continue;
		}

		std::string artist = "Unknown Artist";
		std::string album = "Unknown Album";
		int trackNo = 0;

		sqlite3_stmt* stmt = nullptr;
		sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO artists(name) VALUES(?)", -1, &stmt, nullptr);
		sqlite3_bind_text(stmt, 1, artist.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		sqlite3_prepare_v2(db, "SELECT id FROM artists WHERE name=?", -1, &stmt, nullptr);
		sqlite3_bind_text(stmt, 1, artist.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		const std::int64_t artistId = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);

		sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO albums(title, artist_id) VALUES(?,?)", -1, &stmt, nullptr);
		sqlite3_bind_text(stmt, 1, album.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, artistId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		sqlite3_prepare_v2(db, "SELECT id FROM albums WHERE title=? AND artist_id=?", -1, &stmt, nullptr);
		sqlite3_bind_text(stmt, 1, album.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, artistId);
		sqlite3_step(stmt);
		const std::int64_t albumId = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);

		sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO tracks(title, album_id, artist_id, track_number, file_path) VALUES(?,?,?,?,?)", -1, &stmt, nullptr);
		sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, albumId);
		sqlite3_bind_int64(stmt, 3, artistId);
		sqlite3_bind_int(stmt, 4, trackNo);
		sqlite3_bind_text(stmt, 5, pathUtf8.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		++count;
	}

	sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr);
	sqlite3_close(db);
	return count;
}

std::vector<ArtistRecord> Catalog::listArtists() const {
	std::vector<ArtistRecord> out;
	sqlite3* db = nullptr;
	if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK)
		return out;
	sqlite3_stmt* stmt = nullptr;
	sqlite3_prepare_v2(db, "SELECT id,name FROM artists ORDER BY name", -1, &stmt, nullptr);
	while (sqlite3_step(stmt) == SQLITE_ROW)
		out.push_back(
			{ sqlite3_column_int64(stmt,0), reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)) }
		);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return out;
}

std::vector<AlbumRecord> Catalog::listAlbums() const {
	std::vector<AlbumRecord> out; sqlite3* db = nullptr;
	if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK)
		return out;
	sqlite3_stmt* stmt = nullptr;
	sqlite3_prepare_v2(db, "SELECT a.id,a.title,ar.name FROM albums a JOIN artists ar ON ar.id=a.artist_id ORDER BY ar.name,a.title", -1, &stmt, nullptr);
	while (sqlite3_step(stmt) == SQLITE_ROW)
		out.push_back(
			{ sqlite3_column_int64(stmt,0),
			reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)),
			reinterpret_cast<const char*>(sqlite3_column_text(stmt,2)) }
		);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return out;
}

std::vector<TrackRecord> Catalog::listTracks() const {
	return searchTracks("");
}

std::vector<ArtistRecord> Catalog::searchArtists(const std::string& query) const {
	std::vector<ArtistRecord> out; sqlite3* db = nullptr;
	if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK)
		return out;
	sqlite3_stmt* stmt = nullptr;
	sqlite3_prepare_v2(db, "SELECT id,name FROM artists WHERE name LIKE ? ORDER BY name", -1, &stmt, nullptr);
	std::string q = "%" + query + "%";
	sqlite3_bind_text(stmt, 1, q.c_str(), -1, SQLITE_TRANSIENT);
	while (sqlite3_step(stmt) == SQLITE_ROW)
		out.push_back(
			{ sqlite3_column_int64(stmt,0),
			reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)) }
		);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return out;
}
std::vector<AlbumRecord> Catalog::searchAlbums(const std::string& query) const {
	std::vector<AlbumRecord> out;
	sqlite3* db = nullptr;
	if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK)
		return out;
	sqlite3_stmt* stmt = nullptr;
	sqlite3_prepare_v2(
		db,
		"SELECT a.id,a.title,ar.name "
		"FROM albums a "
		"JOIN artists ar ON ar.id=a.artist_id "
		"WHERE a.title LIKE ? OR ar.name LIKE ? "
		"ORDER BY ar.name,a.title",
		-1,
		&stmt,
		nullptr);
	std::string q = "%" + query + "%";
	sqlite3_bind_text(stmt, 1, q.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, q.c_str(), -1, SQLITE_TRANSIENT);
	while (sqlite3_step(stmt) == SQLITE_ROW)
		out.push_back(
			{ sqlite3_column_int64(stmt,0),
			reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)),
			reinterpret_cast<const char*>(sqlite3_column_text(stmt,2)) }
		);
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return out;
}

std::vector<TrackRecord> Catalog::searchTracks(const std::string& query) const {
	std::vector<TrackRecord> out;
	sqlite3* db = nullptr;
	if (sqlite3_open(dbPath_.c_str(), &db) != SQLITE_OK)
		return out;
	sqlite3_stmt* stmt = nullptr;
	sqlite3_prepare_v2(db, "SELECT t.id,t.title,ar.name,al.title,t.track_number,t.file_path FROM tracks t JOIN artists ar ON ar.id=t.artist_id JOIN albums al ON al.id=t.album_id WHERE ?='' OR t.title LIKE ? OR ar.name LIKE ? OR al.title LIKE ? ORDER BY ar.name,al.title,t.track_number,t.title", -1, &stmt, nullptr);
	std::string q = "%" + query + "%";
	sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, q.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, q.c_str(), -1, SQLITE_TRANSIENT); sqlite3_bind_text(stmt, 4, q.c_str(), -1, SQLITE_TRANSIENT);
	while (sqlite3_step(stmt) == SQLITE_ROW) out.push_back(
		{ sqlite3_column_int64(stmt,0),
		reinterpret_cast<const char*>(sqlite3_column_text(stmt,1)),
		reinterpret_cast<const char*>(sqlite3_column_text(stmt,2)),
		reinterpret_cast<const char*>(sqlite3_column_text(stmt,3)),
		sqlite3_column_int(stmt,4),
		reinterpret_cast<const char*>(sqlite3_column_text(stmt,5)) });
	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return out;
}






void Catalog::addMeasurement(const Measurement& measurement) {
	{
		std::lock_guard<std::mutex> lock(mutex_);
		measurements_.push_back(measurement);
	}
	cv_.notify_all();
}

Measurement Catalog::latestOrDefault() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return measurements_.empty() ? Measurement::defaultMeasurement() : measurements_.back();
}

std::vector<Measurement> Catalog::since(std::size_t index) const {
	std::lock_guard<std::mutex> lock(mutex_);
	if (index >= measurements_.size())
		return {};
	return std::vector<Measurement>(measurements_.begin() + static_cast<std::ptrdiff_t>(index), measurements_.end());
}

std::size_t Catalog::size() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return measurements_.size();
}

void Catalog::waitForUpdate(std::size_t nextIndex, std::atomic<bool>* stopFlag) const {
	std::unique_lock<std::mutex> lock(mutex_);
	cv_.wait(lock, [&] {
		return (stopFlag != nullptr && stopFlag->load()) || nextIndex < measurements_.size();
		});
}
