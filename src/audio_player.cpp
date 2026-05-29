#include "audio_player.h"

#include <stdexcept>

#ifdef RUNCHART_WITH_LIBVLC

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>

#include <vlc/vlc.h>

namespace {
struct PlaybackState {
    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    bool failed = false;
};

void handlePlaybackEvent(const libvlc_event_t* event, void* userData) {
    auto* state = static_cast<PlaybackState*>(userData);
    if (state == nullptr || event == nullptr) return;

    std::lock_guard<std::mutex> lock(state->mutex);
    switch (event->type) {
    case libvlc_MediaPlayerEndReached:
    case libvlc_MediaPlayerStopped:
        state->finished = true;
        state->cv.notify_all();
        break;
    case libvlc_MediaPlayerEncounteredError:
        state->finished = true;
        state->failed = true;
        state->cv.notify_all();
        break;
    default:
        break;
    }
}
}

struct AudioPlayer::Impl {
    libvlc_instance_t* vlc = nullptr;

    Impl() {
        const char* args[] = {"--no-video", "--quiet"};
        vlc = libvlc_new(static_cast<int>(sizeof(args) / sizeof(args[0])), args);
        if (vlc == nullptr) {
            throw std::runtime_error("Could not initialize libVLC");
        }
    }

    ~Impl() {
        if (vlc != nullptr) {
            libvlc_release(vlc);
        }
    }
};

AudioPlayer::AudioPlayer() : impl_(new Impl()) {}

AudioPlayer::~AudioPlayer() { delete impl_; }

void AudioPlayer::playFile(const std::string& filePath) {
    libvlc_media_t* media = libvlc_media_new_path(impl_->vlc, filePath.c_str());
    if (media == nullptr) {
        throw std::runtime_error("Could not create libVLC media for: " + filePath);
    }

    libvlc_media_player_t* player = libvlc_media_player_new_from_media(media);
    libvlc_media_release(media);
    if (player == nullptr) {
        throw std::runtime_error("Could not create libVLC media player");
    }

    PlaybackState state;
    libvlc_event_manager_t* events = libvlc_media_player_event_manager(player);
    libvlc_event_attach(events, libvlc_MediaPlayerEndReached, handlePlaybackEvent, &state);
    libvlc_event_attach(events, libvlc_MediaPlayerStopped, handlePlaybackEvent, &state);
    libvlc_event_attach(events, libvlc_MediaPlayerEncounteredError, handlePlaybackEvent, &state);

    if (libvlc_media_player_play(player) != 0) {
        libvlc_media_player_release(player);
        throw std::runtime_error("libVLC failed to start playback for: " + filePath);
    }

    std::cout << "Playing: " << filePath << "\n";
    {
        std::unique_lock<std::mutex> lock(state.mutex);
        state.cv.wait(lock, [&] { return state.finished; });
    }

    libvlc_event_detach(events, libvlc_MediaPlayerEndReached, handlePlaybackEvent, &state);
    libvlc_event_detach(events, libvlc_MediaPlayerStopped, handlePlaybackEvent, &state);
    libvlc_event_detach(events, libvlc_MediaPlayerEncounteredError, handlePlaybackEvent, &state);
    libvlc_media_player_stop(player);
    libvlc_media_player_release(player);

    if (state.failed) {
        throw std::runtime_error("libVLC encountered a playback error for: " + filePath);
    }
}

#else

struct AudioPlayer::Impl {};

AudioPlayer::AudioPlayer() : impl_(new Impl()) {}

AudioPlayer::~AudioPlayer() { delete impl_; }

void AudioPlayer::playFile(const std::string&) {
    throw std::runtime_error("Audio playback is not available because this build was configured without libVLC");
}

#endif
