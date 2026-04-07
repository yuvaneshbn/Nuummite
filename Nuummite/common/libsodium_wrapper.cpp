#include "libsodium_wrapper.h"
#include <iostream>
#include <cstring>

HMODULE SodiumWrapper::module_ = nullptr;
bool SodiumWrapper::initialized_ = false;
std::vector<uint8_t> SodiumWrapper::key_;

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

    auto p_sodium_init = (int (*)())GetProcAddress(module_, "sodium_init");
    auto p_crypto_generichash = (int (*)(unsigned char*, size_t, const unsigned char*, unsigned long long, const unsigned char*, size_t))
                                GetProcAddress(module_, "crypto_generichash");

    if (!p_sodium_init || !p_crypto_generichash) {
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
    std::cout << "[SODIUM] Initialized with keyed MAC (anti-spoofing enabled)\n";
    return true;
}

void SodiumWrapper::shutdown() {
    if (module_) FreeLibrary(module_);
    module_ = nullptr;
    initialized_ = false;
    key_.clear();
}

void SodiumWrapper::setKey(const std::string& secret) {
    if (secret.empty()) return;
    auto p_crypto_generichash = (int (*)(unsigned char*, size_t, const unsigned char*, unsigned long long, const unsigned char*, size_t))
                                GetProcAddress(module_, "crypto_generichash");
    if (!p_crypto_generichash) return;

    uint8_t hash[32];
    p_crypto_generichash(hash, 32, (const uint8_t*)secret.data(), secret.size(), nullptr, 0);
    key_.assign(hash, hash + 32);
    std::cout << "[SODIUM] Custom room secret applied\n";
}

bool SodiumWrapper::signPacket(const uint8_t* data, size_t len, uint8_t* signature) {
    if (!initialized_ || key_.empty()) return false;
    auto p = (int (*)(unsigned char*, size_t, const unsigned char*, unsigned long long, const unsigned char*, size_t))
             GetProcAddress(module_, "crypto_generichash");
    return p(signature, 32, data, len, key_.data(), key_.size()) == 0;
}

bool SodiumWrapper::verifyPacket(const uint8_t* data, size_t len, const uint8_t* signature) {
    if (!initialized_ || key_.empty()) return false;
    uint8_t expected[32];
    auto p = (int (*)(unsigned char*, size_t, const unsigned char*, unsigned long long, const unsigned char*, size_t))
             GetProcAddress(module_, "crypto_generichash");
    if (p(expected, 32, data, len, key_.data(), key_.size()) != 0) return false;
    return std::memcmp(expected, signature, 32) == 0;
}
