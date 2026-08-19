#include "MainWindow.h"
#include "CrystalRoomSelectionDialog.h"
#include "audio/audio_engine.h"
#include "p2p/peer_discovery.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QMessageBox>
#include <QMetaType>
#include <QSettings>
#include <QVariant>

#include <algorithm>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <objbase.h>

namespace {

// Restore saved audio device indices ONLY.
// All other parameters (volume, gain, noise suppression, AEC delay, etc.)
// are applied by VolumeControlPanel::applyUiToEngine() inside the MainWindow
// constructor.  Applying them here a second time would call setInputDevice()
// while the engine is already running, triggering an unnecessary restart.
void applySavedDeviceSettings(AudioEngine& audio) {
    QSettings s;

    const QVariant inIdxV = s.value("audio/inputDeviceIndex", QVariant());
    if (inIdxV.isValid() && !inIdxV.toString().isEmpty()) {
        const int requested = inIdxV.toInt();
        const auto inputs = audio.listInputDevices();
        const bool exists = std::any_of(inputs.begin(), inputs.end(),
            [requested](const AudioDeviceInfo& dev) { return dev.index == requested; });
        audio.setInputDevice(exists ? requested : -1);
    }

    const QVariant outIdxV = s.value("audio/outputDeviceIndex", QVariant());
    if (outIdxV.isValid() && !outIdxV.toString().isEmpty()) {
        const int requested = outIdxV.toInt();
        const auto outputs = audio.listOutputDevices();
        const bool exists = std::any_of(outputs.begin(), outputs.end(),
            [requested](const AudioDeviceInfo& dev) { return dev.index == requested; });
        audio.setOutputDevice(exists ? requested : -1);
    }
}

} // namespace

int main(int argc, char* argv[]) {
    // Initialize the main thread as STA so Qt/OLE subsystems can initialize cleanly.
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << " Failed to set main thread apartment state to COM STA\n";
    }

    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/icons/app.ico"));
    QCoreApplication::setOrganizationName("Nuummite");
    QCoreApplication::setApplicationName("Nuummite");

    // Show the unified room-selection / login dialog.
    // It collects display name, target room, and passphrase in one step.
    CrystalRoomSelectionDialog joinDlg;
    if (joinDlg.exec() != QDialog::Accepted) {
        if (SUCCEEDED(hr)) CoUninitialize();
        return 0;
    }

    const QString myId      = joinDlg.usernameValue();
    QString       room      = joinDlg.targetRoomValue();
    const QString passphrase = joinDlg.roomPassphraseValue();

    if (room.isEmpty())       room = QStringLiteral("main");
    if (myId.isEmpty()) {
        QMessageBox::warning(nullptr, "Error", "Display name is required!");
        if (SUCCEEDED(hr)) CoUninitialize();
        return 1;
    }
    if (passphrase.isEmpty()) {
        QMessageBox::warning(nullptr, "Error", "Room passphrase is required!");
        if (SUCCEEDED(hr)) CoUninitialize();
        return 1;
    }

    // Create and pre-configure the audio engine.
    // applySavedDeviceSettings only sets input/output device indices
    // (stored before engine start so no restart is triggered).
    // Volume/gain/noise/AEC settings are loaded later by VolumeControlPanel.
    AudioEngine audio;
    applySavedDeviceSettings(audio);
    audio.setClientId(myId.toStdString());
    audio.setRoomSecret(passphrase.toStdString());

    PeerDiscovery discovery;
    discovery.start(myId.toStdString(), static_cast<uint16_t>(audio.port()), room.toStdString());

    MainWindow win(myId, room, &audio, &discovery);
    win.show();
    win.raise();
    win.activateWindow();

    const int ret_code = app.exec();

    // Safely uninitialize the COM stack on exit
    if (SUCCEEDED(hr)) {
        CoUninitialize();
    }
    return ret_code;
}
