#ifndef AUDIO_PACKET_H
#define AUDIO_PACKET_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class VoicePacketKind {
    ClientAudio,
    MixedAudio,
};

struct VoicePacket {
    VoicePacketKind kind = VoicePacketKind::ClientAudio;
    std::string sender_id;
    uint16_t seq = 0;
    uint32_t timestamp = 0;
    std::vector<uint8_t> payload;
};

std::optional<VoicePacket> parse_voice_packet(const std::vector<uint8_t>& data);

std::vector<uint8_t> build_client_audio_packet(const std::string& client_id,
                                               uint16_t seq,
                                               uint32_t timestamp,
                                               const std::vector<uint8_t>& payload);

std::vector<uint8_t> build_mixed_audio_packet(uint16_t seq,
                                              const std::vector<uint8_t>& payload);

#endif
