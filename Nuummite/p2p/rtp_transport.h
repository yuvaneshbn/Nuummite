#ifndef RTP_TRANSPORT_H
#define RTP_TRANSPORT_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <winsock2.h>

class RTPTransport {
public:
    explicit RTPTransport(uint16_t port = 50002);

    // Destinations can be "ip" or "ip:port". If no port is provided, the
    // transport default port_ is used.
    void setDestinations(const std::vector<std::string>& destinations);
    std::vector<std::string> destinations() const;
    bool hasDestinations() const;

    int sendPacket(SOCKET sock, const std::vector<uint8_t>& packet) const;

private:
    struct Destination {
        std::string ip;
        uint16_t port;
        sockaddr_in addr{};
    };

    uint16_t port_;
    mutable std::mutex mutex_;
    std::vector<Destination> destinations_;
};

#endif
