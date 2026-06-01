#include "VolumeControlPanel.h"

#include "audio/audio_engine.h"

#include <algorithm>
#include <QCheckBox>
#include <QCoreApplication>
#include <QLabel>
#include <QMetaType>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSlider>

#include "ui_volume_control.h"

namespace {
int readIntSetting(QSettings& settings, const char* key, int defaultValue) {
    bool ok = false;
    const int v = settings.value(key, defaultValue).toInt(&ok);
    return ok ? v : defaultValue;
}

bool readBoolSetting(QSettings& settings, const char* key, bool defaultValue) {
    const QVariant v = settings.value(key, defaultValue);
    if (v.metaType().id() == QMetaType::Bool) return v.toBool();
    if (v.metaType().id() == QMetaType::Int || v.metaType().id() == QMetaType::Double) return v.toBool();
    if (v.metaType().id() == QMetaType::QString) {
        const auto s = v.toString().trimmed().toLower();
        return s == "1" || s == "true" || s == "yes" || s == "on";
    }
    return v.toBool();
}
} // namespace

VolumeControlPanel::VolumeControlPanel(AudioEngine* audio, QWidget* parent)
    : QWidget(parent), audio_(audio), ui_(new Ui::VolumeControlForm) {
    ui_->setupUi(this);

    const auto ticks = QSlider::TicksBelow;
    for (auto* slider : {ui_->masterSlider, ui_->outputSlider, ui_->gainSlider, ui_->inputSensitivitySlider, ui_->noiseSuppressionSlider, ui_->aecDelaySlider}) {
        if (!slider) continue;
        slider->setTickPosition(ticks);
    }
    if (ui_->masterSlider) ui_->masterSlider->setTickInterval(10);
    if (ui_->outputSlider) ui_->outputSlider->setTickInterval(10);
    if (ui_->gainSlider) ui_->gainSlider->setTickInterval(5);
    if (ui_->inputSensitivitySlider) ui_->inputSensitivitySlider->setTickInterval(5);
    if (ui_->noiseSuppressionSlider) ui_->noiseSuppressionSlider->setTickInterval(5);
    if (ui_->aecDelaySlider) ui_->aecDelaySlider->setTickInterval(20);

    connect(ui_->masterSlider, &QSlider::valueChanged, this, &VolumeControlPanel::onMasterChanged, Qt::AutoConnection);
    connect(ui_->outputSlider, &QSlider::valueChanged, this, &VolumeControlPanel::onOutputChanged, Qt::AutoConnection);
    connect(ui_->gainSlider, &QSlider::valueChanged, this, &VolumeControlPanel::onGainChanged, Qt::AutoConnection);
    connect(ui_->inputSensitivitySlider, &QSlider::valueChanged, this, &VolumeControlPanel::onMicSensitivityChanged, Qt::AutoConnection);
    connect(ui_->noiseSuppressionSlider, &QSlider::valueChanged, this, &VolumeControlPanel::onNoiseSuppressionChanged, Qt::AutoConnection);
    connect(ui_->aecDelaySlider, &QSlider::valueChanged, this, &VolumeControlPanel::onAecDelayChanged, Qt::AutoConnection);

    connect(ui_->autoGainCheckbox, &QCheckBox::toggled, this, &VolumeControlPanel::onAutoGainToggled, Qt::AutoConnection);
    connect(ui_->noiseSuppCheckbox, &QCheckBox::toggled, this, &VolumeControlPanel::onNoiseSuppressionToggled, Qt::AutoConnection);
    connect(ui_->echoCheckbox, &QCheckBox::toggled, this, &VolumeControlPanel::onEchoToggled, Qt::AutoConnection);

    connect(ui_->testMicButton, &QPushButton::clicked, this, &VolumeControlPanel::onTestMic, Qt::AutoConnection);
    connect(ui_->restoreDefaultsButton, &QPushButton::clicked, this, &VolumeControlPanel::onRestoreDefaults, Qt::AutoConnection);

    ui_->micLevelBar->setRange(0, 100);
    ui_->micLevelBar->setValue(0);

    loadSettingsIntoUi();
    applyUiToEngine();
    updateValueLabels();
    syncFeatureControls();
}

