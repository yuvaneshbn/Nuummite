#ifndef LIBSODIUM_WRAPPER_H
#define LIBSODIUM_WRAPPER_H

#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

class SodiumWrapper {
public:
    static bool init();
    static void shutdown();
    static void setKey(const std::string& passphrase);
    
    static constexpr size_t kNonceSize = 24;
    static constexpr size_t kMacSize   = 16;

    static bool encrypt(const uint8_t* data,
                        size_t len,
                        std::vector<uint8_t>& ciphertext,
                        std::array<uint8_t, kNonceSize>& nonce);
                        
    static bool decrypt(const uint8_t* ciphertext,
                        size_t len,
                        const uint8_t* nonce,
                        size_t nonce_len,
                        std::vector<uint8_t>& plaintext);

private:
    static HMODULE module_;
    static bool initialized_;
    static std::shared_ptr<const std::array<uint8_t, 32>> key_;

    // Static declarations of cryptographic function templates
    typedef int (*crypto_secretbox_easy_fn)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*);
    typedef int (*crypto_secretbox_open_easy_fn)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*);
    typedef void (*randombytes_buf_fn)(void*, size_t);

    // Statically allocated system pointers
    static crypto_secretbox_easy_fn p_encrypt_;
    static crypto_secretbox_open_easy_fn p_decrypt_;
    static randombytes_buf_fn p_random_;

    static std::vector<uint8_t> deriveKey(const std::string& passphrase);
};

#endif // LIBSODIUM_WRAPPER_H
