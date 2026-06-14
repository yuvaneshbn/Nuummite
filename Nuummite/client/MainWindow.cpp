#include "MainWindow.h"
#include "ParticipantRowWidget.h"
#include "SettingsDialog.h"
#include "VolumeControlPanel.h"
#include "EncryptionDialog.h"
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
#include <QMessageBox>
#include <QSettings>
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

QStringList liveRoomListFromPeers(const std::vector<PeerInfo>& peers, const QString& currentRoom) {
    std::unordered_set<std::string> rooms;
    rooms.insert(currentRoom.toStdString());
    for (const auto& peer : peers) {
        if (!peer.room.empty()) rooms.insert(peer.room);
    }

    std::vector<std::string> sortedRooms(rooms.begin(), rooms.end());
    std::sort(sortedRooms.begin(), sortedRooms.end());

    QStringList out;
    out.reserve(static_cast<int>(sortedRooms.size()));
    for (const auto& room : sortedRooms) {
        out.append(QString::fromStdString(room));
    }
    return out;
}
} // namespace

MainWindow::MainWindow(const QString& myId, const QString& roomName, AudioEngine* audio, PeerDiscovery* discovery, QWidget* parent)
    : QMainWindow(parent),
      myId_(myId),
      currentRoom_(roomName.trimmed().isEmpty()? "main" : roomName.trimmed()),
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

    themeSwitch_ = ui.themeSwitch;

    mainStatusBar_ = root_->findChild<QStatusBar*>("mainStatusBar");

    if (!mainStatusBar_) {
        mainStatusBar_ = statusBar();
    } else {
        setStatusBar(mainStatusBar_);
    }

    volumeControls_ = new VolumeControlPanel(audio_, this);
    if (controlsHint_) controlsHint_->setParent(nullptr);
    if (controlsLayout_) controlsLayout_->addWidget(volumeControls_);

    // Populate the room dropdown from live peer observations.
    roomCombo_->clear();
    if (discovery_) {
        roomCombo_->addItems(liveRoomListFromPeers(discovery_->peers(), currentRoom_));
    } else {
        roomCombo_->addItem(currentRoom_);
    }
    roomCombo_->setCurrentText(currentRoom_);
    roomCombo_->setEditable(true);
    roomCombo_->setEnabled(true);

    systemLevelBar_->setRange(0, 100);
    systemLevelBar_->setValue(0);

    muteButton_->setCheckable(true);
    broadcastButton_->setCheckable(true);

    connect(joinLeaveButton_, &QPushButton::clicked, this, &MainWindow::close);
    
    // Refresh slot triggers instant discovery broadcast
    connect(refreshButton_, &QPushButton::clicked, this, [this]() {
        if (discovery_) {
            discovery_->forceAnnounce();
        }
        refreshParticipants(false);
    });
    
    connect(searchInput_, &QLineEdit::textChanged, this, &MainWindow::applySearchFilter);
    connect(muteButton_, &QPushButton::toggled, this, &MainWindow::toggleSelfMute);
    connect(broadcastButton_, &QPushButton::toggled, this, &MainWindow::toggleBroadcast);
    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettings);
    
    // Binding room change triggers
    connect(roomCombo_, &QComboBox::textActivated, this, &MainWindow::onRoomChangeRequested);
    if (roomCombo_->lineEdit()) {
        connect(roomCombo_->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
            onRoomChangeRequested(roomCombo_->currentText());
        });
    }

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

    // Load user's saved preference, defaulting to dark mode
    bool startInDarkMode = QSettings().value("ui/darkMode", true).toBool();

    if (themeSwitch_) {
        themeSwitch_->blockSignals(true);
        themeSwitch_->setChecked(startInDarkMode);
        themeSwitch_->blockSignals(false);
    }

    // Apply the loaded theme state
    applyTheme(startInDarkMode);

    if (themeSwitch_) {
        connect(themeSwitch_, &QCheckBox::toggled, this, &MainWindow::onThemeToggled);
    }

    mainStatusBar_->showMessage(QString("Client %1 in room '%2' (P2P mesh)").arg(myId_).arg(currentRoom_));

    refreshParticipants(false);
    setConnectedState(true);
}


MainWindow::~MainWindow() = default;

