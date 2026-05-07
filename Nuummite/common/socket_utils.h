#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <winsock2.h>

namespace socket_utils {

void set_dscp(SOCKET sock, int ip_tos);
bool set_non_blocking(SOCKET sock, bool enabled);

}

#endif
