#include "main_window.h"

#include "audio/audio_engine.h"
#include "p2p/peer_discovery.h"
#include "ui/components/participant_row.h"
#include "ui/components/settings_dialog.h"
#include "ui/components/ui_helpers.h"
#include "volume_control_panel.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTime>
#include <QVBoxLayout>

#include <algorithm>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace {
constexpr const char* DEFAULT_ROOM_NAME = "main";

std::pair<int, QString> sortClientKey(const QString& client_id) {
    bool ok = false;
    int number = client_id.toInt(&ok);
    if (ok) {
        return {0, QString::number(number)};
    }
    return {1, client_id};
}

} // namespace

MainWindow::MainWindow(const QString& my_id,
                       AudioEngine* audio,
                       const QString& register_secret,
                       PeerDiscovery* discovery,
                       QWidget* parent)
    : QMainWindow(parent),
      my_id_(my_id),
      register_secret_(register_secret),
      audio_(audio),
      discovery_(discovery) {
    audio_->setClientId(my_id_.toStdString());

    root_ = load_ui_widget(ui_path("main_window.ui"), this);
    setCentralWidget(root_);
    setWindowTitle(root_->windowTitle());

    room_combo_ = require_child<QComboBox>(root_, "roomCombo");
    join_leave_button_ = require_child<QPushButton>(root_, "joinLeaveButton");
    refresh_button_ = require_child<QPushButton>(root_, "refreshButton");
    connection_indicator_ = require_child<QLabel>(root_, "connectionIndicator");

    search_input_ = require_child<QLineEdit>(root_, "searchInput");
    participant_list_ = require_child<QListWidget>(root_, "participantList");
    count_label_ = require_child<QLabel>(root_, "countLabel");

    active_speakers_label_ = require_child<QLabel>(root_, "activeSpeakersLabel");
    speaker_log_list_ = require_child<QListWidget>(root_, "speakerLogList");
    system_level_bar_ = require_child<QProgressBar>(root_, "systemLevelBar");

    controls_layout_ = root_->findChild<QVBoxLayout*>("controlsPlaceholderLayout");
    if (!controls_layout_) {
        QWidget* group = root_->findChild<QWidget*>("myControlsGroup");
        if (group) {
            controls_layout_ = qobject_cast<QVBoxLayout*>(group->layout());
        }
    }
    if (!controls_layout_) {
        throw std::runtime_error("Missing required controls layout");
    }
    controls_hint_ = root_->findChild<QLabel*>("controlsHint");

    mute_button_ = require_child<QPushButton>(root_, "muteButton");
    broadcast_button_ = require_child<QPushButton>(root_, "broadcastButton");
    settings_button_ = require_child<QPushButton>(root_, "settingsButton");

    warning_label_ = require_child<QLabel>(root_, "warningLabel");
    main_status_bar_ = require_child<QStatusBar>(root_, "mainStatusBar");

    volume_controls_ = new VolumeControlPanel(audio_, this);
    if (controls_hint_) {
        controls_hint_->setParent(nullptr);
    }
    controls_layout_->addWidget(volume_controls_->widget());

    room_combo_->clear();
    room_combo_->addItem(DEFAULT_ROOM_NAME);
    room_combo_->setCurrentText(DEFAULT_ROOM_NAME);
    room_combo_->setEnabled(false);

    join_leave_button_->setText("Leave Room");
    connect(join_leave_button_, &QPushButton::clicked, this, [this]() { cleanup(true); close(); });

    connect(refresh_button_, &QPushButton::clicked, this, [this]() { refreshParticipants(false); });
    connect(search_input_, &QLineEdit::textChanged, this, [this]() { applySearchFilter(); });

    mute_button_->setCheckable(true);
    connect(mute_button_, &QPushButton::toggled, this, &MainWindow::toggleSelfMute);

    broadcast_button_->setCheckable(true);
    broadcast_button_->setChecked(false);
    broadcast_button_->setText("Broadcast Off");
    connect(broadcast_button_, &QPushButton::toggled, this, &MainWindow::toggleBroadcast);

    connect(settings_button_, &QPushButton::clicked, this, [this]() {
        SettingsDialog dlg(audio_, "P2P Mesh", []() {
            return std::make_pair(true, QString());
        }, this);
        dlg.exec();
    });

    stop_capture_timer_.setSingleShot(true);
    stop_capture_timer_.setInterval(1200);
    connect(&stop_capture_timer_, &QTimer::timeout, this, &MainWindow::stopCaptureIfIdle);

    ui_timer_.setInterval(200);
    connect(&ui_timer_, &QTimer::timeout, this, &MainWindow::updateLiveUi);
    ui_timer_.start();

    auto_refresh_timer_.setInterval(1500);
    connect(&auto_refresh_timer_, &QTimer::timeout, this, [this]() { refreshParticipants(true); });
    auto_refresh_timer_.start();

    system_level_bar_->setMinimum(0);
    system_level_bar_->setMaximum(100);
    system_level_bar_->setValue(0);

    main_status_bar_->showMessage(QString("Client %1 in P2P mesh").arg(my_id_));
    setConnectedState(true);

    refreshParticipants(false);
}