void MainWindow::openSettings() {
    SettingsDialog dlg(audio_, currentRoom_, this);
    dlg.exec();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (uiTimer_) uiTimer_->stop();
    if (autoRefreshTimer_) autoRefreshTimer_->stop();
    if (stopCaptureTimer_) stopCaptureTimer_->stop();
    if (discovery_) discovery_->stop();
    if (audio_) audio_->stop();
    QMainWindow::closeEvent(event);
}

void MainWindow::onRoomChangeRequested(const QString& newRoom) {
    QString trimmed = newRoom.trimmed();
    if (trimmed.isEmpty() || trimmed == currentRoom_) {
        roomCombo_->blockSignals(true);
        roomCombo_->setCurrentText(currentRoom_);
        roomCombo_->blockSignals(false);
        return;
    }

    // Step 1: Confirmation Prompt
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Change Room Confirmation",
        QString("Are you sure want to change room?"),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply!= QMessageBox::Yes) {
        roomCombo_->blockSignals(true);
        roomCombo_->setCurrentText(currentRoom_);
        roomCombo_->blockSignals(false);
        return;
    }

    // Step 2: Passphrase Request via ui layout
    EncryptionDialog keyDlg(this);
    if (keyDlg.exec()!= QDialog::Accepted) {
        roomCombo_->blockSignals(true);
        roomCombo_->setCurrentText(currentRoom_);
        roomCombo_->blockSignals(false);
        return;
    }

    QString passphrase = keyDlg.passphrase();
    if (passphrase.isEmpty()) {
        QMessageBox::warning(this, "Verification Failure", "Encryption key cannot be empty!");
        roomCombo_->blockSignals(true);
        roomCombo_->setCurrentText(currentRoom_);
        roomCombo_->blockSignals(false);
        return;
    }

    // Changing parameters inside underlying layers
    currentRoom_ = trimmed;

    refreshParticipants(true);
    roomCombo_->blockSignals(true);
    roomCombo_->setCurrentText(currentRoom_);
    roomCombo_->blockSignals(false);

    if (audio_) {
        audio_->stop();
        audio_->setRoomSecret(passphrase.toStdString());
    }

    if (discovery_) {
        discovery_->stop();
        discovery_->start(myId_.toStdString(), static_cast<uint16_t>(audio_->port()), currentRoom_.toStdString());
    }

    participantList_->clear();
    rows_.clear();
    targets_.clear();
    muted_.clear();
    hearTargets_.clear();

    mainStatusBar_->showMessage(QString("Migrated to room '%1'").arg(currentRoom_));
    refreshParticipants(false);
}

void MainWindow::toggleBroadcast(bool enabled) {
    std::unordered_set<std::string> allTargets;
    for (const auto& it : rows_) {
        if (it.first!= myId_.toStdString()) allTargets.insert(it.first);
    }

    if (enabled && allTargets.empty()) {
        broadcastButton_->blockSignals(true);
        broadcastButton_->setChecked(false);
        broadcastButton_->setText("Broadcast Off");
        broadcastButton_->blockSignals(false);
        return;
    }

    targets_ = enabled? allTargets : std::unordered_set<std::string>{};
    for (auto& it : rows_) {
        if (it.first == myId_.toStdString()) continue;
        it.second->setTalkChecked(targets_.count(it.first)!= 0);
    }
    updateLocalTargets();
}

void MainWindow::toggleSelfMute(bool muted) {
    setSelfMute(muted, "button");
}

void MainWindow::setSelfMute(bool muted, const char* source) {
    selfMuted_ = muted;
    if (audio_) audio_->setTxMuted(muted);

    muteButton_->blockSignals(std::string(source) == "checkbox");
    muteButton_->setChecked(muted);
    muteButton_->setText(muted? "Unmute Mic" : "Mute Mic");
    muteButton_->blockSignals(false);

    auto it = rows_.find(myId_.toStdString());
    if (it!= rows_.end()) {
        it->second->setMuteChecked(muted);
        it->second->setMicStatus(!muted);
    }

    mainStatusBar_->showMessage(muted? "Microphone muted" : "Microphone unmuted");
}

void MainWindow::autoRefreshParticipants() {
    refreshParticipants(true);
}

