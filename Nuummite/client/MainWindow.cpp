#include "MainWindow.h"

#include "ParticipantRowWidget.h"
#include "SettingsDialog.h"
#include "VolumeControlPanel.h"

#include "audio/audio_engine.h"
#include "p2p/peer_discovery.h"

#include <algorithm>
#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>
#include <vector>

#include "ui_main_window.h"

namespace {
bool isAllDigits(const std::string& s) {
    if (s.empty()) return false;
    for (const unsigned char ch : s) {
        if (ch < '0' || ch > '9') return false;
    }
    return true;
}
} // namespace

MainWindow::MainWindow(const QString& myId, const QString& roomName, AudioEngine* audio, PeerDiscovery* discovery, QWidget* parent)
    : QMainWindow(parent),
      myId_(myId),
      currentRoom_(roomName.trimmed().isEmpty() ? "main" : roomName.trimmed()),
      audio_(audio),
      discovery_(discovery) {
    if (audio_) audio_->setClientId(myId_.toStdString());

    root_ = new QWidget(this);
    Ui::MainWindowForm ui;
    ui.setupUi(root_);
    setCentralWidget(root_);
    setWindowTitle(root_->windowTitle());

    roomCombo_ = ui.roomCombo;
    joinLeaveButton_ = ui.joinLeaveButton;
    refreshButton_ = ui.refreshButton;
    connectionIndicator_ = ui.connectionIndicator;
    searchInput_ = ui.searchInput;
    participantList_ = ui.participantList;
    countLabel_ = ui.countLabel;
    activeSpeakersLabel_ = ui.activeSpeakersLabel;
    speakerLogList_ = ui.speakerLogList;
    systemLevelBar_ = ui.systemLevelBar;
    controlsLayout_ = ui.controlsPlaceholderLayout;
    controlsHint_ = ui.controlsHint;
    muteButton_ = ui.muteButton;
    broadcastButton_ = ui.broadcastButton;
    settingsButton_ = ui.settingsButton;
    warningLabel_ = ui.warningLabel;

    mainStatusBar_ = root_->findChild<QStatusBar*>("mainStatusBar");
    if (!mainStatusBar_) {
        mainStatusBar_ = statusBar();
    } else {
        setStatusBar(mainStatusBar_);
    }

    volumeControls_ = new VolumeControlPanel(audio_, this);
    if (controlsHint_) controlsHint_->setParent(nullptr);
    if (controlsLayout_) controlsLayout_->addWidget(volumeControls_);

    roomCombo_->clear();
    roomCombo_->addItem(currentRoom_);
    roomCombo_->setEnabled(false);

    systemLevelBar_->setRange(0, 100);
    systemLevelBar_->setValue(0);

    muteButton_->setCheckable(true);
    broadcastButton_->setCheckable(true);

    connect(joinLeaveButton_, &QPushButton::clicked, this, &MainWindow::close);
    connect(refreshButton_, &QPushButton::clicked, this, [this]() { refreshParticipants(false); });
    connect(searchInput_, &QLineEdit::textChanged, this, &MainWindow::applySearchFilter);
    connect(muteButton_, &QPushButton::toggled, this, &MainWindow::toggleSelfMute);
    connect(broadcastButton_, &QPushButton::toggled, this, &MainWindow::toggleBroadcast);
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettings);

    stopCaptureTimer_ = new QTimer(this);
    stopCaptureTimer_->setSingleShot(true);
    stopCaptureTimer_->setInterval(1200);
    connect(stopCaptureTimer_, &QTimer::timeout, this, &MainWindow::stopCaptureIfIdle);

    uiTimer_ = new QTimer(this);
    uiTimer_->setInterval(200);
    connect(uiTimer_, &QTimer::timeout, this, &MainWindow::updateLiveUI);
    uiTimer_->start();

    autoRefreshTimer_ = new QTimer(this);
    autoRefreshTimer_->setInterval(1500);
    connect(autoRefreshTimer_, &QTimer::timeout, this, &MainWindow::autoRefreshParticipants);
    autoRefreshTimer_->start();

    monotonic_.start();
    mainStatusBar_->showMessage(QString("Client %1 in room '%2' (P2P mesh)").arg(myId_).arg(currentRoom_));

    refreshParticipants(false);
    setConnectedState(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent* event) {
    if (uiTimer_) uiTimer_->stop();
    if (autoRefreshTimer_) autoRefreshTimer_->stop();
    if (stopCaptureTimer_) stopCaptureTimer_->stop();

    if (audio_) audio_->shutdown();
    if (discovery_) discovery_->stop();

    event->accept();
}

void MainWindow::openSettings() {
    SettingsDialog dlg(audio_, "P2P Mesh", this);
    dlg.exec();
}

void MainWindow::toggleBroadcast(bool enabled) {
    std::unordered_set<std::string> allTargets;
    for (const auto& it : rows_) {
        if (it.first != myId_.toStdString()) allTargets.insert(it.first);
    }

    if (enabled && allTargets.empty()) {
        broadcastButton_->blockSignals(true);
        broadcastButton_->setChecked(false);
        broadcastButton_->setText("Broadcast Off");
        broadcastButton_->blockSignals(false);
        return;
    }

    targets_ = enabled ? allTargets : std::unordered_set<std::string>{};
    for (auto& it : rows_) {
        if (it.first == myId_.toStdString()) continue;
        it.second->setTalkChecked(targets_.count(it.first) != 0);
    }
    updateLocalTargets();
}

