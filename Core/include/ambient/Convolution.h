// AmbientSynth -- convolution reverb ("Room"): a third, optional reverb on the far plane that
// plays a real (or generated) impulse response. Uniform partitioned convolution: the input
// is cut into blocks of kBlock samples, each block's spectrum enters a frequency-domain
// delay line, and every output block is the sum over all partitions of (input spectrum x
// impulse-partition spectrum), one inverse FFT per block. Latency is kBlock samples, which
// the far plane does not notice. True-stereo impulses (L and R tails) are kept apart; a mono
// impulse is used for both channels with the two channels' own inputs.
//
// Impulses are double-buffered like textures: the message thread writes the buffer the
// audio thread is not reading, then swaps. Without a file a generated dark hall is loaded
// at prepare(), so the Room works out of the box.
#pragma once
#include "Cosmos.h"   // Fft
#include <atomic>
#include <cstdint>
#include <vector>

namespace ambient {

class Convolver {
public:
    static constexpr int kBlock = 512;

    // maxSeconds bounds the memory (and CPU) an impulse may take; longer files are cut.
    void prepare(double sampleRate, float maxSeconds = 8.0f);
    void reset();
    // Message thread. R may be nullptr (mono impulse). Resampled to the engine rate, energy-normalised.
    void setImpulse(const float* L, const float* R, int n, double impulseSampleRate);
    // A generated dark hall (~4 s, low end ringing longest), for a Room without a file.
    void generateDefault(uint64_t seed, float seconds = 4.0f);
    bool  hasImpulse() const { return active_.load(std::memory_order_acquire) >= 0; }
    float impulseSeconds() const;
    // Wet only, replaces outL/outR. Any n. Audio thread; never allocates.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

private:
    struct Set {
        std::vector<float> re[2], im[2];   // parts x (kBlock + 1) bins per channel
        int    parts = 0;
        double seconds = 0.0;
        bool   stereo = false;
    };
    void analyse(Set& s, const std::vector<float>& L, const std::vector<float>& R, bool stereo);
    void processBlock();

    Set   sets_[2];
    std::atomic<int> active_{ -1 };
    // Begun and finished; see Engine::waitForQuiet for why it takes two of them.
    std::atomic<unsigned long long> blocksBegun_ { 0 }, blocksDone_ { 0 };
    void waitForQuiet();
    Fft   fft_{ 2 * kBlock };
    int   maxParts_ = 0;
    double sr_ = 48000.0;
    // frequency-domain delay line of input spectra (per channel), ring over maxParts_
    std::vector<float> fdlRe_[2], fdlIm_[2];
    int   fdlHead_ = 0, fdlFilled_ = 0;
    // time-domain staging
    std::vector<float> inBuf_[2], tail_[2], outBuf_[2];
    int   inFill_ = 0, outRead_ = 0;
    std::vector<float> workRe_, workIm_, accRe_, accIm_;
};

} // namespace ambient
