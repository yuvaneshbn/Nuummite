#include "CrystalRoomSelectionDialog.h"
#include "LoadingSpinner.h"
#include "p2p/peer_discovery.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <unordered_set>

CrystalRoomSelectionDialog::CrystalRoomSelectionDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("Nuummite \u2013 Connect to Room"));

    buildLayout();
    initializeStyles();
    // Let Qt compute the correct minimum from widget content — prevents
    // the QWindowsWindow::setGeometry geometry conflict warning.
    adjustSize();
    setMinimumSize(sizeHint());

    // PeerDiscovery is created here but NOT started yet.
    // It is started only when the user clicks "Scan" and stopped
    // immediately after, so no "configuration_probe" announcement
    // reaches other clients on the network.
    liveDiscoveryEngine_ = new PeerDiscovery();
}

CrystalRoomSelectionDialog::~CrystalRoomSelectionDialog() {
    if (liveDiscoveryEngine_) {
        liveDiscoveryEngine_->stop();
        delete liveDiscoveryEngine_;
    }
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void CrystalRoomSelectionDialog::buildLayout() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setSpacing(16);
    rootLayout->setContentsMargins(20, 20, 20, 16);

    // ── Title ──────────────────────────────────────────────────────────────
    auto* titleLabel = new QLabel(QStringLiteral("Connect to a Voice Room"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(titleLabel);

    // ── Form ───────────────────────────────────────────────────────────────
    auto* form = new QFormLayout();
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    // Display name
    userField_ = new QLineEdit(this);
    userField_->setPlaceholderText(QStringLiteral("e.g. Alice"));
    form->addRow(QStringLiteral("Display Name:"), userField_);

    rootLayout->addLayout(form);

    // ── Mode toggle ────────────────────────────────────────────────────────
    modeToggleCheckbox_ = new QCheckBox(QStringLiteral("Create a New Room"), this);
    rootLayout->addWidget(modeToggleCheckbox_);

    // ── Create-mode container ──────────────────────────────────────────────
    createModeContainer_ = new QWidget(this);
    {
        auto* layout = new QFormLayout(createModeContainer_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setVerticalSpacing(8);
        roomNameField_ = new QLineEdit(createModeContainer_);
        roomNameField_->setPlaceholderText(QStringLiteral("Enter unique room name…"));
        layout->addRow(QStringLiteral("New Room Name:"), roomNameField_);
    }
    rootLayout->addWidget(createModeContainer_);

    // ── Join-mode container ────────────────────────────────────────────────
    joinModeContainer_ = new QWidget(this);
    {
        auto* layout = new QFormLayout(joinModeContainer_);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setVerticalSpacing(8);

        auto* comboRow = new QWidget(joinModeContainer_);
        auto* comboRowLayout = new QHBoxLayout(comboRow);
        comboRowLayout->setContentsMargins(0, 0, 0, 0);
        comboRowLayout->setSpacing(6);

        roomSelectorCombo_ = new QComboBox(joinModeContainer_);
        scanRoomsButton_   = new QPushButton(QStringLiteral("Scan"), joinModeContainer_);
        scanRoomsButton_->setFixedWidth(64);

        comboRowLayout->addWidget(roomSelectorCombo_, 1);
        comboRowLayout->addWidget(scanRoomsButton_);

        // Status label shown below the combo — updated by handleRoomScan()
        scanStatusLabel_ = new QLabel(
            QStringLiteral("Press \"Scan\" to discover active rooms on your network."),
            joinModeContainer_);
        scanStatusLabel_->setWordWrap(true);
        scanStatusLabel_->setObjectName(QStringLiteral("scanStatusLabel"));

        layout->addRow(QStringLiteral("Target Room:"), comboRow);
        layout->addRow(QString(), scanStatusLabel_);
    }
    rootLayout->addWidget(joinModeContainer_);

    // ── Passphrase ─────────────────────────────────────────────────────────
    auto* passphraseForm = new QFormLayout();
    passphraseForm->setVerticalSpacing(8);
    passphraseField_ = new QLineEdit(this);
    passphraseField_->setEchoMode(QLineEdit::Password);
    passphraseField_->setPlaceholderText(QStringLiteral("Room passphrase (not transmitted)…"));
    passphraseForm->addRow(QStringLiteral("Passphrase:"), passphraseField_);
    rootLayout->addLayout(passphraseForm);

    // ── Footer: spinner + Connect button ───────────────────────────────────
    auto* footerRow = new QHBoxLayout();
    footerRow->setSpacing(10);

    transitionLoader_ = new LoadingSpinner(this);
    transitionLoader_->setFixedSize(24, 24);
    transitionLoader_->stopAnimation();

    proceedButton_ = new QPushButton(QStringLiteral("Connect"), this);
    proceedButton_->setObjectName(QStringLiteral("proceedButton"));
    proceedButton_->setFixedHeight(34);
    proceedButton_->setDefault(true);

    footerRow->addWidget(transitionLoader_);
    footerRow->addStretch();
    footerRow->addWidget(proceedButton_);
    rootLayout->addLayout(footerRow);

    // ── Wire signals ───────────────────────────────────────────────────────
    connect(modeToggleCheckbox_, &QCheckBox::toggled, this, &CrystalRoomSelectionDialog::handleModeToggle);
    connect(scanRoomsButton_,    &QPushButton::clicked, this, &CrystalRoomSelectionDialog::handleRoomScan);
    connect(proceedButton_,      &QPushButton::clicked, this, &CrystalRoomSelectionDialog::handleConnectAttempt);

    // Default: join mode, no rooms scanned yet — Connect disabled until scan runs
    handleModeToggle(false);
    proceedButton_->setEnabled(false);
}

void CrystalRoomSelectionDialog::initializeStyles() {
    setStyleSheet(R"(
        CrystalRoomSelectionDialog {
            background-color: #2D2D2D;
        }
        QLabel {
            color: #E0E0E0;
        }
        QLineEdit {
            background-color: #1A1A1A;
            color: #FFFFFF;
            border: 1px solid #444444;
            border-radius: 5px;
            padding: 5px 8px;
            font-size: 12px;
        }
        QLineEdit:focus {
            border: 1px solid #1E8E3E;
        }
        QCheckBox {
            color: #E0E0E0;
            font-size: 12px;
        }
        QComboBox {
            background-color: #1A1A1A;
            color: #E0E0E0;
            border: 1px solid #444444;
            border-radius: 5px;
            padding: 4px 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #2D2D2D;
            color: #E0E0E0;
            border: 1px solid #444444;
            selection-background-color: #1E8E3E;
            selection-color: #FFFFFF;
        }
        QPushButton {
            background-color: #3A3A3A;
            color: #FFFFFF;
            border: 1px solid #555555;
            border-radius: 5px;
            padding: 5px 14px;
            font-size: 12px;
        }
        QPushButton:hover {
            background-color: #484848;
        }
        QPushButton#proceedButton {
            background-color: #1E8E3E;
            border: 1px solid #15692B;
            font-weight: bold;
            min-width: 100px;
        }
        QPushButton#proceedButton:hover {
            background-color: #22A047;
        }
        QPushButton#proceedButton:disabled {
            background-color: #2D5C3A;
            color: #888888;
        }
        QLabel#scanStatusLabel {
            color: #AAAAAA;
            font-size: 11px;
            font-style: italic;
        }
        QComboBox:disabled {
            color: #666666;
            background-color: #141414;
        }
    )");
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CrystalRoomSelectionDialog::handleModeToggle(bool isCreateMode) {
    createModeContainer_->setVisible(isCreateMode);
    joinModeContainer_->setVisible(!isCreateMode);

    // In create mode the Connect button is always enabled (user types their own name).
    // In join mode it depends on whether any real rooms were scanned.
    if (isCreateMode) {
        proceedButton_->setEnabled(true);
    } else {
        // Re-evaluate based on current combo contents (real rooms only, not placeholder)
        const bool hasRealRoom = roomSelectorCombo_->count() > 0
                                 && roomSelectorCombo_->itemData(0).toBool(); // real = true
        proceedButton_->setEnabled(hasRealRoom);
    }
    adjustSize();
}