void MainWindow::setConnectedState(bool connected, const QString& detail) {
    connected_ = connected;
    if (!connectionIndicator_ ||!warningLabel_ ||!mainStatusBar_) return;
    if (connected) {
        connectionIndicator_->setText("Connected");
        connectionIndicator_->setStyleSheet("color:#1E8E3E; font-weight:bold;");
        if (!detail.isEmpty()) mainStatusBar_->showMessage(detail);
        warningLabel_->setText("");
    } else {
        connectionIndicator_->setText("Disconnected");
        connectionIndicator_->setStyleSheet("color:#C62828; font-weight:bold;");
        const QString msg = detail.isEmpty()? "No peers reachable" : detail;
        warningLabel_->setText(msg);
        mainStatusBar_->showMessage(msg);
    }
}

void MainWindow::refreshParticipants(bool silent) {
    if (!discovery_) return;
    std::vector<PeerInfo> peers = discovery_->peers();
    const bool roomSignalsBlocked = roomCombo_->blockSignals(true);
    roomCombo_->clear();
    roomCombo_->addItems(liveRoomListFromPeers(peers, currentRoom_));
    roomCombo_->setCurrentText(currentRoom_);
    roomCombo_->blockSignals(roomSignalsBlocked);

    std::vector<std::string> participants;
    participants.reserve(peers.size() + 1);
    for (const auto& p : peers) participants.push_back(p.id);
    const std::string myIdStd = myId_.toStdString();
    if (std::find(participants.begin(), participants.end(), myIdStd) == participants.end()) participants.push_back(myIdStd);
    std::sort(participants.begin(), participants.end(), [](const std::string& a, const std::string& b) {
        const bool ad = isAllDigits(a);
        const bool bd = isAllDigits(b);
        if (ad!= bd) return ad > bd;
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
        for (auto it = targets_.begin(); it!= targets_.end();) {
            if (set.count(*it) == 0) it = targets_.erase(it);
            else ++it;
        }
        for (auto it = muted_.begin(); it!= muted_.end();) {
            if (set.count(*it) == 0) it = muted_.erase(it);
            else ++it;
        }
    }

    for (auto& [cid, row] : rows_) {
        if (row) {
            QObject::disconnect(row, nullptr, this, nullptr);
        }
    }
    rows_.clear();
    participantList_->clear();
    for (const auto& cid : participants) {
        const bool isSelf = cid == myIdStd;
        const bool talkChecked =!isSelf && (targets_.count(cid)!= 0);
        const bool muteChecked = isSelf? selfMuted_ : (muted_.count(cid)!= 0);
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
        QLabel* nameLabel = widget? widget->findChild<QLabel*>("participantName") : nullptr;
        const QString text = nameLabel? nameLabel->text().toLower() : QString();
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
        if (muted_.count(it.first)!= 0) continue;
        hearTargets_.insert(it.first);
    }
}

void MainWindow::updateLocalTargets() {
    if (!audio_ ||!discovery_) return;
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
        if (peer.is_local || ip.rfind("127.", 0) == 0) {
            ip = "127.0.0.1";
        }
        destIps.push_back(ip + ":" + std::to_string(peer.port));
    }

    // Always keep output stream and listeners running to hear incoming audio
    if (!audio_->isRunning()) {
        audio_->start(destIps, false, true); // Starts with capture disabled initially
    } else {
        audio_->updateDestinations(destIps);
    }

    // Dynamically manage microphone hardware based on broadcast or talk selections
    bool micNeeded =!targets_.empty();
    audio_->setInputActive(micNeeded);

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

    const bool isBroadcast =!allTargets.empty() && targets_ == allTargets;
    broadcastButton_->blockSignals(true);
    broadcastButton_->setChecked(isBroadcast);
    broadcastButton_->setText(isBroadcast? "Broadcast On" : "Broadcast Off");
    broadcastButton_->blockSignals(false);
}

void MainWindow::stopCaptureIfIdle() {
    // Dynamic resource allocation removes the need for hard thread stops
}

void MainWindow::onThemeToggled(bool checked) {
    applyTheme(checked);
    QSettings().setValue("ui/darkMode", checked);
    QSettings().sync();
}

void MainWindow::applyTheme(bool dark) {
    QApplication::setStyle("Fusion");
    if (dark) {
        QApplication::setPalette(createDarkPalette());
        if (themeSwitch_) themeSwitch_->setText("Dark Theme");
    } else {
        QApplication::setPalette(createLightPalette());
        if (themeSwitch_) themeSwitch_->setText("Light Theme");
    }

    // Force a full window redraw to apply palette changes cleanly
    this->update();
}

