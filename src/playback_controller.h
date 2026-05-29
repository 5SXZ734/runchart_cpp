#pragma once

#include "audio_player.h"
#include "run_chart_client.h"

#include <cstddef>
#include <optional>
#include <vector>

class PlaybackController {
public:
    PlaybackController();

    void setQueue(std::vector<ClientTrack> tracks);
    void playTrack(const ClientTrack& track);
    void playOrPause();
    void stop();
    void next();
    void setVolume(int volume);
    void setPositionPercent(int percent);

    bool hasCurrentTrack() const;
    bool isPlaying() const;
    int volume() const;
    int positionPercent() const;
    long long durationMs() const;
    long long currentTimeMs() const;
    const ClientTrack* currentTrack() const;

private:
    void playCurrentTrack();
    std::optional<std::size_t> findTrackIndex(std::int64_t trackId) const;

    LibVlcPlayer player_;
    std::vector<ClientTrack> queue_;
    std::optional<ClientTrack> currentTrack_;
    std::optional<std::size_t> currentIndex_;
    bool stopped_ = true;
};
