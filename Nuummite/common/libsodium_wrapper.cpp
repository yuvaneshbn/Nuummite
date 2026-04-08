#include "libsodium_wrapper.h"
#include <iostream>
#include <cstring>
#include <array>

HMODULE SodiumWrapper::module_ = nullptr;
bool SodiumWrapper::initialized_ = false;
std::vector<uint8_t> SodiumWrapper::key_;

namespace {
using sodium_init_fn = int (*)();
using crypto_generichash_fn = int (*)(unsigned char*, size_t, const unsigned char*, unsigned long long, const unsigned char*, size_t);
using crypto_secretbox_easy_fn = int (*)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*);
using crypto_secretbox_open_easy_fn = int (*)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*);
using randombytes_buf_fn = void (*)(void*, size_t);

sodium_init_fn p_sodium_init = nullptr;
crypto_generichash_fn p_crypto_generichash = nullptr;
crypto_secretbox_easy_fn p_crypto_secretbox_easy = nullptr;
crypto_secretbox_open_easy_fn p_crypto_secretbox_open_easy = nullptr;
randombytes_buf_fn p_randombytes_buf = nullptr;
} // namespace

bool SodiumWrapper::init() {
    if (initialized_) return true;

    // Try same locations as before
    const std::vector<std::string> candidates = {
        "libsodium.dll",
        "libsodium.dll",
        "third_party\\libsodium\\libsodium.dll",
        "third_party\\libsodium\\bin\\libsodium-26.dll",
    };

    for (const auto& c : candidates) {
        module_ = LoadLibraryA(c.c_str());
        if (module_) break;
    }

    if (!module_) {
        std::cerr << "[SODIUM] Could not load libsodium.dll\n";
        return false;
    }

    p_sodium_init = (sodium_init_fn)GetProcAddress(module_, "sodium_init");
    p_crypto_generichash = (crypto_generichash_fn)GetProcAddress(module_, "crypto_generichash");
    p_crypto_secretbox_easy = (crypto_secretbox_easy_fn)GetProcAddress(module_, "crypto_secretbox_easy");
    p_crypto_secretbox_open_easy = (crypto_secretbox_open_easy_fn)GetProcAddress(module_, "crypto_secretbox_open_easy");
    p_randombytes_buf = (randombytes_buf_fn)GetProcAddress(module_, "randombytes_buf");

    if (!p_sodium_init || !p_crypto_generichash || !p_crypto_secretbox_easy ||
        !p_crypto_secretbox_open_easy || !p_randombytes_buf) {
        std::cerr << "[SODIUM] Missing symbols\n";
        shutdown();
        return false;
    }

    p_sodium_init();

    // Default key if none set yet
    if (key_.empty()) {
        const char* default_secret = "nuummite_default_secret_2026";
        uint8_t hash[32];
        p_crypto_generichash(hash, 32, (const uint8_t*)default_secret, strlen(default_secret), nullptr, 0);
        key_.assign(hash, hash + 32);
    }

    initialized_ = true;
    std::cout << "[SODIUM] Initialized with secretbox encryption + MAC\n";
    return true;
}

void SodiumWrapper::shutdown() {
    if (module_) FreeLibrary(module_);
    module_ = nullptr;
    initialized_ = false;
    key_.clear();
    p_sodium_init = nullptr;
    p_crypto_generichash = nullptr;
    p_crypto_secretbox_easy = nullptr;
    p_crypto_secretbox_open_easy = nullptr;
    p_randombytes_buf = nullptr;
}

void SodiumWrapper::setKey(const std::string& secret) {
    if (secret.empty() || !initialized_ || !p_crypto_generichash) return;

    uint8_t hash[32];
    p_crypto_generichash(hash, 32, (const uint8_t*)secret.data(), secret.size(), nullptr, 0);
    key_.assign(hash, hash + 32);
    std::cout << "[SODIUM] Custom room secret applied\n";
}

bool SodiumWrapper::encrypt(const uint8_t* data,
                            size_t len,
                            std::vector<uint8_t>& ciphertext,
                            std::array<uint8_t, kNonceSize>& nonce) {
    if (!initialized_ || key_.size() != 32 || !p_crypto_secretbox_easy || !p_randombytes_buf) {
        return false;
    }
    p_randombytes_buf(nonce.data(), nonce.size());
    ciphertext.resize(len + kMacSize);
    const int rc = p_crypto_secretbox_easy(ciphertext.data(), data, static_cast<unsigned long long>(len),
                                           nonce.data(), key_.data());
    return rc == 0;
}

bool SodiumWrapper::decrypt(const uint8_t* ciphertext,
                            size_t len,
                            const uint8_t* nonce,
                            size_t nonce_len,
                            std::vector<uint8_t>& plaintext) {
    if (!initialized_ || key_.size() != 32 || !p_crypto_secretbox_open_easy) {
        return false;
    }
    if (nonce_len != kNonceSize || len < kMacSize) {
        return false;
    }
    plaintext.resize(len - kMacSize);
    const int rc = p_crypto_secretbox_open_easy(plaintext.data(), ciphertext,
                                                static_cast<unsigned long long>(len),
                                                nonce, key_.data());
    if (rc != 0) {
        plaintext.clear();
        return false;
    }
    return true;
}

bool SodiumWrapper::signPacket(const uint8_t* data, size_t len, uint8_t* signature) {
    if (!initialized_ || key_.empty() || !p_crypto_generichash) return false;
    return p_crypto_generichash(signature, 32, data, len, key_.data(), key_.size()) == 0;
}

bool SodiumWrapper::verifyPacket(const uint8_t* data, size_t len, const uint8_t* signature) {
    if (!initialized_ || key_.empty() || !p_crypto_generichash) return false;
    uint8_t expected[32];
    if (p_crypto_generichash(expected, 32, data, len, key_.data(), key_.size()) != 0) return false;
    return std::memcmp(expected, signature, 32) == 0;
}