MainWindow::~MainWindow() {
    cleanup(true);
}

void MainWindow::setConnectedState(bool connected, const QString& detail) {
    connected_ = connected;
    if (connected_) {
        connection_indicator_->setText("Connected");
        connection_indicator_->setStyleSheet("color:#1E8E3E; font-weight:bold;");
        if (!detail.isEmpty()) {
            main_status_bar_->showMessage(detail);
        }
        warning_label_->setText("");
    } else {
        connection_indicator_->setText("Disconnected");
        connection_indicator_->setStyleSheet("color:#C62828; font-weight:bold;");
        QString message = detail.isEmpty() ? "No peers reachable" : detail;
        warning_label_->setText(message);
        main_status_bar_->showMessage(message);
    }
}

void MainWindow::refreshParticipants(bool silent) {
    if (!discovery_) {
        return;
    }

    std::vector<PeerInfo> peers = discovery_->peers();
    std::vector<std::string> participants;
    participants.reserve(peers.size() + 1);
    for (const auto& p : peers) {
        participants.push_back(p.id);
    }
    bool has_self = false;
    for (const auto& p : participants) {
        if (p == my_id_.toStdString()) {
            has_self = true;
            break;
        }
    }
    if (!has_self) {
        participants.push_back(my_id_.toStdString());
    }

    std::sort(participants.begin(), participants.end(), [](const std::string& a, const std::string& b) {
        return sortClientKey(QString::fromStdString(a)) < sortClientKey(QString::fromStdString(b));
    });
    participants.erase(std::unique(participants.begin(), participants.end()), participants.end());

    if (!silent) {
        setConnectedState(true, "Participant list refreshed");
    } else if (!connected_) {
        setConnectedState(true, "Connection restored");
    }

    QSet<QString> participant_set;
    for (const auto& p : participants) {
        participant_set.insert(QString::fromStdString(p));
    }

    targets_ = targets_.intersect(participant_set);
    muted_participants_ = muted_participants_.intersect(participant_set);

    for (auto it = participant_rows_.begin(); it != participant_rows_.end(); ++it) {
        delete it.value();
    }
    participant_rows_.clear();
    participant_list_->clear();

    for (const auto& p : participants) {
        const QString cid = QString::fromStdString(p);
        bool is_self = (cid == my_id_);
        auto* row = new ParticipantRow(cid, is_self, targets_.contains(cid), muted_participants_.contains(cid), this);
        connect(row, &ParticipantRow::talkToggled, this, [this](const QString& client_id, bool enabled) {
            if (client_id == my_id_) {
                return;
            }
            if (enabled) {
                targets_.insert(client_id);
            } else {
                targets_.remove(client_id);
            }
            updateLocalTargets();
        });
        connect(row, &ParticipantRow::muteToggled, this, [this](const QString& client_id, bool enabled) {
            if (client_id == my_id_) {
                return;
            }
            if (enabled) {
                muted_participants_.insert(client_id);
            } else {
                muted_participants_.remove(client_id);
            }
            recomputeHearTargets();
            updateLocalTargets();
        });

        auto* item = new QListWidgetItem();
        item->setSizeHint(row->widget()->sizeHint());
        participant_list_->addItem(item);
        participant_list_->setItemWidget(item, row->widget());
        participant_rows_.insert(cid, row);
    }

    recomputeHearTargets();
    updateLocalTargets();
    applySearchFilter();
}

