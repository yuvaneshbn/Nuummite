#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QTimer>

#include <atomic>
#include <mutex>
#include <string>

#include "p2p/peer_discovery.h"

class AudioEngine;
class ParticipantRow;
class VolumeControlPanel;
class QListWidget;
class QListWidgetItem;
class QLabel;
class QComboBox;
class QPushButton;
class QLineEdit;
class QProgressBar;
class QStatusBar;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const QString& my_id,
               AudioEngine* audio,
               const QString& register_secret,
               PeerDiscovery* discovery,
               QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setConnectedState(bool connected, const QString& detail = QString());
    void refreshParticipants(bool silent = false);
    void applySearchFilter();
    void updateLocalTargets();
    void recomputeHearTargets();
    void syncBroadcastButton();
    void stopCaptureIfIdle();
    void toggleBroadcast(bool enabled);
    void toggleSelfMute(bool muted);
    void updateLiveUi();
    void cleanup(bool unregister = true);

    QString my_id_;
    QString register_secret_;
    AudioEngine* audio_ = nullptr;
    PeerDiscovery* discovery_ = nullptr;

    QWidget* root_ = nullptr;
    QComboBox* room_combo_ = nullptr;
    QPushButton* join_leave_button_ = nullptr;
    QPushButton* refresh_button_ = nullptr;
    QLabel* connection_indicator_ = nullptr;

    QLineEdit* search_input_ = nullptr;
    QListWidget* participant_list_ = nullptr;
    QLabel* count_label_ = nullptr;

    QLabel* active_speakers_label_ = nullptr;
    QListWidget* speaker_log_list_ = nullptr;
    QProgressBar* system_level_bar_ = nullptr;

    QVBoxLayout* controls_layout_ = nullptr;
    QLabel* controls_hint_ = nullptr;

    QPushButton* mute_button_ = nullptr;
    QPushButton* broadcast_button_ = nullptr;
    QPushButton* settings_button_ = nullptr;

    QLabel* warning_label_ = nullptr;
    QStatusBar* main_status_bar_ = nullptr;

    VolumeControlPanel* volume_controls_ = nullptr;

    QSet<QString> targets_;
    QSet<QString> muted_participants_;
    QSet<QString> hear_targets_;
    QMap<QString, ParticipantRow*> participant_rows_;
    QMap<QString, bool> speaker_state_;

    bool connected_ = true;
    bool registration_successful_ = true;
    bool cleaned_up_ = false;

    QTimer stop_capture_timer_;
    QTimer ui_timer_;
    QTimer auto_refresh_timer_;
};

#endif
