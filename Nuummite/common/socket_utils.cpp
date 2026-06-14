#include "socket_utils.h"
#include <mswsock.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

#ifndef SIO_UDP_NETRESET
#define SIO_UDP_NETRESET _WSAIOW(IOC_VENDOR, 11)
#endif

namespace socket_utils {

void set_dscp(SOCKET sock, int ip_tos) {
    if (sock == INVALID_SOCKET) return;
    const int tos = ip_tos;
    setsockopt(sock, IPPROTO_IP, IP_TOS, reinterpret_cast<const char*>(&tos), sizeof(tos));
}

bool set_non_blocking(SOCKET sock, bool enabled) {
    if (sock == INVALID_SOCKET) return false;
    u_long mode = enabled? 1UL : 0UL;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
}

bool disable_udp_connreset(SOCKET sock) {
    if (sock == INVALID_SOCKET) return false;

    BOOL new_behavior = FALSE;
    DWORD bytes_returned = 0;
    
    // Disable connection reset errors caused by ICMP Port Unreachable
    int r1 = WSAIoctl(sock, SIO_UDP_CONNRESET, &new_behavior, sizeof(new_behavior),
                      nullptr, 0, &bytes_returned, nullptr, nullptr);

    // Disable network reset errors caused by connection drops
    BOOL net_behavior = FALSE;
    DWORD net_bytes_returned = 0;
    int r2 = WSAIoctl(sock, SIO_UDP_NETRESET, &net_behavior, sizeof(net_behavior),
                      nullptr, 0, &net_bytes_returned, nullptr, nullptr);

    return (r1 == 0) && (r2 == 0);
}

} // namespace socket_utils
