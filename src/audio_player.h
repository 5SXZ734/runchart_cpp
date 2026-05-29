#pragma once

#include <string>

class LibVlcPlayer {
public:
    LibVlcPlayer();
    ~LibVlcPlayer();

    LibVlcPlayer(const LibVlcPlayer&) = delete;
    LibVlcPlayer& operator=(const LibVlcPlayer&) = delete;

    void playFile(const std::string& filePath);
    void togglePause();
    void stop();
    void setVolume(int volume);
    void setPosition(float position);

    bool isPlaying() const;
    int volume() const;
    float position() const;
    long long durationMs() const;
    long long currentTimeMs() const;

private:
    struct Impl;
    Impl* impl_;
};

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    void playFile(const std::string& filePath);

private:
    LibVlcPlayer player_;
};
