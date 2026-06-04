#ifndef NUUMMITE_VOLUMECONTROLPANEL_H
#define NUUMMITE_VOLUMECONTROLPANEL_H

#include <QWidget>
#include <QList>

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

    // Static registry tracking active control panels to sync values in real-time
    inline static QList<VolumeControlPanel*> instances_;

private:
    void loadSettingsIntoUi();
    void applyUiToEngine();
    void updateValueLabels();
    void syncFeatureControls();
    void notifyObservers();

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

#endif // NUUMMITE_VOLUMECONTROLPANEL_H