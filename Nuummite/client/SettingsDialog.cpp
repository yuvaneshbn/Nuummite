#include "SettingsDialog.h"

#include "VolumeControlPanel.h"
#include "audio/audio_engine.h"

#include <optional>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QVariant>
#include <QVBoxLayout>

#include "ui_settings_dialog.h"

namespace {
std::optional<int> readOptionalInt(QSettings& settings, const char* key) {
    const QVariant v = settings.value(key, QVariant());
    if (!v.isValid() || v.toString().isEmpty()) return std::nullopt;
    bool ok = false;
    const int out = v.toInt(&ok);
    return ok ? std::optional<int>(out) : std::nullopt;
}
} // namespace

SettingsDialog::SettingsDialog(AudioEngine* audio, const QString& serverLabel, QWidget* parent)
    : QDialog(parent), audio_(audio), ui_(new Ui::SettingsDialogForm) {
    setModal(true);

    form_ = new QWidget(this);
    ui_->setupUi(form_);
    setWindowTitle(form_->windowTitle());

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(form_);
    rootLayout->addWidget(scroll);

    if (ui_->serverIpValue) ui_->serverIpValue->setText(serverLabel);

    volumeControls_ = new VolumeControlPanel(audio_, this);
    if (ui_->volumeControlHint) ui_->volumeControlHint->hide();
    if (ui_->advancedAudioLayout) {
        while (ui_->advancedAudioLayout->count() > 0) {
            QLayoutItem* item = ui_->advancedAudioLayout->takeAt(0);
            if (auto* w = item ? item->widget() : nullptr) w->deleteLater();
            delete item;
        }
        ui_->advancedAudioLayout->addWidget(volumeControls_);
    }

    applySavedDevicesToEngineBestEffort();
    populateDevices();
    loadSettings();

    connect(ui_->reconnectButton, &QPushButton::clicked, this, &SettingsDialog::onReconnect, Qt::AutoConnection);
    connect(ui_->saveCloseButton, &QPushButton::clicked, this, &SettingsDialog::onSaveAndClose, Qt::AutoConnection);
    connect(ui_->cancelButton, &QPushButton::clicked, this, &SettingsDialog::onCancel, Qt::AutoConnection);
    connect(ui_->inputDeviceCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::onInputDeviceChanged, Qt::AutoConnection);
    connect(ui_->outputDeviceCombo, &QComboBox::currentIndexChanged, this, &SettingsDialog::onOutputDeviceChanged, Qt::AutoConnection);

    // Ensure the dialog opens at a sensible size (the ScrollArea would otherwise allow a tiny window).
    form_->adjustSize();
    adjustSize();

    QSize target = form_->sizeHint();
    target.rwidth() += 60;
    target.rheight() += 60;
    target = target.expandedTo(QSize(720, 520));

    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QSize avail = screen->availableGeometry().size();
        target.setWidth(std::min(target.width(), std::max(480, avail.width() - 80)));
        target.setHeight(std::min(target.height(), std::max(360, avail.height() - 80)));

        setMinimumSize(QSize(std::min(640, std::max(480, avail.width() - 120)),
                             std::min(480, std::max(360, avail.height() - 120))));
    } else {
        setMinimumSize(QSize(640, 480));
    }

    resize(target);
    setSizeGripEnabled(true);
}

SettingsDialog::~SettingsDialog() { delete ui_; }

void SettingsDialog::restoreComboToDevice(QComboBox* combo, int deviceIndex) {
    if (!combo) return;
    const bool wasBlocked = combo->blockSignals(true);
    const int idx = combo->findData(deviceIndex);
    if (idx >= 0) combo->setCurrentIndex(idx);
    combo->blockSignals(wasBlocked);
}

void SettingsDialog::applySavedDevicesToEngineBestEffort() {
    if (!audio_) return;
    QSettings s;

    const auto inIdx = readOptionalInt(s, "audio/inputDeviceIndex");
    const auto outIdx = readOptionalInt(s, "audio/outputDeviceIndex");
    if (inIdx) (void)audio_->setInputDevice(*inIdx);
    if (outIdx) (void)audio_->setOutputDevice(*outIdx);
}

