#ifndef RNNOISE_PROCESSOR_H
#define RNNOISE_PROCESSOR_H

#include <vector>
#include <cstdint>

extern "C" {
#include <rnnoise.h>
}

class RnNoiseProcessor {
public:
    RnNoiseProcessor();
    ~RnNoiseProcessor();

    void process(std::vector<int16_t>& frame);
    void processBlock(int16_t* samples, int sample_count);
    bool isAvailable() const { return state_ != nullptr; }

private:
    DenoiseState* state_ = nullptr;
};

#endif
