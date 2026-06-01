#ifndef NUUMMITE_VOLUMECONTROLPANEL_H
#define NUUMMITE_VOLUMECONTROLPANEL_H

#include <QWidget>

class AudioEngine;

namespace Ui {
class VolumeControlForm;
}

class VolumeControlPanel final : public QWidget {
    Q_OBJECT

public:
    explicit VolumeControlPanel(AudioEngine* audio, QWidget* parent = nullptr);
    ~VolumeControlPanel() override;

    void setMicLevel(int level);

private:
    void loadSettingsIntoUi();
    void applyUiToEngine();
    void updateValueLabels();
    void syncFeatureControls();

    void onMasterChanged(int value);
    void onOutputChanged(int value);
    void onGainChanged(int value);
    void onMicSensitivityChanged(int value);
    void onNoiseSuppressionChanged(int value);
    void onAecDelayChanged(int value);

    void onAutoGainToggled(bool enabled);
    void onNoiseSuppressionToggled(bool enabled);
    void onEchoToggled(bool enabled);

    void onTestMic();
    void onRestoreDefaults();

    AudioEngine* audio_ = nullptr;
    Ui::VolumeControlForm* ui_ = nullptr;
};

#endif

