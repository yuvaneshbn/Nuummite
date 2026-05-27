#include "libsodium_wrapper.h"
#include <iostream>
#include <cstring>
#include <vector>

namespace {

HMODULE SecureLoadDynamicLibrary(const std::string& full_path) {
    const DWORD flags =
        LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
        LOAD_LIBRARY_SEARCH_SYSTEM32 |
        LOAD_LIBRARY_SEARCH_USER_DIRS |
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;

    HMODULE h_module = LoadLibraryExA(full_path.c_str(), nullptr, flags);
    if (!h_module) {
        // Fallback for older systems that don't support LOAD_LIBRARY_SEARCH_* flags.
        // This still uses an explicit path (no insecure directory searching for the DLL itself).
        const DWORD err = GetLastError();
        if (err == ERROR_INVALID_PARAMETER) {
            h_module = LoadLibraryA(full_path.c_str());
        }
    }

    if (!h_module) {
        std::cerr << " Blocked unsecure DLL search path for: " << full_path << "\n";
    }
    return h_module;
}

HMODULE SecureLoadDynamicLibraryByName(const char* dll_name) {
    const DWORD flags =
        LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
        LOAD_LIBRARY_SEARCH_SYSTEM32 |
        LOAD_LIBRARY_SEARCH_USER_DIRS;

    HMODULE h_module = LoadLibraryExA(dll_name, nullptr, flags);
    if (!h_module) {
        const DWORD err = GetLastError();
        if (err == ERROR_INVALID_PARAMETER) {
            // Without safe search flags we do not attempt a name-only load.
            return nullptr;
        }
    }
    return h_module;
}

std::string exeDir() {
    char path[MAX_PATH] = {0};
    const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return {};
    }
    std::string full(path, path + len);
    const size_t slash = full.find_last_of("\\/");
    if (slash == std::string::npos) {
        return {};
    }
    return full.substr(0, slash);
}

void addCandidates(std::vector<std::string>& out, const std::string& base) {
    if (base.empty()) {
        return;
    }

    // Restrict resolution to the executable's install directory tree.
    // Primary: DLLs placed next to the executable.
    out.push_back(base + "\\libsodium-26.dll");
    out.push_back(base + "\\libsodium.dll");

    // PyInstaller-style extraction targets (still under exeDir()).
    out.push_back(base + "\\_internal\\libsodium-26.dll");
    out.push_back(base + "\\_internal\\libsodium.dll");
    out.push_back(base + "\\_internal\\third_party\\libsodium\\bin\\libsodium-26.dll");
    out.push_back(base + "\\_internal\\third_party\\libsodium\\libsodium.dll");

    // Optional embedded third_party layout under the install directory.
    out.push_back(base + "\\third_party\\libsodium\\bin\\libsodium-26.dll");
    out.push_back(base + "\\third_party\\libsodium\\libsodium.dll");
}

} // namespace

HMODULE SodiumWrapper::module_ = nullptr;
bool SodiumWrapper::initialized_ = false;
std::vector<uint8_t> SodiumWrapper::key_;

SodiumWrapper::crypto_secretbox_easy_fn SodiumWrapper::p_encrypt_ = nullptr;
SodiumWrapper::crypto_secretbox_open_easy_fn SodiumWrapper::p_decrypt_ = nullptr;
SodiumWrapper::randombytes_buf_fn SodiumWrapper::p_random_ = nullptr;

bool SodiumWrapper::init() {
    if (initialized_) return true;

    std::vector<std::string> candidates;
    candidates.reserve(24);

    const std::string secure_dir = exeDir();
    addCandidates(candidates, secure_dir);
     
    for (const auto& c : candidates) {
        module_ = SecureLoadDynamicLibrary(c);
        if (module_) break;
    }

    if (!module_) {
        // Safe fallback for host processes (e.g. python.exe) that configure DLL roots via AddDllDirectory().
        module_ = SecureLoadDynamicLibraryByName("libsodium-26.dll");
        if (!module_) {
            module_ = SecureLoadDynamicLibraryByName("libsodium.dll");
        }
    }

    if (!module_) {
        std::cerr << " Secure libsodium initialization failed.\n";
        return false;
    }

    auto p_init = reinterpret_cast<int (*)()>(GetProcAddress(module_, "sodium_init"));
    p_encrypt_ = reinterpret_cast<crypto_secretbox_easy_fn>(GetProcAddress(module_, "crypto_secretbox_easy"));
    p_decrypt_ = reinterpret_cast<crypto_secretbox_open_easy_fn>(GetProcAddress(module_, "crypto_secretbox_open_easy"));
    p_random_ = reinterpret_cast<randombytes_buf_fn>(GetProcAddress(module_, "randombytes_buf"));

    if (!p_init ||!p_encrypt_ ||!p_decrypt_ ||!p_random_) {
        std::cerr << " Failed to resolve target dynamic entries\n";
        shutdown();
        return false;
    }

    p_init();
    initialized_ = true;
    std::cout << " API resolved successfully (procedures cached)\n";
    return true;
}

