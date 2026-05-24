#ifndef NUUMMITE_MAINWINDOW_H
#define NUUMMITE_MAINWINDOW_H

#include <QElapsedTimer>
#include <QMainWindow>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

class AudioEngine;
class PeerDiscovery;
class ParticipantRowWidget;
class VolumeControlPanel;

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStatusBar;
class QTimer;
class QVBoxLayout;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const QString& myId, const QString& roomName, AudioEngine* audio, PeerDiscovery* discovery, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void openSettings();
    void toggleBroadcast(bool enabled);
    void toggleSelfMute(bool muted);
    void updateLiveUI();
    void stopCaptureIfIdle();
    void autoRefreshParticipants();

private:
    void setConnectedState(bool connected, const QString& detail = QString());
    void refreshParticipants(bool silent);
    void applySearchFilter();
    void recomputeHearTargets();
    void updateLocalTargets();
    void syncBroadcastButton();
    void setSelfMute(bool muted, const char* source);

    void onTalkToggled(const QString& clientId, bool enabled);
    void onMuteToggled(const QString& clientId, bool enabled);

    QString myId_;
    QString currentRoom_;
    AudioEngine* audio_ = nullptr;
    PeerDiscovery* discovery_ = nullptr;

    QWidget* root_ = nullptr;
    QComboBox* roomCombo_ = nullptr;
    QPushButton* joinLeaveButton_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QLabel* connectionIndicator_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QListWidget* participantList_ = nullptr;
    QLabel* countLabel_ = nullptr;
    QLabel* activeSpeakersLabel_ = nullptr;
    QListWidget* speakerLogList_ = nullptr;
    QProgressBar* systemLevelBar_ = nullptr;
    QVBoxLayout* controlsLayout_ = nullptr;
    QLabel* controlsHint_ = nullptr;
    QPushButton* muteButton_ = nullptr;
    QPushButton* broadcastButton_ = nullptr;
    QPushButton* settingsButton_ = nullptr;
    QLabel* warningLabel_ = nullptr;
    QStatusBar* mainStatusBar_ = nullptr;

    VolumeControlPanel* volumeControls_ = nullptr;

    QTimer* uiTimer_ = nullptr;
    QTimer* autoRefreshTimer_ = nullptr;
    QTimer* stopCaptureTimer_ = nullptr;

    bool connected_ = true;
    bool selfMuted_ = false;

    std::unordered_set<std::string> targets_;
    std::unordered_set<std::string> muted_;
    std::unordered_set<std::string> hearTargets_;
    std::unordered_map<std::string, ParticipantRowWidget*> rows_;
    std::unordered_map<std::string, bool> speakerState_;
    std::unordered_map<std::string, qint64> lastVoiceMs_;
    QElapsedTimer monotonic_;
};

#endif

