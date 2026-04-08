#ifndef LIBSODIUM_WRAPPER_H
#define LIBSODIUM_WRAPPER_H

#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>

class SodiumWrapper {
public:
    static bool init();
    static void shutdown();
    static void setKey(const std::string& secret);   // ← new

    // Secretbox (XSalsa20-Poly1305) helpers
    static constexpr size_t kNonceSize = 24;  // crypto_secretbox_NONCEBYTES
    static constexpr size_t kMacSize   = 16;  // crypto_secretbox_MACBYTES

    static bool encrypt(const uint8_t* data,
                        size_t len,
                        std::vector<uint8_t>& ciphertext,
                        std::array<uint8_t, kNonceSize>& nonce);

    static bool decrypt(const uint8_t* ciphertext,
                        size_t len,
                        const uint8_t* nonce,
                        size_t nonce_len,
                        std::vector<uint8_t>& plaintext);

    static bool signPacket(const uint8_t* data, size_t len, uint8_t* signature);
    static bool verifyPacket(const uint8_t* data, size_t len, const uint8_t* signature);

private:
    static HMODULE module_;
    static bool initialized_;
    static std::vector<uint8_t> key_;   // 32-byte key derived from secret
};

#endif
