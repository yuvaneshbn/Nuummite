#ifndef LIBSODIUM_WRAPPER_H
#define LIBSODIUM_WRAPPER_H

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

class SodiumWrapper {
public:
    static bool init();
    static void shutdown();
    static void setKey(const std::string& secret);   // ← new

    static bool signPacket(const uint8_t* data, size_t len, uint8_t* signature);
    static bool verifyPacket(const uint8_t* data, size_t len, const uint8_t* signature);

private:
    static HMODULE module_;
    static bool initialized_;
    static std::vector<uint8_t> key_;   // 32-byte key derived from secret
};

#endif
