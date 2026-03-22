// Stubbed AEC processor – echo cancellation disabled for now.
#include "aec_processor.h"

AecProcessor::AecProcessor(int sample_rate_hz, int frame_samples) {}
AecProcessor::~AecProcessor() = default;
void AecProcessor::set_delay_ms(int delay_ms) {}
bool AecProcessor::process_render(const int16_t* frame, int samples) { return false; }
bool AecProcessor::process_capture(std::vector<int16_t>& frame) { return false; }
