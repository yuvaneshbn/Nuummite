#ifndef NUUMMITE_SETTINGSDIALOG_H
#define NUUMMITE_SETTINGSDIALOG_H

#include <QDialog>

class AudioEngine;
class QComboBox;
class VolumeControlPanel;

namespace Ui {
class SettingsDialogForm;
}

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    SettingsDialog(AudioEngine* audio, const QString& serverLabel, QWidget* parent = nullptr);
    ~SettingsDialog() override;

private:
    void restoreComboToDevice(QComboBox* combo, int deviceIndex);
    void applySavedDevicesToEngineBestEffort();
    void populateDevices();
    void loadSettings();
    void saveSettings();

    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);
    void onSaveAndClose();
    void onCancel();
    void onReconnect();

    AudioEngine* audio_ = nullptr;
    QWidget* form_ = nullptr;
    Ui::SettingsDialogForm* ui_ = nullptr;
    VolumeControlPanel* volumeControls_ = nullptr;
};

#endif