void CrystalRoomSelectionDialog::handleRoomScan() {
    if (!liveDiscoveryEngine_) return;

    // Disable scan button while the probe is running
    scanRoomsButton_->setEnabled(false);
    scanRoomsButton_->setText(QStringLiteral("Scanning\u2026"));
    scanStatusLabel_->setText(QStringLiteral("Scanning for active rooms\u2026"));
    proceedButton_->setEnabled(false);

    // The probe ID must be unique so the loopback filter below works.
    // We use the same ID every time — the filter explicitly excludes
    // any peer packet whose room is the one WE announced (multicast
    // loopback causes the socket to receive its own packet).
    const std::string probeRoom = "__scan_probe__";
    liveDiscoveryEngine_->start("configuration_probe", 50005, probeRoom);

    // Give real peers ~500 ms to respond, then collect and stop immediately.
    QTimer::singleShot(500, this, [this, probeRoom]() {
        const std::vector<PeerInfo> discovered = liveDiscoveryEngine_->peers();
        liveDiscoveryEngine_->stop();   // Stop broadcasting right away

        // Collect unique room names, excluding:
        //   - the probe's own room (multicast loopback self-announcement)
        //   - configuration_probe peer id (shouldn't appear, but guard anyway)
        std::unordered_set<std::string> liveRooms;
        for (const auto& peer : discovered) {
            if (peer.id == "configuration_probe") continue;  // own echo
            if (peer.room == probeRoom)           continue;  // loopback self
            if (!peer.room.empty()) {
                liveRooms.insert(peer.room);
            }
        }

        roomSelectorCombo_->clear();
        if (liveRooms.empty()) {
            // Add a disabled placeholder — NOT a joinable room
            roomSelectorCombo_->addItem(QStringLiteral("(no active rooms found)"),
                                        QVariant(false));  // false = placeholder
            roomSelectorCombo_->setEnabled(false);
            scanStatusLabel_->setText(
                QStringLiteral("No active rooms found. Use \"Create a New Room\" to start one."));
            proceedButton_->setEnabled(false);
        } else {
            roomSelectorCombo_->setEnabled(true);
            for (const auto& room : liveRooms) {
                roomSelectorCombo_->addItem(QString::fromStdString(room),
                                            QVariant(true));  // true = real room
            }
            scanStatusLabel_->setText(
                QString(QStringLiteral("%1 active room(s) found.")).arg(liveRooms.size()));
            proceedButton_->setEnabled(true);
        }

        scanRoomsButton_->setEnabled(true);
        scanRoomsButton_->setText(QStringLiteral("Scan"));
    });
}