QPalette MainWindow::createDarkPalette() const {
    QPalette palette;
    QColor darkBg(45, 45, 45);
    QColor alternateBg(53, 53, 53);
    QColor baseBg(25, 25, 25);
    QColor textWhite(255, 255, 255);
    QColor accentBlue(42, 130, 218);
    QColor disabledGray(128, 128, 128);

    palette.setColor(QPalette::Window, darkBg);
    palette.setColor(QPalette::WindowText, textWhite);
    palette.setColor(QPalette::Base, baseBg);
    palette.setColor(QPalette::AlternateBase, darkBg);
    palette.setColor(QPalette::ToolTipBase, baseBg);
    palette.setColor(QPalette::ToolTipText, textWhite);
    palette.setColor(QPalette::Text, textWhite);
    palette.setColor(QPalette::Button, darkBg);
    palette.setColor(QPalette::ButtonText, textWhite);
    palette.setColor(QPalette::BrightText, Qt::red);
    palette.setColor(QPalette::Link, accentBlue);
    palette.setColor(QPalette::Highlight, accentBlue);
    palette.setColor(QPalette::HighlightedText, Qt::black);

    // Explicit colors for disabled widgets
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabledGray);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabledGray);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledGray);
    palette.setColor(QPalette::Disabled, QPalette::Base, darkBg);

    return palette;
}

QPalette MainWindow::createLightPalette() const {
    // Falls back to the default light theme palette of the Fusion style
    return QApplication::style()->standardPalette();
}

void MainWindow::updateLiveUI() {

    if (!audio_) return;

    const int micLevel = audio_->captureLevel();
    systemLevelBar_->setValue(micLevel);
    if (volumeControls_) volumeControls_->setMicLevel(micLevel);
    std::unordered_map<std::string, bool> speakingState;

    const std::string myIdStd = myId_.toStdString();
    const bool selfState = audio_->captureActive() &&!audio_->isTxMuted();
    if (auto it = rows_.find(myIdStd); it!= rows_.end()) {
        it->second->setVolume(micLevel);
        it->second->setMicStatus(!audio_->isTxMuted());
    }
    speakingState[myIdStd] = selfState;
    lastVoiceMs_[myIdStd] = monotonic_.elapsed();

    const qint64 now = monotonic_.elapsed();
    for (auto& it : rows_) {
        const std::string& cid = it.first;
        if (cid == myIdStd) continue;

        const int peerPeakRaw = audio_->getPeerPeak(cid);
        const int peerLevel = peerPeakRaw > 0? std::min(100, static_cast<int>((peerPeakRaw * 100.0f) / 32767.0f)) : 0;

        const bool activeInstant = peerLevel >= 2;
        if (activeInstant) lastVoiceMs_[cid] = now;
        const qint64 last = lastVoiceMs_.count(cid)? lastVoiceMs_[cid] : 0;
        const bool isActive = (now - last) < 800;

        it.second->setVolume(peerLevel);
        it.second->setMicStatus(isActive);
        const bool prev = speakerState_.count(cid)? speakerState_[cid] : false;
        if (isActive!= prev && speakerLogList_) {
            const QString timestamp = QTime::currentTime().toString("HH:mm:ss");
            const QString msg = QString("[%1] Client %2 %3").arg(timestamp, QString::fromStdString(cid), isActive? "speaking" : "stopped");
            speakerLogList_->addItem(msg);
        }
        speakerState_[cid] = isActive;
        speakingState[cid] = isActive;
    }

    // Prepend transmission telemetry at the top of active speakers list
    QStringList status_lines;
    status_lines.append(QString("Packets: tx=%1 rx=%2 dec=%3")
                   .arg(audio_->debugPacketsSent())
                   .arg(audio_->debugPacketsRecv())
                   .arg(audio_->debugPacketsDecrypted()));
    status_lines.append("-----------------------------");
    
    for (const auto& it : speakingState) {
        status_lines.append(QString("Client %1 - %2").arg(QString::fromStdString(it.first), it.second? "talking" : "listening"));
    }
    
    if (activeSpeakersLabel_) {
        activeSpeakersLabel_->setText(status_lines.join("\n"));
    }
}
