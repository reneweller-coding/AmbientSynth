// AmbientSynth -- loudness to ITU-R BS.1770-4 / EBU R 128, and the peak numbers that go with it.
//
// Ambient is the one genre where loudness is the enemy rather than the goal: a brickwall limiter
// takes the finest amplitude movement out of a reverb tail and leaves it grainy and flat, and the
// sense of an enormous room comes from the distance between the quietest texture and the loudest
// swell, not from the average level. A master that has been squeezed to -9 LUFS has thrown that
// distance away and cannot get it back. So this instrument, which can render a finished piece,
// says what the piece measures: integrated and short-term loudness, the crest factor, and the
// true peak between the samples.
//
// It runs in the engine rather than in the editor, because the integrated figure is a number
// about the whole piece: a meter that only sees what the interface happened to ask for is a
// meter with holes in it.
#pragma once
#include <cstddef>
#include <vector>

namespace ambient {

struct LoudnessReading {
    float momentary  = -120.0f;   // LUFS over the last 400 ms
    float shortTerm  = -120.0f;   // LUFS over the last 3 s
    float integrated = -120.0f;   // LUFS, gated, since the last reset
    float range      = 0.0f;      // LU: the spread of the short-term values (10th to 95th centile)
    float truePeak   = -120.0f;   // dBTP, 4x oversampled, since the last reset
    float crest      = 0.0f;      // dB: true peak over the short-term loudness
    float seconds    = 0.0f;      // how long it has been measuring
};

// K-weighting: a high shelf and a high-pass, the two stages of BS.1770. The coefficients are
// derived from the analogue prototypes the standard names, so they are right at any sample rate
// rather than only at 48 kHz.
class KFilter {
public:
    void prepare(double sampleRate);
    void reset();
    float process(float x);
private:
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float z1 = 0, z2 = 0;
        float process(float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void reset() { z1 = z2 = 0.0f; }
    };
    Biquad shelf_, hp_;
};

class LoudnessMeter {
public:
    void prepare(double sampleRate);
    void reset();
    // The finished output, after the master stage: what would be written to the file.
    void process(const float* L, const float* R, int n);
    LoudnessReading read() const;
private:
    void pushBlock();

    double sr_ = 48000.0;
    KFilter kL_, kR_;
    // One 400 ms block, taken every 100 ms: the 75 % overlap the standard's gating is defined on.
    int    blockLen_ = 19200, hopLen_ = 4800, hopPos_ = 0;
    std::vector<double> sumL_, sumR_;   // ring of per-hop mean squares, four hops to a block
    int    ringPos_ = 0, ringFilled_ = 0;
    static constexpr int kHopsPerBlock = 4;
    static constexpr int kHopsPerShort = 30;      // 3 s
    double hopL_ = 0.0, hopR_ = 0.0;
    long   hopSamples_ = 0;
    // Gating needs every block's loudness, not a running mean: the relative gate is a threshold
    // computed from all of them and then applied to all of them.
    std::vector<float> blocks_;                   // block loudness in LUFS
    std::vector<float> shortBlocks_;              // short-term values, for the range
    double truePeak_ = 0.0;
    double seconds_ = 0.0;
    float  lastShort_ = -120.0f;
    // True peak: four-times oversampling with a short windowed-sinc, which is what "inter-sample"
    // means in practice -- a signal at 0 dBFS on every sample can reach well above it between them.
    float  tpHistL_[4] = {}, tpHistR_[4] = {};
};

}   // namespace ambient