void MainWindow::toggleSelfMute(bool muted) { setSelfMute(muted, "button"); }

void MainWindow::setSelfMute(bool muted, const char* source) {
    selfMuted_ = muted;
    if (audio_) audio_->setTxMuted(muted);

    muteButton_->blockSignals(std::string(source) == "checkbox");
    muteButton_->setChecked(muted);
    muteButton_->setText(muted ? "Unmute Mic" : "Mute Mic");
    muteButton_->blockSignals(false);

    auto it = rows_.find(myId_.toStdString());
    if (it != rows_.end()) {
        it->second->setMuteChecked(muted);
        it->second->setMicStatus(!muted);
    }

    mainStatusBar_->showMessage(muted ? "Microphone muted" : "Microphone unmuted");
}

void MainWindow::autoRefreshParticipants() { refreshParticipants(true); }

void MainWindow::setConnectedState(bool connected, const QString& detail) {
    connected_ = connected;
    if (!connectionIndicator_ || !warningLabel_ || !mainStatusBar_) return;

    if (connected) {
        connectionIndicator_->setText("Connected");
        connectionIndicator_->setStyleSheet("color:#1E8E3E; font-weight:bold;");
        if (!detail.isEmpty()) mainStatusBar_->showMessage(detail);
        warningLabel_->setText("");
    } else {
        connectionIndicator_->setText("Disconnected");
        connectionIndicator_->setStyleSheet("color:#C62828; font-weight:bold;");
        const QString msg = detail.isEmpty() ? "No peers reachable" : detail;
        warningLabel_->setText(msg);
        mainStatusBar_->showMessage(msg);
    }
}

void MainWindow::refreshParticipants(bool silent) {
    if (!discovery_) return;

    std::vector<PeerInfo> peers = discovery_->peers();
    std::vector<std::string> participants;
    participants.reserve(peers.size() + 1);
    for (const auto& p : peers) participants.push_back(p.id);
    const std::string myIdStd = myId_.toStdString();
    if (std::find(participants.begin(), participants.end(), myIdStd) == participants.end()) participants.push_back(myIdStd);

    std::sort(participants.begin(), participants.end(), [](const std::string& a, const std::string& b) {
        const bool ad = isAllDigits(a);
        const bool bd = isAllDigits(b);
        if (ad != bd) return ad > bd;
        return a < b;
    });
    participants.erase(std::unique(participants.begin(), participants.end()), participants.end());

    if (!silent) {
        setConnectedState(true, "Participant list refreshed");
    } else if (!connected_) {
        setConnectedState(true, "Connection restored");
    }

    {
        std::unordered_set<std::string> set(participants.begin(), participants.end());
        for (auto it = targets_.begin(); it != targets_.end();) {
            if (set.count(*it) == 0) it = targets_.erase(it);
            else ++it;
        }
        for (auto it = muted_.begin(); it != muted_.end();) {
            if (set.count(*it) == 0) it = muted_.erase(it);
            else ++it;
        }
    }

    rows_.clear();
    participantList_->clear();

    for (const auto& cid : participants) {
        const bool isSelf = cid == myIdStd;
        const bool talkChecked = !isSelf && (targets_.count(cid) != 0);
        const bool muteChecked = isSelf ? selfMuted_ : (muted_.count(cid) != 0);

        auto* row = new ParticipantRowWidget(QString::fromStdString(cid), isSelf, talkChecked, muteChecked, participantList_);
        connect(row, &ParticipantRowWidget::talkToggled, this, &MainWindow::onTalkToggled);
        connect(row, &ParticipantRowWidget::muteToggled, this, &MainWindow::onMuteToggled);

        auto* item = new QListWidgetItem;
        item->setSizeHint(row->sizeHint());
        participantList_->addItem(item);
        participantList_->setItemWidget(item, row);
        rows_[cid] = row;
    }

    recomputeHearTargets();
    updateLocalTargets();
    applySearchFilter();
}

void MainWindow::onTalkToggled(const QString& clientId, bool enabled) {
    const std::string cid = clientId.toStdString();
    if (cid == myId_.toStdString()) return;

    if (enabled) targets_.insert(cid);
    else targets_.erase(cid);
    updateLocalTargets();
}

void MainWindow::onMuteToggled(const QString& clientId, bool enabled) {
    const std::string cid = clientId.toStdString();
    if (cid == myId_.toStdString()) {
        setSelfMute(enabled, "checkbox");
        return;
    }

    if (enabled) muted_.insert(cid);
    else muted_.erase(cid);
    recomputeHearTargets();
    updateLocalTargets();
}