VolumeControlPanel::~VolumeControlPanel() { delete ui_; }

void VolumeControlPanel::setMicLevel(int level) {
    if (!ui_->micLevelBar) return;
    ui_->micLevelBar->setValue(std::max(0, std::min(100, level)));
}

void VolumeControlPanel::loadSettingsIntoUi() {
    QSettings s;
    ui_->masterSlider->setValue(readIntSetting(s, "audio/masterVolume", 100));
    ui_->outputSlider->setValue(readIntSetting(s, "audio/outputVolume", 100));
    ui_->gainSlider->setValue(readIntSetting(s, "audio/txGainDb", 0));
    ui_->inputSensitivitySlider->setValue(readIntSetting(s, "audio/micSensitivity", 45));
    ui_->noiseSuppressionSlider->setValue(readIntSetting(s, "audio/noiseSuppAmount", 65));
    ui_->aecDelaySlider->setValue(readIntSetting(s, "audio/aecDelayMs", 180));

    ui_->noiseSuppCheckbox->setChecked(readBoolSetting(s, "audio/noiseSuppEnabled", false));
    ui_->echoCheckbox->setChecked(readBoolSetting(s, "audio/echoEnabled", false));
    ui_->autoGainCheckbox->setChecked(readBoolSetting(s, "audio/autoGainEnabled", false));
}

void VolumeControlPanel::applyUiToEngine() {
    if (!audio_) return;
    audio_->setMasterVolume(ui_->masterSlider->value());
    audio_->setOutputVolume(ui_->outputSlider->value());
    audio_->setGainDb(ui_->gainSlider->value());
    audio_->setMicSensitivity(ui_->inputSensitivitySlider->value());
    audio_->setNoiseSuppression(ui_->noiseSuppressionSlider->value());
    audio_->setAecStreamDelayMs(ui_->aecDelaySlider->value());

    audio_->setNoiseSuppressionEnabled(ui_->noiseSuppCheckbox->isChecked());
    audio_->setEchoEnabled(ui_->echoCheckbox->isChecked());
    audio_->setAutoGain(ui_->autoGainCheckbox->isChecked());
}

void VolumeControlPanel::updateValueLabels() {
    if (ui_->masterLabel) ui_->masterLabel->setText(QString("Master Volume: %1").arg(ui_->masterSlider->value()));
    if (ui_->outputLabel) ui_->outputLabel->setText(QString("Output Volume: %1").arg(ui_->outputSlider->value()));
    if (ui_->gainLabel) {
        const bool autogainOn = ui_->autoGainCheckbox && ui_->autoGainCheckbox->isChecked();
        const QString base = QString("Gain (dB): %1").arg(ui_->gainSlider->value());
        ui_->gainLabel->setText(autogainOn ? (base + " (Auto Gain enabled)") : base);
    }
    if (ui_->inputSensitivityLabel) ui_->inputSensitivityLabel->setText(QString("Input Sensitivity: %1").arg(ui_->inputSensitivitySlider->value()));
    if (ui_->noiseSuppressionLabel) ui_->noiseSuppressionLabel->setText(QString("Noise Suppression: %1").arg(ui_->noiseSuppressionSlider->value()));
    if (ui_->aecDelayLabel) ui_->aecDelayLabel->setText(QString("AEC Delay (ms): %1").arg(ui_->aecDelaySlider->value()));
}