void MainWindow::applySearchFilter() {
    const QString query = search_input_->text().trimmed().toLower();
    int shown = 0;
    const int total = participant_list_->count();

    for (int i = 0; i < total; ++i) {
        auto* item = participant_list_->item(i);
        QWidget* widget = participant_list_->itemWidget(item);
        auto* name_label = widget ? widget->findChild<QLabel*>("participantName") : nullptr;
        const QString text = name_label ? name_label->text().toLower() : QString();
        const bool visible = query.isEmpty() ? true : text.contains(query);
        item->setHidden(!visible);
        if (visible) {
            ++shown;
        }
    }

    count_label_->setText(QString("%1 / %2 shown").arg(shown).arg(total));
}

void MainWindow::recomputeHearTargets() {
    hear_targets_.clear();
    for (auto it = participant_rows_.begin(); it != participant_rows_.end(); ++it) {
        const QString cid = it.key();
        if (cid != my_id_ && !muted_participants_.contains(cid)) {
            hear_targets_.insert(cid);
        }
    }
}

void MainWindow::updateLocalTargets() {
    // Update send destinations based on current talk targets
    std::vector<std::string> dest_ips;
    if (discovery_) {
        const auto peers = discovery_->peers();
        for (const auto& cid : targets_) {
            for (const auto& peer : peers) {
                if (peer.id == cid.toStdString()) {
                    std::string ip_port = peer.ip;
                    if (peer.port != 0) {
                        ip_port += ":" + std::to_string(peer.port);
                    }
                    dest_ips.push_back(std::move(ip_port));
                    break;
                }
            }
        }
    }

    if (!dest_ips.empty()) {
        if (stop_capture_timer_.isActive()) {
            stop_capture_timer_.stop();
        }
        if (!audio_->isRunning()) {
            audio_->start(dest_ips);
        } else {
            audio_->updateDestinations(dest_ips);
        }
    } else if (audio_->isRunning() && !stop_capture_timer_.isActive()) {
        stop_capture_timer_.start();
    }

    // Tell audio engine who we want to hear for mixing
    std::unordered_set<std::string> hear;
    for (const auto& cid : hear_targets_) {
        hear.insert(cid.toStdString());
    }
    audio_->setHearTargets(hear);

    syncBroadcastButton();
}

void MainWindow::stopCaptureIfIdle() {
    if (targets_.isEmpty() && audio_->isRunning()) {
        audio_->stop();
    }
}

void MainWindow::syncBroadcastButton() {
    QSet<QString> all_targets;
    for (auto it = participant_rows_.begin(); it != participant_rows_.end(); ++it) {
        const QString cid = it.key();
        if (cid != my_id_) {
            all_targets.insert(cid);
        }
    }
    const bool is_broadcast = !all_targets.isEmpty() && targets_ == all_targets;
    broadcast_button_->blockSignals(true);
    broadcast_button_->setChecked(is_broadcast);
    broadcast_button_->setText(is_broadcast ? "Broadcast On" : "Broadcast Off");
    broadcast_button_->blockSignals(false);
}

