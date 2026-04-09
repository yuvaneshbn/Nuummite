#include "libsodium_wrapper.h"
#include <iostream>
#include <cstring>

HMODULE SodiumWrapper::module_ = nullptr;
bool SodiumWrapper::initialized_ = false;
std::vector<uint8_t> SodiumWrapper::key_;

bool SodiumWrapper::init() {
    if (initialized_) return true;

    const std::vector<std::string> candidates = {
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

    auto p_init = reinterpret_cast<int (*)()>(GetProcAddress(module_, "sodium_init"));
    auto p_encrypt = reinterpret_cast<int (*)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*)>(
        GetProcAddress(module_, "crypto_secretbox_easy"));
    auto p_decrypt = reinterpret_cast<int (*)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*)>(
        GetProcAddress(module_, "crypto_secretbox_open_easy"));
    auto p_random = reinterpret_cast<void (*)(void*, size_t)>(GetProcAddress(module_, "randombytes_buf"));

    if (!p_init || !p_encrypt || !p_decrypt || !p_random) {
        std::cerr << "[SODIUM] Missing symbols\n";
        shutdown();
        return false;
    }

    p_init();
    initialized_ = true;
    std::cout << "[SODIUM] Ready (secretbox encryption)\n";
    return true;
}

void SodiumWrapper::shutdown() {
    if (module_) FreeLibrary(module_);
    module_ = nullptr;
    initialized_ = false;
    key_.clear();
}

std::vector<uint8_t> SodiumWrapper::deriveKey(const std::string& room_name) {
    const char* salt = "NuummiteLANVoice2026SecureSalt";
    std::string input = room_name + salt;

    auto p_hash = reinterpret_cast<int (*)(unsigned char*, size_t, const unsigned char*, unsigned long long, const unsigned char*, size_t)>(
        GetProcAddress(module_, "crypto_generichash"));
    if (!p_hash) {
        std::cerr << "[SODIUM] Missing crypto_generichash\n";
        return {};
    }

    uint8_t hash1[32];
    p_hash(hash1, 32, reinterpret_cast<const uint8_t*>(input.data()), input.size(), nullptr, 0);
    uint8_t final_key[32];
    p_hash(final_key, 32, hash1, 32, nullptr, 0);

    return std::vector<uint8_t>(final_key, final_key + 32);
}

void SodiumWrapper::setKey(const std::string& room_name) {
    if (room_name.empty()) return;
    if (!initialized_ && !init()) return;
    key_ = deriveKey(room_name);
    if (!key_.empty()) {
        std::cout << "[SODIUM] Key derived from room: " << room_name << "\n";
    }
}

bool SodiumWrapper::encrypt(const uint8_t* data,
                            size_t len,
                            std::vector<uint8_t>& ciphertext,
                            std::array<uint8_t, kNonceSize>& nonce) {
    if (!initialized_ || key_.size() != 32) {
        return false;
    }

    auto p_random = reinterpret_cast<void (*)(void*, size_t)>(GetProcAddress(module_, "randombytes_buf"));
    auto p_encrypt = reinterpret_cast<int (*)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*)>(
        GetProcAddress(module_, "crypto_secretbox_easy"));
    if (!p_random || !p_encrypt) return false;

    p_random(nonce.data(), nonce.size());
    ciphertext.resize(len + kMacSize);

    return p_encrypt(ciphertext.data(), data, static_cast<unsigned long long>(len), nonce.data(), key_.data()) == 0;
}

bool SodiumWrapper::decrypt(const uint8_t* ciphertext,
                            size_t len,
                            const uint8_t* nonce,
                            size_t nonce_len,
                            std::vector<uint8_t>& plaintext) {
    if (!initialized_ || key_.size() != 32 || nonce_len != kNonceSize || len < kMacSize) {
        return false;
    }

    auto p_decrypt = reinterpret_cast<int (*)(unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*)>(
        GetProcAddress(module_, "crypto_secretbox_open_easy"));
    if (!p_decrypt) return false;

    plaintext.resize(len - kMacSize);
    int rc = p_decrypt(plaintext.data(), ciphertext, static_cast<unsigned long long>(len), nonce, key_.data());
    if (rc != 0) {
        plaintext.clear();
        return false;
    }
    return true;
}
