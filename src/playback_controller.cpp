#include "playback_controller.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

PlaybackController::PlaybackController() {
    player_.setVolume(80);
}

void PlaybackController::setQueue(std::vector<ClientTrack> tracks) {
    queue_ = std::move(tracks);
    if (currentTrack_) currentIndex_ = findTrackIndex(currentTrack_->id);
}

void PlaybackController::playTrack(const ClientTrack& track) {
    if (track.filePath.empty()) {
        throw std::runtime_error("Track has no file path: " + track.title);
    }

    currentTrack_ = track;
    currentIndex_ = findTrackIndex(track.id);
    playCurrentTrack();
}

void PlaybackController::playOrPause() {
    if (!currentTrack_) return;

    if (stopped_) {
        playCurrentTrack();
        return;
    }

    player_.togglePause();
}

void PlaybackController::stop() {
    player_.stop();
    stopped_ = true;
}

void PlaybackController::next() {
    if (queue_.empty()) return;

    std::size_t nextIndex = 0;
    if (currentIndex_) nextIndex = (*currentIndex_ + 1) % queue_.size();
    currentTrack_ = queue_[nextIndex];
    currentIndex_ = nextIndex;
    playCurrentTrack();
}

void PlaybackController::setVolume(int volume) {
    player_.setVolume(volume);
}

void PlaybackController::setPositionPercent(int percent) {
    const float position = std::clamp(percent, 0, 100) / 100.0F;
    player_.setPosition(position);
}

bool PlaybackController::hasCurrentTrack() const {
    return currentTrack_.has_value();
}

bool PlaybackController::isPlaying() const {
    return player_.isPlaying();
}

int PlaybackController::volume() const {
    return player_.volume();
}

int PlaybackController::positionPercent() const {
    const float position = player_.position();
    if (!std::isfinite(position) || position < 0.0F) return 0;
    return static_cast<int>(std::round(std::clamp(position, 0.0F, 1.0F) * 100.0F));
}

long long PlaybackController::durationMs() const {
    return player_.durationMs();
}

long long PlaybackController::currentTimeMs() const {
    return player_.currentTimeMs();
}

const ClientTrack* PlaybackController::currentTrack() const {
    return currentTrack_ ? &*currentTrack_ : nullptr;
}

void PlaybackController::playCurrentTrack() {
    if (!currentTrack_) return;
    player_.playFile(currentTrack_->filePath);
    stopped_ = false;
}

std::optional<std::size_t> PlaybackController::findTrackIndex(std::int64_t trackId) const {
    const auto it = std::find_if(queue_.begin(), queue_.end(), [trackId](const ClientTrack& track) {
        return track.id == trackId;
    });
    if (it == queue_.end()) return std::nullopt;
    return static_cast<std::size_t>(std::distance(queue_.begin(), it));
}