void MainWindow::toggleBroadcast(bool enabled) {
    QSet<QString> all_targets;
    for (auto it = participant_rows_.begin(); it != participant_rows_.end(); ++it) {
        const QString cid = it.key();
        if (cid != my_id_) {
            all_targets.insert(cid);
        }
    }
    if (enabled && all_targets.isEmpty()) {
        broadcast_button_->blockSignals(true);
        broadcast_button_->setChecked(false);
        broadcast_button_->setText("Broadcast Off");
        broadcast_button_->blockSignals(false);
        return;
    }

    targets_ = enabled ? all_targets : QSet<QString>();
    for (auto it = participant_rows_.begin(); it != participant_rows_.end(); ++it) {
        if (it.key() == my_id_) {
            continue;
        }
        it.value()->setTalkChecked(targets_.contains(it.key()));
    }
    updateLocalTargets();
}

void MainWindow::toggleSelfMute(bool muted) {
    audio_->setTxMuted(muted);
    if (muted) {
        mute_button_->setText("Unmute Mic");
        main_status_bar_->showMessage("Microphone muted");
    } else {
        mute_button_->setText("Mute Mic");
        main_status_bar_->showMessage("Microphone unmuted");
    }
    auto it = participant_rows_.find(my_id_);
    if (it != participant_rows_.end()) {
        it.value()->setMicStatus(!muted);
    }
}

void MainWindow::updateLiveUi() {
    const int mic_level = audio_->captureLevel();
    system_level_bar_->setValue(mic_level);
    volume_controls_->setMicLevel(mic_level);

    QMap<QString, bool> speaking_state;
    const bool self_speaking = audio_->captureActive() && !audio_->isTxMuted();
    auto it = participant_rows_.find(my_id_);
    if (it != participant_rows_.end()) {
        it.value()->setVolume(mic_level);
        it.value()->setMicStatus(!audio_->isTxMuted());
    }
    speaking_state.insert(my_id_, self_speaking);

    const bool prev_self_active = speaker_state_.value(my_id_, false);
    if (self_speaking && !prev_self_active) {
        const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
        speaker_log_list_->addItem(QString("[%1] Client %2 speaking").arg(timestamp, my_id_));
    } else if (!self_speaking && prev_self_active) {
        const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
        speaker_log_list_->addItem(QString("[%1] Client %2 stopped").arg(timestamp, my_id_));
    }
    speaker_state_.insert(my_id_, self_speaking);

    const float raw_level = audio_->mixedPeak();
    for (auto it2 = participant_rows_.begin(); it2 != participant_rows_.end(); ++it2) {
        const QString cid = it2.key();
        if (cid == my_id_) {
            continue;
        }
        const int level = std::min(100, static_cast<int>((raw_level * 100) / 32767));
        const bool is_active = level >= 2;
        it2.value()->setVolume(level);
        it2.value()->setMicStatus(is_active);

        const bool was_active = speaker_state_.value(cid, false);
        if (is_active && !was_active) {
            const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
            speaker_log_list_->addItem(QString("[%1] Client %2 speaking").arg(timestamp, cid));
        } else if (!is_active && was_active) {
            const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
            speaker_log_list_->addItem(QString("[%1] Client %2 stopped").arg(timestamp, cid));
        }
        while (speaker_log_list_->count() > 200) {
            delete speaker_log_list_->takeItem(0);
        }
        speaker_state_.insert(cid, is_active);
        speaking_state.insert(cid, is_active);
    }

    QStringList status_lines;
    for (auto it3 = participant_rows_.begin(); it3 != participant_rows_.end(); ++it3) {
        const QString cid = it3.key();
        const QString state = speaking_state.value(cid, false) ? "talking" : "listening";
        status_lines << QString("Client %1 - %2").arg(cid, state);
    }
    active_speakers_label_->setText(status_lines.isEmpty() ? "No clients" : status_lines.join("\n"));
}

void MainWindow::cleanup(bool unregister) {
    if (cleaned_up_) {
        return;
    }
    cleaned_up_ = true;

    if (ui_timer_.isActive()) {
        ui_timer_.stop();
    }
    if (auto_refresh_timer_.isActive()) {
        auto_refresh_timer_.stop();
    }
    if (stop_capture_timer_.isActive()) {
        stop_capture_timer_.stop();
    }

    audio_->shutdown();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    cleanup(true);
    event->accept();
}
