#include "MainWindow.h"

#include "audio/audio_engine.h"
#include "p2p/peer_discovery.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QIcon>
#include <QMessageBox>
#include <QMetaType>
#include <QSettings>
#include <QVariant>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "ui_Popup_message.h"

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

void applySavedAudioSettings(AudioEngine& audio) {
    QSettings s;
    audio.setMasterVolume(readIntSetting(s, "audio/masterVolume", 100));
    audio.setOutputVolume(readIntSetting(s, "audio/outputVolume", 100));
    audio.setGainDb(readIntSetting(s, "audio/txGainDb", 0));
    audio.setMicSensitivity(readIntSetting(s, "audio/micSensitivity", 45));
    audio.setNoiseSuppression(readIntSetting(s, "audio/noiseSuppAmount", 65));
    audio.setAecStreamDelayMs(readIntSetting(s, "audio/aecDelayMs", 180));

    audio.setNoiseSuppressionEnabled(readBoolSetting(s, "audio/noiseSuppEnabled", false));
    audio.setEchoEnabled(readBoolSetting(s, "audio/echoEnabled", false));
    audio.setAutoGain(readBoolSetting(s, "audio/autoGainEnabled", false));

    const QVariant inIdxV = s.value("audio/inputDeviceIndex", QVariant());
    if (inIdxV.isValid() && !inIdxV.toString().isEmpty()) audio.setInputDevice(inIdxV.toInt());
    const QVariant outIdxV = s.value("audio/outputDeviceIndex", QVariant());
    if (outIdxV.isValid() && !outIdxV.toString().isEmpty()) audio.setOutputDevice(outIdxV.toInt());
}

QString getLocalIpBestEffort() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return "127.0.0.1";
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return "127.0.0.1";
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
    (void)connect(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));

    sockaddr_in name{};
    int name_len = sizeof(name);
    QString out = "127.0.0.1";
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&name), &name_len) == 0) {
        char ip_str[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &name.sin_addr, ip_str, sizeof(ip_str))) {
            out = QString::fromLatin1(ip_str);
        }
    }

    closesocket(sock);
    WSACleanup();
    return out;
}
} // namespace

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/icons/app.ico"));
    QCoreApplication::setOrganizationName("Nuummite");
    QCoreApplication::setApplicationName("Nuummite");

    QDialog join;
    Ui::Dialog joinUi;
    joinUi.setupUi(&join);
    if (joinUi.localIpLabel) {
        joinUi.localIpLabel->setText(getLocalIpBestEffort());
    }
    if (join.exec() != QDialog::Accepted) return 0;

    const QString myId = joinUi.nameEdit ? joinUi.nameEdit->text().trimmed() : QString();
    QString room = joinUi.manualIpEdit ? joinUi.manualIpEdit->text().trimmed() : QString("main");
    if (room.isEmpty()) room = "main";

    if (myId.isEmpty()) {
        QMessageBox::warning(nullptr, "Error", "Name is required!");
        return 1;
    }

    AudioEngine audio;
    applySavedAudioSettings(audio);
    audio.setClientId(myId.toStdString());
    audio.setRoomSecret(room.toStdString());

    PeerDiscovery discovery;
    discovery.start(myId.toStdString(), static_cast<uint16_t>(audio.port()), room.toStdString());

    MainWindow win(myId, room, &audio, &discovery);
    win.show();
    win.raise();
    win.activateWindow();
    return app.exec();
}
