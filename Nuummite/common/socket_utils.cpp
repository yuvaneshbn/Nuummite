#include "socket_utils.h"

#include <mswsock.h>
#include <ws2tcpip.h>

namespace socket_utils {

void set_dscp(SOCKET sock, int ip_tos) {
    if (sock == INVALID_SOCKET) {
        return;
    }
    const int tos = ip_tos;
    setsockopt(sock, IPPROTO_IP, IP_TOS, reinterpret_cast<const char*>(&tos), sizeof(tos));
}

bool set_non_blocking(SOCKET sock, bool enabled) {
    if (sock == INVALID_SOCKET) {
        return false;
    }
    u_long mode = enabled ? 1UL : 0UL;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
}

bool disable_udp_connreset(SOCKET sock) {
    if (sock == INVALID_SOCKET) {
        return false;
    }

    // Prevent WSAECONNRESET on recvfrom() after ICMP "Port Unreachable" on Windows UDP.
    // See SIO_UDP_CONNRESET behavior (Winsock).
    BOOL new_behavior = FALSE;
    DWORD bytes_returned = 0;
    return WSAIoctl(sock,
                    SIO_UDP_CONNRESET,
                    &new_behavior,
                    sizeof(new_behavior),
                    nullptr,
                    0,
                    &bytes_returned,
                    nullptr,
                    nullptr) == 0;
}

}