void VolumeControlPanel::syncFeatureControls() {
    if (!audio_) return;
    const bool autogainOn = ui_->autoGainCheckbox && ui_->autoGainCheckbox->isChecked();
    const bool rnnoiseOn = ui_->noiseSuppCheckbox && ui_->noiseSuppCheckbox->isChecked();
    const bool echoRequested = ui_->echoCheckbox && ui_->echoCheckbox->isChecked();

    if (ui_->gainSlider) {
        ui_->gainSlider->setEnabled(!autogainOn);
        ui_->gainSlider->setToolTip(autogainOn ? "Disabled because Auto Gain is enabled." : "Manual transmit gain.");
    }
    if (ui_->gainLabel) {
        ui_->gainLabel->setEnabled(!autogainOn);
    }

    if (ui_->noiseSuppressionSlider) ui_->noiseSuppressionSlider->setEnabled(rnnoiseOn);
    if (ui_->noiseSuppressionLabel) ui_->noiseSuppressionLabel->setEnabled(rnnoiseOn);

    // Echo must be available to enable AEC controls.
    const bool echoAvailable = audio_->echoAvailable();
    if (ui_->echoCheckbox) ui_->echoCheckbox->setEnabled(echoAvailable);
    if (!echoAvailable && ui_->echoCheckbox && ui_->echoCheckbox->isChecked()) {
        // Keep UI/engine consistent if the feature isn't available.
        ui_->echoCheckbox->blockSignals(true);
        ui_->echoCheckbox->setChecked(false);
        ui_->echoCheckbox->blockSignals(false);
        audio_->setEchoEnabled(false);
        QSettings().setValue("audio/echoEnabled", false);
    }

    const bool echoOn = echoAvailable && echoRequested;
    if (ui_->aecDelaySlider) ui_->aecDelaySlider->setEnabled(echoOn);
    if (ui_->aecDelayLabel) ui_->aecDelayLabel->setEnabled(echoOn);

    updateValueLabels();
}

void VolumeControlPanel::onMasterChanged(int value) {
    if (audio_) audio_->setMasterVolume(value);
    QSettings().setValue("audio/masterVolume", value);
    updateValueLabels();
}

void VolumeControlPanel::onOutputChanged(int value) {
    if (audio_) audio_->setOutputVolume(value);
    QSettings().setValue("audio/outputVolume", value);
    updateValueLabels();
}

void VolumeControlPanel::onGainChanged(int value) {
    if (audio_) audio_->setGainDb(value);
    QSettings().setValue("audio/txGainDb", value);
    updateValueLabels();
}

void VolumeControlPanel::onMicSensitivityChanged(int value) {
    if (audio_) audio_->setMicSensitivity(value);
    QSettings().setValue("audio/micSensitivity", value);
    updateValueLabels();
}

void VolumeControlPanel::onNoiseSuppressionChanged(int value) {
    if (audio_) audio_->setNoiseSuppression(value);
    QSettings().setValue("audio/noiseSuppAmount", value);
    updateValueLabels();
}

void VolumeControlPanel::onAecDelayChanged(int value) {
    if (audio_) audio_->setAecStreamDelayMs(value);
    QSettings().setValue("audio/aecDelayMs", value);
    updateValueLabels();
}

void VolumeControlPanel::onAutoGainToggled(bool enabled) {
    if (audio_) audio_->setAutoGain(enabled);
    QSettings().setValue("audio/autoGainEnabled", enabled);
    syncFeatureControls();
}

void VolumeControlPanel::onNoiseSuppressionToggled(bool enabled) {
    if (audio_) audio_->setNoiseSuppressionEnabled(enabled);
    QSettings().setValue("audio/noiseSuppEnabled", enabled);
    syncFeatureControls();
}

void VolumeControlPanel::onEchoToggled(bool enabled) {
    if (audio_) audio_->setEchoEnabled(enabled);
    QSettings().setValue("audio/echoEnabled", enabled);
    syncFeatureControls();
}

void VolumeControlPanel::onTestMic() {
    if (!audio_ || !ui_->testStatusLabel) return;
    ui_->testStatusLabel->setText("Testing...");
    QCoreApplication::processEvents();

    const int level = audio_->testMicrophoneLevel(1.0);
    ui_->testStatusLabel->setText(QString("Level: %1").arg(level));
}

void VolumeControlPanel::onRestoreDefaults() {
    // Persist defaults explicitly (setValue() won't emit if the value is unchanged).
    QSettings s;
    s.setValue("audio/masterVolume", 100);
    s.setValue("audio/outputVolume", 100);
    s.setValue("audio/txGainDb", 0);
    s.setValue("audio/micSensitivity", 45);
    s.setValue("audio/noiseSuppAmount", 65);
    s.setValue("audio/aecDelayMs", 180);
    s.setValue("audio/autoGainEnabled", false);
    s.setValue("audio/noiseSuppEnabled", false);
    s.setValue("audio/echoEnabled", false);

    loadSettingsIntoUi();
    applyUiToEngine();
    updateValueLabels();
    syncFeatureControls();
}