void CrystalRoomSelectionDialog::handleConnectAttempt() {
    const QString identity   = userField_->text().trimmed();
    const QString targetRoom = createRoomChecked()
                                   ? roomNameField_->text().trimmed()
                                   : roomSelectorCombo_->currentText().trimmed();
    const QString securityKey = passphraseField_->text().trimmed();

    if (identity.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Missing Information"),
                             QStringLiteral("Please enter a display name."));
        userField_->setFocus();
        return;
    }
    if (targetRoom.isEmpty() || targetRoom.startsWith(QLatin1String("("))) {
        QMessageBox::warning(this, QStringLiteral("No Room Selected"),
                             QStringLiteral("No active rooms were found.\n\n"
                                            "Use \"Scan\" to search again, or tick "
                                            "\"Create a New Room\" to start your own."));
        return;
    }
    if (securityKey.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Missing Information"),
                             QStringLiteral("Please enter the room passphrase."));
        passphraseField_->setFocus();
        return;
    }

    proceedButton_->setEnabled(false);
    transitionLoader_->startAnimation();

    // Brief delay so the spinner renders before the dialog closes
    QTimer::singleShot(500, this, [this]() {
        transitionLoader_->stopAnimation();
        accept();
    });
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

QString CrystalRoomSelectionDialog::usernameValue() const {
    return userField_->text().trimmed();
}

QString CrystalRoomSelectionDialog::roomPassphraseValue() const {
    return passphraseField_->text().trimmed();
}

bool CrystalRoomSelectionDialog::createRoomChecked() const {
    return modeToggleCheckbox_->isChecked();
}

QString CrystalRoomSelectionDialog::targetRoomValue() const {
    if (createRoomChecked()) {
        return roomNameField_->text().trimmed();
    }
    return roomSelectorCombo_->currentText().trimmed();
}
