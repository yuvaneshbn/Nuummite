#include "audio/audio_engine.h"
#include "p2p/peer_discovery.h"
#include "ui/main_window.h"
#include "ui/components/startup_dialog.h"
#include "../common/winsock_init.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QFile>

#include <cstdlib>
#include <iostream>

QString app_icon_path() {
    QDir dir(QCoreApplication::applicationDirPath());
    return dir.filePath("technical-support.ico");
}

int main(int argc, char* argv[]) {
    WinSockInit wsa;
    if (!wsa.ok()) {
        std::cerr << "[CLIENT] Winsock failed\n";
        return 1;
    }

    QApplication app(argc, argv);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    const QString icon_path = app_icon_path();
    if (QFile::exists(icon_path)) {
        app.setWindowIcon(QIcon(icon_path));
    }

    AudioEngine audio;
    const int audio_port = audio.port();
    std::cout << "[CLIENT] Audio on port " << audio_port << "\n";

    const char* secret_env = std::getenv("VOICE_REGISTER_SECRET");
    const std::string secret = secret_env ? secret_env : "mysecret";

    QString client_id;
    while (true) {
        StartupDialog startup("LAN Mesh (P2P)", audio_port);
        if (startup.exec() != QDialog::Accepted) {
            std::cout << "[CLIENT] Cancelled\n";
            return 0;
        }
        client_id = startup.clientId();
        std::cout << "[CLIENT] ID: " << client_id.toStdString() << "\n";
        break;
    }

    audio.setClientId(client_id.toStdString());

    PeerDiscovery discovery;
    discovery.start(client_id.toStdString(), static_cast<uint16_t>(audio_port));

    std::cout << "[CLIENT] P2P discovery started – connecting UI...\n";

    MainWindow window(client_id, &audio, QString::fromStdString(secret), &discovery);
    window.show();

    const int rc = app.exec();
    discovery.stop();
    audio.shutdown();
    return rc;
}
