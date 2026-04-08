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

constexpr uint8_t MAGIC[] = {'E', 'N', 'C', '1'};
constexpr size_t MAGIC_SIZE = 4;
constexpr size_t SIG_SIZE = 32; // legacy MAC size

std::optional<VoicePacket> parse_plain_packet(const std::vector<uint8_t>& data, bool expect_signature) {
    if (data.empty()) {
        return std::nullopt;
    }

    const std::string_view view(reinterpret_cast<const char*>(data.data()), data.size());
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
        packet.payload.assign(data.begin() + static_cast<long long>(seq_end + 1), data.end());
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

    size_t payload_offset = colon + 1;
    if (expect_signature) {
        if (data.size() < payload_offset + SIG_SIZE) {
            return std::nullopt;
        }
        const uint8_t* signature = data.data() + payload_offset;
        payload_offset += SIG_SIZE;

        const uint8_t* payload_start = data.data() + payload_offset;
        const size_t payload_len = data.size() - payload_offset;

        // Verify hash of (header + payload) with legacy MAC
        std::vector<uint8_t> signed_content;
        const uint8_t* header_begin = reinterpret_cast<const uint8_t*>(view.data());
        signed_content.insert(signed_content.end(), header_begin, header_begin + colon); // header
        signed_content.insert(signed_content.end(), payload_start, payload_start + payload_len); // payload

        if (!SodiumWrapper::verifyPacket(signed_content.data(), signed_content.size(), signature)) {
            return std::nullopt;
        }
        packet.payload.assign(payload_start, payload_start + payload_len);
        return packet;
    }

    // Encrypted path: payload starts immediately after colon
    if (payload_offset > data.size()) {
        return std::nullopt;
    }
    packet.payload.assign(data.begin() + static_cast<long long>(payload_offset), data.end());
    return packet;
}

} // namespace

std::optional<VoicePacket> parse_voice_packet(const std::vector<uint8_t>& data) {
    if (data.size() >= MAGIC_SIZE &&
        std::equal(std::begin(MAGIC), std::end(MAGIC), data.begin())) {
        if (data.size() < MAGIC_SIZE + SodiumWrapper::kNonceSize + SodiumWrapper::kMacSize) {
            return std::nullopt;
        }

        const uint8_t* nonce = data.data() + MAGIC_SIZE;
        const uint8_t* cipher = nonce + SodiumWrapper::kNonceSize;
        const size_t cipher_len = data.size() - MAGIC_SIZE - SodiumWrapper::kNonceSize;

        std::vector<uint8_t> plain;
        if (!SodiumWrapper::decrypt(cipher, cipher_len, nonce, SodiumWrapper::kNonceSize, plain)) {
            return std::nullopt;
        }
        return parse_plain_packet(plain, /*expect_signature=*/false);
    }

    // Legacy unsigned/plaintext path (kept for compatibility until all peers upgrade)
    return parse_plain_packet(data, /*expect_signature=*/true);
}

std::vector<uint8_t> build_client_audio_packet(const std::string& client_id,
                                               uint16_t seq,
                                               uint32_t timestamp,
                                               const std::vector<uint8_t>& payload) {
    std::string header = client_id + "|" + std::to_string(seq) + "|" + std::to_string(timestamp);

    // Plaintext (header + ':' + payload) gets encrypted with secretbox
    std::vector<uint8_t> plain;
    plain.reserve(header.size() + 1 + payload.size());
    plain.insert(plain.end(), header.begin(), header.end());
    plain.push_back(':');
    plain.insert(plain.end(), payload.begin(), payload.end());

    std::array<uint8_t, SodiumWrapper::kNonceSize> nonce{};
    std::vector<uint8_t> cipher;
    if (!SodiumWrapper::encrypt(plain.data(), plain.size(), cipher, nonce)) {
        return {};
    }

    std::vector<uint8_t> packet;
    packet.reserve(MAGIC_SIZE + nonce.size() + cipher.size());
    packet.insert(packet.end(), std::begin(MAGIC), std::end(MAGIC));
    packet.insert(packet.end(), nonce.begin(), nonce.end());
    packet.insert(packet.end(), cipher.begin(), cipher.end());
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
    std::vector<uint8_t> cipher;
    if (!SodiumWrapper::encrypt(plain.data(), plain.size(), cipher, nonce)) {
        return {};
    }

    std::vector<uint8_t> packet;
    packet.reserve(MAGIC_SIZE + nonce.size() + cipher.size());
    packet.insert(packet.end(), std::begin(MAGIC), std::end(MAGIC));
    packet.insert(packet.end(), nonce.begin(), nonce.end());
    packet.insert(packet.end(), cipher.begin(), cipher.end());
    return packet;
}
