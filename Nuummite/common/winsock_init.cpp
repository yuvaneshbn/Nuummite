#include "winsock_init.h"
#include <objbase.h>

WinSockInit::WinSockInit() {
    const int rc = WSAStartup(MAKEWORD(2, 2), &wsa_);
    ok_ = (rc == 0);

    if (ok_) {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == S_OK || hr == S_FALSE) {
            com_initialized_ = true;
        }
    }
}

WinSockInit::~WinSockInit() {
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
    if (ok_) {
        WSACleanup();
    }
}
