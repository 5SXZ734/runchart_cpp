#include "playback_controller.h"
#include "run_chart_client.h"

#include <grpcpp/grpcpp.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
QString toQString(const std::string& value) {
    return QString::fromUtf8(value.c_str());
}

QString formatTime(long long ms) {
    if (ms <= 0) return QStringLiteral("--:--");
    const long long totalSeconds = ms / 1000;
    return QStringLiteral("%1:%2")
        .arg(totalSeconds / 60)
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(std::shared_ptr<RunChartClient> client, QWidget* parent = nullptr)
        : QMainWindow(parent), client_(std::move(client)) {
        setWindowTitle(QStringLiteral("RunChart Player"));
        resize(1100, 700);
        buildUi();
        connectUi();
        loadArtists();
    }

private:
    void buildUi() {
        auto* central = new QWidget(this);
        auto* root = new QVBoxLayout(central);

        auto* browser = new QSplitter(Qt::Horizontal, central);
        artistsList_ = new QListWidget(browser);
        albumsList_ = new QListWidget(browser);
        tracksTable_ = new QTableWidget(browser);

        artistsList_->setObjectName(QStringLiteral("artistsList"));
        albumsList_->setObjectName(QStringLiteral("albumsList"));
        tracksTable_->setObjectName(QStringLiteral("tracksTable"));

        artistsList_->setSelectionMode(QAbstractItemView::SingleSelection);
        albumsList_->setSelectionMode(QAbstractItemView::SingleSelection);
        tracksTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
        tracksTable_->setSelectionMode(QAbstractItemView::SingleSelection);
        tracksTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tracksTable_->setColumnCount(3);
        tracksTable_->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("Title"), QStringLiteral("File")});
        tracksTable_->horizontalHeader()->setStretchLastSection(true);
        tracksTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tracksTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        tracksTable_->verticalHeader()->setVisible(false);

        browser->addWidget(wrapWithTitle(QStringLiteral("Artists"), artistsList_));
        browser->addWidget(wrapWithTitle(QStringLiteral("Albums"), albumsList_));
        browser->addWidget(wrapWithTitle(QStringLiteral("Tracks"), tracksTable_));
        browser->setStretchFactor(0, 1);
        browser->setStretchFactor(1, 1);
        browser->setStretchFactor(2, 2);

        root->addWidget(browser, 1);
        root->addWidget(buildPlaybackPanel());
        setCentralWidget(central);
        statusBar()->showMessage(QStringLiteral("Ready"));
    }

    QWidget* wrapWithTitle(const QString& title, QWidget* child) {
        auto* panel = new QWidget(this);
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(6, 6, 6, 6);
        auto* label = new QLabel(title, panel);
        QFont font = label->font();
        font.setBold(true);
        label->setFont(font);
        layout->addWidget(label);
        layout->addWidget(child, 1);
        return panel;
    }

    QWidget* buildPlaybackPanel() {
        auto* group = new QGroupBox(QStringLiteral("Now Playing"), this);
        auto* layout = new QVBoxLayout(group);

        titleLabel_ = new QLabel(QStringLiteral("No track selected"), group);
        artistAlbumLabel_ = new QLabel(QStringLiteral("Select an artist, album, then double-click a track."), group);
        layout->addWidget(titleLabel_);
        layout->addWidget(artistAlbumLabel_);

        auto* controls = new QHBoxLayout();
        playPauseButton_ = new QPushButton(QStringLiteral("Play/Pause"), group);
        stopButton_ = new QPushButton(QStringLiteral("Stop"), group);
        nextButton_ = new QPushButton(QStringLiteral("Next"), group);
        positionSlider_ = new QSlider(Qt::Horizontal, group);
        positionSlider_->setRange(0, 100);
        timeLabel_ = new QLabel(QStringLiteral("--:-- / --:--"), group);
        volumeSlider_ = new QSlider(Qt::Horizontal, group);
        volumeSlider_->setRange(0, 100);
        volumeSlider_->setValue(80);
        volumeSlider_->setMaximumWidth(140);

        controls->addWidget(playPauseButton_);
        controls->addWidget(stopButton_);
        controls->addWidget(nextButton_);
        controls->addWidget(positionSlider_, 1);
        controls->addWidget(timeLabel_);
        controls->addWidget(new QLabel(QStringLiteral("Volume"), group));
        controls->addWidget(volumeSlider_);
        layout->addLayout(controls);

        return group;
    }

    void connectUi() {
        connect(artistsList_, &QListWidget::currentRowChanged, this, [this](int row) { loadAlbumsForArtist(row); });
        connect(albumsList_, &QListWidget::currentRowChanged, this, [this](int row) { loadTracksForAlbum(row); });
        connect(tracksTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { playTrackAtRow(row); });
        connect(playPauseButton_, &QPushButton::clicked, this, [this] { runPlaybackAction([this] { playback_.playOrPause(); }); });
        connect(stopButton_, &QPushButton::clicked, this, [this] { runPlaybackAction([this] { playback_.stop(); }); });
        connect(nextButton_, &QPushButton::clicked, this, [this] { runPlaybackAction([this] { playback_.next(); }); });
        connect(volumeSlider_, &QSlider::valueChanged, this, [this](int value) { playback_.setVolume(value); });
        connect(positionSlider_, &QSlider::sliderReleased, this, [this] { playback_.setPositionPercent(positionSlider_->value()); });

        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, [this] { refreshPlaybackUi(); });
        timer_->start(500);
    }

    void loadArtists() {
        try {
            artists_ = client_->listArtistsData();
            artistsList_->clear();
            for (const auto& artist : artists_) artistsList_->addItem(toQString(artist.name));
            statusBar()->showMessage(QStringLiteral("Loaded %1 artists").arg(static_cast<int>(artists_.size())));
            if (!artists_.empty()) artistsList_->setCurrentRow(0);
        } catch (const std::exception& ex) {
            showError(QStringLiteral("Could not load artists"), ex.what());
        }
    }

    void loadAlbumsForArtist(int row) {
        albums_.clear();
        tracks_.clear();
        albumsList_->clear();
        tracksTable_->setRowCount(0);
        if (row < 0 || row >= static_cast<int>(artists_.size())) return;

        const auto& artist = artists_[static_cast<std::size_t>(row)];
        try {
            const auto albums = client_->listAlbumsData();
            std::copy_if(albums.begin(), albums.end(), std::back_inserter(albums_), [&artist](const ClientAlbum& album) {
                return album.artistName == artist.name;
            });
            for (const auto& album : albums_) albumsList_->addItem(toQString(album.title));
            statusBar()->showMessage(QStringLiteral("Loaded %1 albums for %2").arg(static_cast<int>(albums_.size())).arg(toQString(artist.name)));
            if (!albums_.empty()) albumsList_->setCurrentRow(0);
        } catch (const std::exception& ex) {
            showError(QStringLiteral("Could not load albums"), ex.what());
        }
    }

    void loadTracksForAlbum(int row) {
        tracks_.clear();
        tracksTable_->setRowCount(0);
        if (row < 0 || row >= static_cast<int>(albums_.size())) return;

        const auto& album = albums_[static_cast<std::size_t>(row)];
        try {
            const auto tracks = client_->listTracksData();
            std::copy_if(tracks.begin(), tracks.end(), std::back_inserter(tracks_), [&album](const ClientTrack& track) {
                return track.artistName == album.artistName && track.albumTitle == album.title;
            });
            std::sort(tracks_.begin(), tracks_.end(), [](const ClientTrack& lhs, const ClientTrack& rhs) {
                if (lhs.trackNumber != rhs.trackNumber) return lhs.trackNumber < rhs.trackNumber;
                return lhs.title < rhs.title;
            });
            playback_.setQueue(tracks_);
            populateTracksTable();
            statusBar()->showMessage(QStringLiteral("Loaded %1 tracks from %2").arg(static_cast<int>(tracks_.size())).arg(toQString(album.title)));
        } catch (const std::exception& ex) {
            showError(QStringLiteral("Could not load tracks"), ex.what());
        }
    }

    void populateTracksTable() {
        tracksTable_->setRowCount(static_cast<int>(tracks_.size()));
        for (int row = 0; row < static_cast<int>(tracks_.size()); ++row) {
            const auto& track = tracks_[static_cast<std::size_t>(row)];
            auto* number = new QTableWidgetItem(QString::number(track.trackNumber));
            number->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(track.id));
            tracksTable_->setItem(row, 0, number);
            tracksTable_->setItem(row, 1, new QTableWidgetItem(toQString(track.title)));
            tracksTable_->setItem(row, 2, new QTableWidgetItem(toQString(track.filePath)));
        }
    }

    void playTrackAtRow(int row) {
        if (row < 0 || row >= static_cast<int>(tracks_.size())) return;
        const auto track = tracks_[static_cast<std::size_t>(row)];
        runPlaybackAction([this, track] { playback_.playTrack(track); });
    }

    template <typename Func>
    void runPlaybackAction(Func action) {
        try {
            action();
            refreshPlaybackUi();
        } catch (const std::exception& ex) {
            showError(QStringLiteral("Playback error"), ex.what());
        }
    }

    void refreshPlaybackUi() {
        const ClientTrack* track = playback_.currentTrack();
        if (track == nullptr) {
            titleLabel_->setText(QStringLiteral("No track selected"));
            artistAlbumLabel_->setText(QStringLiteral("Select an artist, album, then double-click a track."));
        } else {
            titleLabel_->setText(toQString(track->title));
            artistAlbumLabel_->setText(QStringLiteral("%1 — %2").arg(toQString(track->artistName), toQString(track->albumTitle)));
        }

        if (!positionSlider_->isSliderDown()) positionSlider_->setValue(playback_.positionPercent());
        timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(playback_.currentTimeMs()), formatTime(playback_.durationMs())));
    }

    void showError(const QString& title, const std::string& message) {
        QMessageBox::critical(this, title, toQString(message));
        statusBar()->showMessage(title);
    }

    std::shared_ptr<RunChartClient> client_;
    PlaybackController playback_;
    std::vector<ClientArtist> artists_;
    std::vector<ClientAlbum> albums_;
    std::vector<ClientTrack> tracks_;

    QListWidget* artistsList_ = nullptr;
    QListWidget* albumsList_ = nullptr;
    QTableWidget* tracksTable_ = nullptr;
    QLabel* titleLabel_ = nullptr;
    QLabel* artistAlbumLabel_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QPushButton* playPauseButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QSlider* positionSlider_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QTimer* timer_ = nullptr;
};
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    if (argc != 2) {
        QMessageBox::critical(nullptr, QStringLiteral("RunChart Player"), QStringLiteral("Usage: runchart_desktop <server>"));
        return 1;
    }

    try {
        auto channel = grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials());
        auto client = std::make_shared<RunChartClient>(channel);
        MainWindow window(client);
        window.show();
        return app.exec();
    } catch (const std::exception& ex) {
        QMessageBox::critical(nullptr, QStringLiteral("RunChart Player"), toQString(ex.what()));
        return 1;
    }
}
