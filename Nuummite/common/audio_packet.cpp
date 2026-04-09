#include "audio_packet.h"
#include "libsodium_wrapper.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <string_view>

namespace {

template <typename T>
bool parseUnsigned(std::string_view text, T& out) {
    if (text.empty()) {
        return false;
    }

    T value = 0;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc() || result.ptr != end) {
        return false;
    }

    out = value;
    return true;
}

} // namespace

std::optional<VoicePacket> parse_voice_packet(const std::vector<uint8_t>& data) {
    if (data.size() < SodiumWrapper::kNonceSize + SodiumWrapper::kMacSize) {
        return std::nullopt;
    }

    const uint8_t* nonce = data.data();
    const uint8_t* cipher = data.data() + SodiumWrapper::kNonceSize;
    const size_t cipher_len = data.size() - SodiumWrapper::kNonceSize;

    std::vector<uint8_t> plain;
    if (!SodiumWrapper::decrypt(cipher, cipher_len, nonce, SodiumWrapper::kNonceSize, plain)) {
        return std::nullopt;
    }

    const std::string_view view(reinterpret_cast<const char*>(plain.data()), plain.size());
    VoicePacket packet;

    // Mixed audio packets: MIXED|<seq>|<payload>
    if (view.rfind("MIXED|", 0) == 0) {
        const size_t seq_end = view.find('|', 6);
        if (seq_end == std::string_view::npos) {
            return std::nullopt;
        }

        unsigned long seq = 0;
        if (!parseUnsigned(view.substr(6, seq_end - 6), seq)) {
            return std::nullopt;
        }

        packet.kind = VoicePacketKind::MixedAudio;
        packet.seq = static_cast<uint16_t>(seq & 0xFFFFu);
        packet.payload.assign(plain.begin() + static_cast<long long>(seq_end + 1), plain.end());
        return packet;
    }

    const size_t colon = view.find(':');
    if (colon == std::string_view::npos) {
        return std::nullopt;
    }

    const std::string_view header = view.substr(0, colon);
    const size_t first_pipe = header.find('|');
    if (first_pipe == std::string_view::npos) {
        return std::nullopt;
    }

    const size_t second_pipe = header.find('|', first_pipe + 1);
    if (second_pipe == std::string_view::npos) {
        return std::nullopt;
    }

    packet.sender_id.assign(header.substr(0, first_pipe));
    if (packet.sender_id.empty()) {
        return std::nullopt;
    }

    unsigned long seq = 0;
    unsigned long timestamp = 0;
    if (!parseUnsigned(header.substr(first_pipe + 1, second_pipe - first_pipe - 1), seq) ||
        !parseUnsigned(header.substr(second_pipe + 1), timestamp)) {
        return std::nullopt;
    }

    packet.kind = VoicePacketKind::ClientAudio;
    packet.seq = static_cast<uint16_t>(seq & 0xFFFFu);
    packet.timestamp = static_cast<uint32_t>(timestamp & 0xFFFFFFFFu);

    packet.payload.assign(plain.begin() + static_cast<long long>(colon + 1), plain.end());
    return packet;
}

std::vector<uint8_t> build_client_audio_packet(const std::string& client_id,
                                               uint16_t seq,
                                               uint32_t timestamp,
                                               const std::vector<uint8_t>& payload) {
    std::string header = client_id + "|" + std::to_string(seq) + "|" + std::to_string(timestamp);

    std::vector<uint8_t> plain;
    plain.reserve(header.size() + 1 + payload.size());
    plain.insert(plain.end(), header.begin(), header.end());
    plain.push_back(':');
    plain.insert(plain.end(), payload.begin(), payload.end());

    std::array<uint8_t, SodiumWrapper::kNonceSize> nonce{};
    std::vector<uint8_t> ciphertext;
    if (!SodiumWrapper::encrypt(plain.data(), plain.size(), ciphertext, nonce)) {
        return {};
    }

    std::vector<uint8_t> packet;
    packet.reserve(nonce.size() + ciphertext.size());
    packet.insert(packet.end(), nonce.begin(), nonce.end());
    packet.insert(packet.end(), ciphertext.begin(), ciphertext.end());
    return packet;
}

std::vector<uint8_t> build_mixed_audio_packet(uint16_t seq,
                                               const std::vector<uint8_t>& payload) {
    std::string header = "MIXED|" + std::to_string(seq) + "|";

    std::vector<uint8_t> plain;
    plain.reserve(header.size() + payload.size());
    plain.insert(plain.end(), header.begin(), header.end());
    plain.insert(plain.end(), payload.begin(), payload.end());

    std::array<uint8_t, SodiumWrapper::kNonceSize> nonce{};
    std::vector<uint8_t> ciphertext;
    if (!SodiumWrapper::encrypt(plain.data(), plain.size(), ciphertext, nonce)) {
        return {};
    }

    std::vector<uint8_t> packet;
    packet.reserve(nonce.size() + ciphertext.size());
    packet.insert(packet.end(), nonce.begin(), nonce.end());
    packet.insert(packet.end(), ciphertext.begin(), ciphertext.end());
    return packet;
}
