#include "audio/audio_engine.h"
#include "p2p/peer_discovery.h"
#include "ui/main_window.h"
#include "ui/components/startup_dialog.h"
#include "../common/winsock_init.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QInputDialog>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <iostream>

QString app_icon_path() {
    QDir dir(QCoreApplication::applicationDirPath());
    return dir.filePath("technical-support.ico");
}

// Return first non-loopback IPv4 address, or empty if none.
QString primary_ipv4() {
    char hostname[256] = {0};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return {};
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &res) != 0) {
        return {};
    }
    QString found;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        auto* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        if (!addr) continue;
        const uint32_t ip = ntohl(addr->sin_addr.s_addr);
        // Skip loopback 127.x.x.x and APIPA 169.254.x.x
        if ((ip >> 24) == 127 || ((ip >> 16) == 0xA9FE)) continue;
        char buf[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf))) {
            found = QString::fromLatin1(buf);
            break;
        }
    }
    if (res) freeaddrinfo(res);
    return found;
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

    const QString local_ip = primary_ipv4();
    QString prefill;
    if (!local_ip.isEmpty()) {
        prefill = QString("Me=%1\n").arg(local_ip);
    }
    QString manualText = QInputDialog::getMultiLineText(nullptr,
        "Campus Cross-Building Connect (optional)",
        "Add peers (leave blank to skip):\n- id=ip   (e.g., Thor=192.168.10.45)\n- or just ip   (id defaults to the ip)\n\nYour local IPv4 (for sharing):",
        prefill, nullptr, Qt::Dialog);
    if (!manualText.isEmpty()) {
        const auto lines = manualText.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const auto parts = line.split('=', Qt::KeepEmptyParts);
            QString id = parts.size() >= 2 ? parts[0].trimmed() : QString();
            QString ip = parts.size() >= 2 ? parts[1].trimmed() : line.trimmed();
            if (ip.isEmpty()) continue;
            discovery.addManualPeer(id.toStdString(), ip.toStdString());
        }
    }

    std::cout << "[CLIENT] P2P discovery started - connecting UI...\n";

    MainWindow window(client_id, &audio, QString::fromStdString(secret), &discovery);
    window.show();

    const int rc = app.exec();
    discovery.stop();
    audio.shutdown();
    return rc;
}