void MainWindow::applySearchFilter() {
    const QString query = searchInput_->text().trimmed().toLower();
    int shown = 0;
    const int total = participantList_->count();
    for (int i = 0; i < total; ++i) {
        auto* item = participantList_->item(i);
        QWidget* widget = participantList_->itemWidget(item);
        QLabel* nameLabel = widget ? widget->findChild<QLabel*>("participantName") : nullptr;
        const QString text = nameLabel ? nameLabel->text().toLower() : QString();
        const bool visible = query.isEmpty() || text.contains(query);
        item->setHidden(!visible);
        if (visible) ++shown;
    }
    if (countLabel_) countLabel_->setText(QString("%1 / %2 shown").arg(shown).arg(total));
}

void MainWindow::recomputeHearTargets() {
    hearTargets_.clear();
    const std::string myIdStd = myId_.toStdString();
    for (const auto& it : rows_) {
        if (it.first == myIdStd) continue;
        if (muted_.count(it.first) != 0) continue;
        hearTargets_.insert(it.first);
    }
}

void MainWindow::updateLocalTargets() {
    if (!audio_ || !discovery_) return;

    std::vector<PeerInfo> peers = discovery_->peers();
    std::unordered_map<std::string, PeerInfo> peerById;
    peerById.reserve(peers.size());
    for (const auto& p : peers) peerById[p.id] = p;

    std::vector<std::string> destIps;
    for (const auto& cid : targets_) {
        const auto it = peerById.find(cid);
        if (it == peerById.end()) continue;
        const PeerInfo& peer = it->second;
        std::string ip = peer.ip;
        // If on same host, use loopback (127.0.0.1)
        // (PeerDiscovery already sets ip="127.0.0.1" for local peers by default,
        //  but we double-check for safety.)
        if (peer.is_local || ip.rfind("127.", 0) == 0) {
            ip = "127.0.0.1";
        }
        destIps.push_back(ip + ":" + std::to_string(peer.port));
    }

    if (!destIps.empty()) {
        if (stopCaptureTimer_->isActive()) stopCaptureTimer_->stop();
        if (!audio_->isRunning()) audio_->start(destIps);
        else audio_->updateDestinations(destIps);
    } else if (audio_->isRunning() && !stopCaptureTimer_->isActive()) {
        stopCaptureTimer_->start();
    }

    audio_->setHearTargets(hearTargets_);
    syncBroadcastButton();
}

void MainWindow::syncBroadcastButton() {
    std::unordered_set<std::string> allTargets;
    const std::string myIdStd = myId_.toStdString();
    for (const auto& it : rows_) {
        if (it.first == myIdStd) continue;
        allTargets.insert(it.first);
    }

    const bool isBroadcast = !allTargets.empty() && targets_ == allTargets;
    broadcastButton_->blockSignals(true);
    broadcastButton_->setChecked(isBroadcast);
    broadcastButton_->setText(isBroadcast ? "Broadcast On" : "Broadcast Off");
    broadcastButton_->blockSignals(false);
}

void MainWindow::stopCaptureIfIdle() {
    if (targets_.empty() && audio_ && audio_->isRunning()) audio_->stop();
}

void MainWindow::updateLiveUI() {
    if (!audio_) return;

    const int micLevel = audio_->captureLevel();
    systemLevelBar_->setValue(micLevel);
    if (volumeControls_) volumeControls_->setMicLevel(micLevel);

    std::unordered_map<std::string, bool> speakingState;

    const std::string myIdStd = myId_.toStdString();
    const bool selfState = audio_->captureActive() && !audio_->isTxMuted();
    if (auto it = rows_.find(myIdStd); it != rows_.end()) {
        it->second->setVolume(micLevel);
        it->second->setMicStatus(!audio_->isTxMuted());
    }
    speakingState[myIdStd] = selfState;
    lastVoiceMs_[myIdStd] = monotonic_.elapsed();

    const float rawPeak = audio_->mixedPeak();
    const int level = rawPeak > 0.0f ? std::min(100, static_cast<int>((rawPeak * 100.0f) / 32767.0f)) : 0;
    const qint64 now = monotonic_.elapsed();

    for (auto& it : rows_) {
        const std::string& cid = it.first;
        if (cid == myIdStd) continue;

        const bool activeInstant = level >= 2;
        if (activeInstant) lastVoiceMs_[cid] = now;
        const qint64 last = lastVoiceMs_.count(cid) ? lastVoiceMs_[cid] : 0;
        const bool isActive = (now - last) < 800;

        it.second->setVolume(level);
        it.second->setMicStatus(isActive);

        const bool prev = speakerState_.count(cid) ? speakerState_[cid] : false;
        if (isActive != prev && speakerLogList_) {
            const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
            const QString msg = QString("[%1] Client %2 %3").arg(timestamp, QString::fromStdString(cid), isActive ? "speaking" : "stopped");
            speakerLogList_->addItem(msg);
        }
        speakerState_[cid] = isActive;
        speakingState[cid] = isActive;
    }

    QStringList lines;
    for (const auto& it : speakingState) {
        lines << QString("Client %1 - %2").arg(QString::fromStdString(it.first), it.second ? "talking" : "listening");
    }
    if (activeSpeakersLabel_) activeSpeakersLabel_->setText(lines.isEmpty() ? "No clients" : lines.join("\n"));
}
