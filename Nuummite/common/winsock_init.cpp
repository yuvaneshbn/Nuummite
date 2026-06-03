#include "winsock_init.h"
#include <iostream>

WinSockInit::WinSockInit() {
    // Initialize Winsock 2.2 without touching OLE/COM apartments [2]
    const int rc = WSAStartup(MAKEWORD(2, 2), &wsa_);
    ok_ = (rc == 0);
    if (!ok_) {
        std::cerr << " Failed to initialize Winsock stack, error: " << rc << "\n";
    }
}

WinSockInit::~WinSockInit() {
    if (ok_) {
        WSACleanup();
    }
}