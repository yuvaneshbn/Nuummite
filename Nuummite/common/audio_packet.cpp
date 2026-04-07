#include "audio_packet.h"
#include "libsodium_wrapper.h"

#include <charconv>
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
    if (data.empty()) {
        return std::nullopt;
    }

    const std::string_view view(reinterpret_cast<const char*>(data.data()), data.size());
    VoicePacket packet;

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

    // Verify signature
    constexpr size_t SIG_SIZE = 32;
    if (data.size() < colon + 1 + SIG_SIZE) {
        return std::nullopt;
    }

    const uint8_t* signature = data.data() + colon + 1;
    const uint8_t* payload_start = signature + SIG_SIZE;
    size_t payload_len = data.size() - (colon + 1 + SIG_SIZE);

    // We verify the hash of (header + payload)
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

std::vector<uint8_t> build_client_audio_packet(const std::string& client_id,
                                               uint16_t seq,
                                               uint32_t timestamp,
                                               const std::vector<uint8_t>& payload) {
    std::string header = client_id + "|" + std::to_string(seq) + "|" + std::to_string(timestamp);
    
    // Compute signature (hash of header + payload)
    std::vector<uint8_t> signed_content;
    signed_content.insert(signed_content.end(), header.begin(), header.end());
    signed_content.insert(signed_content.end(), payload.begin(), payload.end());
    
    uint8_t signature[32];
    if (!SodiumWrapper::signPacket(signed_content.data(), signed_content.size(), signature)) {
        std::memset(signature, 0, 32);
    }

    std::vector<uint8_t> packet;
    packet.reserve(header.size() + 1 + 32 + payload.size());
    packet.insert(packet.end(), header.begin(), header.end());
    packet.push_back(':');
    packet.insert(packet.end(), signature, signature + 32);
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}

std::vector<uint8_t> build_mixed_audio_packet(uint16_t seq,
                                              const std::vector<uint8_t>& payload) {
    std::string header = "MIXED|" + std::to_string(seq) + "|";
    std::vector<uint8_t> packet;
    packet.reserve(header.size() + payload.size());
    packet.insert(packet.end(), header.begin(), header.end());
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}