void SettingsDialog::populateDevices() {
    if (!audio_) return;

    const bool inWasBlocked = ui_->inputDeviceCombo->blockSignals(true);
    const bool outWasBlocked = ui_->outputDeviceCombo->blockSignals(true);

    ui_->inputDeviceCombo->clear();
    for (const auto& dev : audio_->listInputDevices()) {
        const QString label = QString::fromStdString(dev.name);
        ui_->inputDeviceCombo->addItem(label, dev.index);
    }

    ui_->outputDeviceCombo->clear();
    for (const auto& dev : audio_->listOutputDevices()) {
        const QString label = QString::fromStdString(dev.name);
        ui_->outputDeviceCombo->addItem(label, dev.index);
    }

    restoreComboToDevice(ui_->inputDeviceCombo, audio_->inputDeviceIndex());
    restoreComboToDevice(ui_->outputDeviceCombo, audio_->outputDeviceIndex());

    ui_->inputDeviceCombo->blockSignals(inWasBlocked);
    ui_->outputDeviceCombo->blockSignals(outWasBlocked);
}

void SettingsDialog::loadSettings() {
    QSettings s;

    if (auto inIdx = readOptionalInt(s, "audio/inputDeviceIndex")) {
        restoreComboToDevice(ui_->inputDeviceCombo, *inIdx);
    }
    if (auto outIdx = readOptionalInt(s, "audio/outputDeviceIndex")) {
        restoreComboToDevice(ui_->outputDeviceCombo, *outIdx);
    }
}

void SettingsDialog::saveSettings() {
    if (!audio_) return;
    QSettings s;

    const QVariant inData = ui_->inputDeviceCombo->currentData();
    if (inData.isValid()) {
        const int idx = inData.toInt();
        s.setValue("audio/inputDeviceIndex", idx);
        audio_->setInputDevice(idx);
    }

    const QVariant outData = ui_->outputDeviceCombo->currentData();
    if (outData.isValid()) {
        const int idx = outData.toInt();
        s.setValue("audio/outputDeviceIndex", idx);
        audio_->setOutputDevice(idx);
    }
}

void SettingsDialog::onInputDeviceChanged(int) {
    if (!audio_) return;
    const QVariant data = ui_->inputDeviceCombo->currentData();
    if (!data.isValid()) return;

    QSettings s;
    const int requested = data.toInt();
    const int previous = audio_->inputDeviceIndex();
    const bool ok = audio_->setInputDevice(requested);
    if (!ok) {
        QMessageBox::critical(
            this,
            "Input device error",
            QString("Failed to open the selected input device.\n\nDevice: %1\nIndex: %2").arg(ui_->inputDeviceCombo->currentText()).arg(requested));
        restoreComboToDevice(ui_->inputDeviceCombo, previous);
        return;
    }
    s.setValue("audio/inputDeviceIndex", requested);
}

void SettingsDialog::onOutputDeviceChanged(int) {
    if (!audio_) return;
    const QVariant data = ui_->outputDeviceCombo->currentData();
    if (!data.isValid()) return;

    QSettings s;
    const int requested = data.toInt();
    const int previous = audio_->outputDeviceIndex();
    const bool ok = audio_->setOutputDevice(requested);
    if (!ok) {
        QMessageBox::critical(
            this,
            "Output device error",
            QString("Failed to open the selected output device.\n\nDevice: %1\nIndex: %2").arg(ui_->outputDeviceCombo->currentText()).arg(requested));
        restoreComboToDevice(ui_->outputDeviceCombo, previous);
        return;
    }
    s.setValue("audio/outputDeviceIndex", requested);
}

void SettingsDialog::onSaveAndClose() {
    saveSettings();
    QSettings().sync();
    accept();
}

void SettingsDialog::onCancel() { reject(); }

void SettingsDialog::onReconnect() {
    QMessageBox::information(this, "Reconnect", "Reconnect not required for LAN mesh.");
}
