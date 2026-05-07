#include "rtp_transport.h"

#include <ws2tcpip.h>

#include <algorithm>

RTPTransport::RTPTransport(uint16_t port)
    : port_(port) {}

void RTPTransport::setDestinations(const std::vector<std::string>& destinations) {
    std::vector<Destination> resolved;
    for (const auto& ip : destinations) {
        if (ip.empty()) {
            continue;
        }

        std::string host = ip;
        uint16_t port = port_;
        const auto colon = ip.find(':');
        if (colon != std::string::npos) {
            host = ip.substr(0, colon);
            try {
                int parsed = std::stoi(ip.substr(colon + 1));
                if (parsed > 0 && parsed <= 65535) {
                    port = static_cast<uint16_t>(parsed);
                }
            } catch (...) {
                // ignore, fallback to default port_
            }
        }

        auto duplicate = std::find_if(resolved.begin(), resolved.end(), [&host, port](const Destination& entry) {
            return entry.ip == host && entry.port == port;
        });
        if (duplicate != resolved.end()) {
            continue;
        }

        Destination entry;
        entry.ip = host;
        entry.port = port;
        entry.addr.sin_family = AF_INET;
        entry.addr.sin_port = htons(static_cast<u_short>(port));
        if (inet_pton(AF_INET, host.c_str(), &entry.addr.sin_addr) != 1) {
            continue;
        }
        resolved.push_back(std::move(entry));
    }

    std::lock_guard<std::mutex> lock(mutex_);
    destinations_ = std::move(resolved);
}

std::vector<std::string> RTPTransport::destinations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.reserve(destinations_.size());
    for (const auto& entry : destinations_) {
        if (entry.port == port_) {
            out.push_back(entry.ip);
        } else {
            out.push_back(entry.ip + ":" + std::to_string(entry.port));
        }
    }
    return out;
}

bool RTPTransport::hasDestinations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !destinations_.empty();
}

int RTPTransport::sendPacket(SOCKET sock, const std::vector<uint8_t>& packet) const {
    std::vector<Destination> destinations;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        destinations = destinations_;
    }

    int success_count = 0;
    for (const auto& entry : destinations) {
        const int rc = sendto(sock,
                              reinterpret_cast<const char*>(packet.data()),
                              static_cast<int>(packet.size()),
                              0,
                              reinterpret_cast<const sockaddr*>(&entry.addr),
                              sizeof(entry.addr));
        if (rc != SOCKET_ERROR) {
            ++success_count;
            continue;
        }

        // When using non-blocking UDP, treat "would block" as a soft drop.
        const int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            continue;
        }
    }
    return success_count;
}