void SodiumWrapper::shutdown() {
    if (module_) FreeLibrary(module_);
    module_ = nullptr;
    initialized_ = false;
    key_.clear();
    p_encrypt_ = nullptr;
    p_decrypt_ = nullptr;
    p_random_ = nullptr;
}

std::vector<uint8_t> SodiumWrapper::deriveKey(const std::string& passphrase) {
    if (passphrase.empty() ||!initialized_) {
        return {};
    }

    auto p_pwhash = reinterpret_cast<int (*)(unsigned char*, unsigned long long, const char*, unsigned long long, const unsigned char*, unsigned long long, size_t, int)>(
        GetProcAddress(module_, "crypto_pwhash"));
    auto p_alg_default = reinterpret_cast<int (*)()>(GetProcAddress(module_, "crypto_pwhash_alg_default"));
    auto p_opslimit_interactive = reinterpret_cast<size_t (*)()>(GetProcAddress(module_, "crypto_pwhash_opslimit_interactive"));
    auto p_memlimit_interactive = reinterpret_cast<size_t (*)()>(GetProcAddress(module_, "crypto_pwhash_memlimit_interactive"));

    if (p_pwhash && p_alg_default && p_opslimit_interactive && p_memlimit_interactive) {
        static const unsigned char salt[16] = {
            'N','u','u','m','m','i','t','e','V','o','i','c','e','K','D','F'
        };
        std::vector<uint8_t> out(32, 0);
        const unsigned long long ops = static_cast<unsigned long long>(p_opslimit_interactive());
        const size_t mem = p_memlimit_interactive();
        const int alg = p_alg_default();

        const int rc = p_pwhash(out.data(),
                                static_cast<unsigned long long>(out.size()),
                                passphrase.c_str(),
                                static_cast<unsigned long long>(passphrase.size()),
                                salt, ops, mem, alg);
        if (rc == 0) {
            return out;
        }
    }

    auto p_hash = reinterpret_cast<int (*)(unsigned char*, size_t, const unsigned char*, unsigned long long, const unsigned char*, size_t)>(
        GetProcAddress(module_, "crypto_generichash"));
    if (!p_hash) {
        std::cerr << " Fallback generation missing\n";
        return {};
    }

    const char* domain = "Nuummite:secretbox:key:v1";
    std::string input = passphrase + "|" + domain;

    uint8_t final_key[32] = {0};
    p_hash(final_key, 32, reinterpret_cast<const unsigned char*>(input.data()), static_cast<unsigned long long>(input.size()), nullptr, 0);

    std::vector<uint8_t> out(final_key, final_key + 32);
    std::memset(final_key, 0, sizeof(final_key));
    return out;
}

void SodiumWrapper::setKey(const std::string& passphrase) {
    if (passphrase.empty()) return;
    if (!initialized_ &&!init()) return;
    key_ = deriveKey(passphrase);
    if (!key_.empty()) {
        std::cout << " Symmetric key derived\n";
    }
}

bool SodiumWrapper::encrypt(const uint8_t* data,
                            size_t len,
                            std::vector<uint8_t>& ciphertext,
                            std::array<uint8_t, kNonceSize>& nonce) {
    if (!initialized_ || key_.size()!= 32 ||!p_random_ ||!p_encrypt_) {
        return false;
    }

    p_random_(nonce.data(), nonce.size());
    ciphertext.resize(len + kMacSize);
    return p_encrypt_(ciphertext.data(), data, static_cast<unsigned long long>(len), nonce.data(), key_.data()) == 0;
}

bool SodiumWrapper::decrypt(const uint8_t* ciphertext,
                            size_t len,
                            const uint8_t* nonce,
                            size_t nonce_len,
                            std::vector<uint8_t>& plaintext) {
    if (!initialized_ || key_.size()!= 32 || nonce_len!= kNonceSize || len < kMacSize ||!p_decrypt_) {
        return false;
    }

    plaintext.resize(len - kMacSize);
    int rc = p_decrypt_(plaintext.data(), ciphertext, static_cast<unsigned long long>(len), nonce, key_.data());
    if (rc!= 0) {
        plaintext.clear();
        return false;
    }
    return true;
}
