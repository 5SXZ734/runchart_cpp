#include "audio_player.h"

#include <chrono>
#include <stdexcept>
#include <thread>

#ifdef RUNCHART_WITH_LIBVLC

#include <vlc/vlc.h>

struct LibVlcPlayer::Impl {
    libvlc_instance_t* vlc = nullptr;
    libvlc_media_player_t* player = nullptr;

    Impl() {
        const char* args[] = {"--no-video", "--quiet"};
        vlc = libvlc_new(static_cast<int>(sizeof(args) / sizeof(args[0])), args);
        if (vlc == nullptr) {
            throw std::runtime_error("Could not initialize libVLC");
        }
        player = libvlc_media_player_new(vlc);
        if (player == nullptr) {
            libvlc_release(vlc);
            vlc = nullptr;
            throw std::runtime_error("Could not create libVLC media player");
        }
    }

    ~Impl() {
        if (player != nullptr) {
            libvlc_media_player_stop(player);
            libvlc_media_player_release(player);
        }
        if (vlc != nullptr) {
            libvlc_release(vlc);
        }
    }
};

LibVlcPlayer::LibVlcPlayer() : impl_(new Impl()) {}

LibVlcPlayer::~LibVlcPlayer() { delete impl_; }

void LibVlcPlayer::playFile(const std::string& filePath) {
    libvlc_media_t* media = libvlc_media_new_path(impl_->vlc, filePath.c_str());
    if (media == nullptr) {
        throw std::runtime_error("Could not create libVLC media for: " + filePath);
    }

    libvlc_media_player_stop(impl_->player);
    libvlc_media_player_set_media(impl_->player, media);
    libvlc_media_release(media);

    if (libvlc_media_player_play(impl_->player) != 0) {
        throw std::runtime_error("libVLC failed to start playback for: " + filePath);
    }
}

void LibVlcPlayer::togglePause() {
    libvlc_media_player_pause(impl_->player);
}

void LibVlcPlayer::stop() {
    libvlc_media_player_stop(impl_->player);
}

void LibVlcPlayer::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    libvlc_audio_set_volume(impl_->player, volume);
}

void LibVlcPlayer::setPosition(float position) {
    if (position < 0.0F) position = 0.0F;
    if (position > 1.0F) position = 1.0F;
    libvlc_media_player_set_position(impl_->player, position);
}

bool LibVlcPlayer::isPlaying() const {
    return libvlc_media_player_is_playing(impl_->player) != 0;
}

int LibVlcPlayer::volume() const {
    return libvlc_audio_get_volume(impl_->player);
}

float LibVlcPlayer::position() const {
    return libvlc_media_player_get_position(impl_->player);
}

long long LibVlcPlayer::durationMs() const {
    return libvlc_media_player_get_length(impl_->player);
}

long long LibVlcPlayer::currentTimeMs() const {
    return libvlc_media_player_get_time(impl_->player);
}

#else

struct LibVlcPlayer::Impl {};

LibVlcPlayer::LibVlcPlayer() : impl_(new Impl()) {}

LibVlcPlayer::~LibVlcPlayer() { delete impl_; }

void LibVlcPlayer::playFile(const std::string&) {
    throw std::runtime_error("Audio playback is unavailable because this client was built without libVLC support");
}

void LibVlcPlayer::togglePause() {}

void LibVlcPlayer::stop() {}

void LibVlcPlayer::setVolume(int) {}

void LibVlcPlayer::setPosition(float) {}

bool LibVlcPlayer::isPlaying() const { return false; }

int LibVlcPlayer::volume() const { return 0; }

float LibVlcPlayer::position() const { return 0.0F; }

long long LibVlcPlayer::durationMs() const { return 0; }

long long LibVlcPlayer::currentTimeMs() const { return 0; }

#endif

AudioPlayer::AudioPlayer() = default;

AudioPlayer::~AudioPlayer() = default;

void AudioPlayer::playFile(const std::string& filePath) {
    player_.playFile(filePath);

    using namespace std::chrono_literals;
    for (int i = 0; i < 50 && !player_.isPlaying(); ++i) {
        std::this_thread::sleep_for(20ms);
    }
    while (player_.isPlaying()) {
        std::this_thread::sleep_for(200ms);
    }
}
