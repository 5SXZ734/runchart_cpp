#pragma once

#include <string>

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    void playFile(const std::string& filePath);

private:
    struct Impl;
    Impl* impl_;
};
