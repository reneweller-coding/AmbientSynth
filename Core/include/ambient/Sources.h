// AmbientSynth -- the sound sources per voice: three equal slots.
//
// Source 1 defaults to Additive, which is the voice's strand bank (Voice.h: up to six detuned or
// stacked copies of a 32-partial spectrum, the classic Oscillator); Additive in Source 2 or 3 is
// a single bank of the same spectrum inside the slot. Every slot can otherwise be one of:
//   Wavetable -- not a table of samples but a table of SPECTRA (32 partial amplitudes per
//                frame); the position morphs between frames and the result is rendered by
//                the same rotating-phasor bank as the main oscillator. Alias-free, and every
//                partial keeps its own life (presence, low cut, feedback PM apply the same
//                way). Built-in tables are generated; a user table is analysed from a WAV
//                with 2048-sample frames (the common wavetable layout).
//   FM        -- a two-operator pair (carrier at the slot pitch, modulator at FM Ratio),
//                index limited automatically for high notes.
//   Texture   -- a granular player over a loaded sample (field recording, flute air,
//                metal), grains around Position with a slow wander; Follow = Note pitches
//                the sample to the note (assuming it was recorded at C4).
//   Stretch   -- the same clip read as a continuum instead of as grains: a spectral time
//                stretch (window, transform, keep the magnitudes, random phases, overlap-add)
//                by a factor of 1 to 1000, so a twenty-second recording becomes hours of
//                weather with no grain rhythm and no transient left standing. Pitch is set by
//                resampling BEFORE the stretch, so a note played higher does not get shorter.
//                Loops with a crossfade at the seam, or without one for a clip that is marked
//                seamless in its file name ("_loop").
// Every slot has level, octave, a just ratio to the note and a pan, and goes through the
// voice's filter, envelope and distance like the main bank.
#pragma once
#include "Dsp.h"
#include "Cosmos.h"   // Fft, for the Stretch type
#include <cstdint>
#include <vector>

namespace ambient {

constexpr int kSlots         = 4;
constexpr int kTableFrames   = 64;
constexpr int kTablePartials = 32;
constexpr int kSlotGrains    = 64;   // ceiling; Grains sets how many a slot may use

// Additive sat last so the indices the presets store for the other types stayed what they were;
// Stretch came after it and is appended for the same reason.
enum class SourceType : int { Off = 0, Wavetable, Fm, Texture, Noise, Additive, Stretch, Bow, Spectral };

constexpr int kNumSourceTypes = 9;
// The longest spectral window the Stretch type analyses: 16384 samples, a third of a second at
// 48 kHz. Paulstretch's own default is a quarter of a second, which is where the smooth results
// start; longer windows are smoother still but cost memory in every slot of every voice.
constexpr int kStretchMaxN = 16384;
constexpr int kStretchMinN = 256;
constexpr int kNumTables      = 6;      // Classic, Organ, Vocal, Glass, Metal, User
constexpr int kNumSlotRatios  = 10;
extern const char* const kSourceTypeNames[kNumSourceTypes];
extern const char* const kTableNames[kNumTables];
extern const char* const kSlotRatioNames[kNumSlotRatios];
extern const double      kSlotRatios[kNumSlotRatios];
extern const char* const kFollowNames[2];

// Noise is a source in its own right, not just the Air band: an ambient instrument spends half
// its life in it. Ten colours, from the textbook slopes to the ones that are really textures.
enum class NoiseKind : int {
    White = 0, Pink, Brown, Blue, Violet, Grey, Band, Wind, Crackle, Digital, Count
};
constexpr int kNumNoiseKinds = static_cast<int>(NoiseKind::Count);
extern const char* const kNoiseKindNames[kNumNoiseKinds];

struct Wavetable {
    int   frames = 0;
    float amp[kTableFrames][kTablePartials] = {};
    // Spectrum at position 0..1, 32 amplitudes. Between two frames the amplitudes are blended
    // linearly, which is what every wavetable does and what makes a morph between two formants
    // sound hollow halfway: the old peak fades out, the new one fades in, and between them the
    // energy dips (two half-height peaks hold half the energy of one). With `transport` the
    // blend becomes the displacement interpolation of optimal transport instead -- the
    // one-dimensional Wasserstein barycentre, which along a line is exact and cheap: each
    // frame's amplitudes are read as a distribution of mass over the partials, the two are
    // walked in step by cumulative mass, and every slice of mass is put down at the point
    // between where it sits in one frame and where in the other. Peaks SLIDE from one partial
    // to the next rather than fading, the total mass is the blend of the two totals, and the
    // hollow is gone (Roma, Green and Tremblay; measured in the selftest: the energy halfway
    // doubles, and the spread of the spectrum collapses from five partials to none).
    void spectrumAt(float pos, float* out, float transport = 0.0f) const;
    // Builds a table from raw samples laid out as consecutive single-cycle frames of
    // `frameLen` samples (2048 = Serum/Vital layout). Up to kTableFrames frames are kept
    // (evenly picked when there are more). Returns false if there is not one full frame.
    bool analyse(const float* mono, int n, int frameLen = 2048);
};

const Wavetable& builtinTable(int index);   // 0 .. kNumTables-2 (the last index is the user slot)

// Base pitch from a texture file name: a trailing "_A3" / "-C#4" / " Bb2" note token before
// the extension (as written by Tools/TextureGen) gives the frequency at A4 = 440 Hz; 0 if none.
double baseHzFromName(const char* fileName);
// Whether the file name marks the clip as seamless ("_loop" or "-loop" anywhere before the
// extension, any case): the Stretch type then wraps without a crossfade.
bool loopFromName(const char* fileName);

// What a recording is made of, band by band and frame by frame.
//
// A granular source cuts a clip into pieces and plays the pieces; a Paulstretch smears its
// spectrum. Neither can hold a recording still at one pitch and read it at another speed, because
// both still play samples. This does not play samples at all: the clip is measured once, into
// thirty-two bands on the ear's own frequency scale, and what is stored per frame is how loud each
// band is, where in the band its strongest partial sits, and how tonal it is -- how far above the
// local noise floor its content stands. Playback then builds the sound again from an oscillator
// and a band of noise per band, so pitch and speed are two separate numbers rather than one.
//
// This is the deterministic-plus-stochastic decomposition of Serra and Smith (1990), taken band by
// band instead of partial by partial. Doing it per band means no fundamental has to be found: a
// bell is nearly all oscillator, rain is nearly all noise, a voice is both, and none of the three
// needs a pitch tracker that could be wrong about it.
struct SpectralModel {
    static constexpr int kBands = kTablePartials;   // 32, the same width as the phasor bank
    int   frames = 0;
    float hop = 0.0f;                 // seconds between frames
    float centre[kBands] = {};        // band centre in Hz, and
    float bandQ[kBands] = {};         // centre / width, the Q the noise band is filtered at
    std::vector<float> amp;           // frames * kBands, linear
    std::vector<float> freq;          // frames * kBands, Hz
    std::vector<float> tone;          // frames * kBands, 0 = noise, 1 = a partial
    bool empty() const { return frames < 2; }
};

struct Texture {
    std::vector<float> mono;
    double sampleRate = 48000.0;
    double baseHz = 261.6256;   // assumed pitch of the sample for Follow = Note
    // Reading gain, from the clip's own RMS: the wavetable and FM slots normalise themselves to
    // unity, so without this a quiet recording enters the mix 20 dB below them at the same Level.
    float  gain = 1.0f;
    bool   seamless = false;    // the end runs into the start: no crossfade needed at the seam
    bool empty() const { return mono.size() < 64; }
    // The band model, measured once when the clip is loaded (on the loader thread, never in
    // render()). A clip too short to analyse simply has none, and a Spectral slot on it is silent.
    SpectralModel spectral;
    void measure();             // sets gain from mono, and builds the spectral model
    void analyse();             // the model alone
};

struct SlotParams {
    SourceType type = SourceType::Off;
    float level = 0.5f;
    int   octave = 0;            // -2 .. 2
    int   ratio = 0;             // index into kSlotRatios
    float pan = 0.0f;            // -1 .. 1
    int   table = 0;             // index into kTableNames (last = user)
    float position = 0.0f;       // wavetable frame position / texture position
    float positionDrift = 0.3f;  // how far the position wanders on its own
    float fmRatio = 2.0f;        // modulator / carrier
    float fmIndex = 1.0f;        // modulation index (radians / 2pi at low notes)
    float grainMs = 200.0f;
    float density = 12.0f;       // grains per second
    bool  follow = false;        // texture pitched to the note
    int   grains = 16;           // how many grains this slot may have sounding at once, 1..kSlotGrains
    float spread = 0.03f;        // start-point scatter around Position, as a fraction of the clip
    NoiseKind noise = NoiseKind::Pink;
    float noiseQ = 0.4f;         // width of Band and Wind, 0 = wide open, 1 = a whistle
    // Additive: the spectrum of the slot's own bank (same formula as the voice's strands)
    int   partials = 16;
    float tilt = 1.2f, bright = 0.7f, oddEven = 0.0f, inharm = 0.0f, shimmer = 0.4f, shimmerRate = 0.15f;
    float drift = 0.0f;          // cents of slow, independent pitch drift (the asymmetric detune)
    // Bow: how hard the bow presses and how fast it travels. The string is the slot's pitch.
    float bowForce = 0.4f, bowSpeed = 0.3f;
    // Spectral: how fast the model is read (1 = the speed it was recorded at, 0 = held still),
    // and which half of it is favoured (-1 = the partials only, +1 = the noise only).
    float specRate = 1.0f, specBreath = 0.0f;
    float transport = 0.0f;      // Wavetable: 0 crossfade between frames, 1 slide the spectral mass
    // Stretch: the factor, and the crossfade at the loop seam as a fraction of the clip (ignored
    // for a clip marked seamless). The spectral window is Grain, the read position Position.
    float stretch = 40.0f;
    float xfade = 0.1f;
};

class SourceSlot {
public:
    void prepare(double sampleRate, uint64_t seed);
    void noteOn(bool fresh);
    // Adds n (<= kControlBlock) samples of this slot into outL/outR. Control values are
    // refreshed once per call; level and pan ramp across the block.
    void render(float* outL, float* outR, int n, double noteHz, const SlotParams& p,
                const Wavetable* table, const Texture* texture, float driftRate);

    // For pictures (message thread, torn reads cost a pixel): the bank's partial amplitudes as
    // they are being summed, and the grains that are sounding.
    int displayAmps(float* out, int maxCount) const
    {
        const int n = maxCount < active_ ? maxCount : active_;
        for (int i = 0; i < n; ++i) out[i] = amp_[i];
        return n < 0 ? 0 : n;
    }
    struct GrainInfo { float pos = 0.0f, age = 0.0f, gain = 0.0f, pan = 0.0f; };   // clip position 0..1, age 0..1, level, pan -1..1
    int displayGrains(GrainInfo* out, int maxCount, int clipLen) const;

private:
    void renderWavetable(float* out, int n, double hz, const SlotParams& p, const Wavetable* table, float dt);
    void renderAdditive(float* out, int n, double hz, const SlotParams& p, float dt);
    void renderBank(const float* spec, int H, int n, float* out);   // the phasor bank's block: targets, then the sum
    void renderFm(float* out, int n, double hz, const SlotParams& p, float dt);
    void renderTexture(float* outL, int n, double hz, double speed, const SlotParams& p, const Texture* tex, float dt);
    void renderNoise(float* outL, int n, double hz, const SlotParams& p, float dt);
    void renderStretch(float* out, int n, double hz, double speed, const SlotParams& p, const Texture* tex, float dt);
    void renderBow(float* out, int n, double hz, const SlotParams& p, float dt);
    void renderSpectral(float* out, int n, double transpose, const SlotParams& p, const Texture* tex, float dt);
    void stretchFrame(const SlotParams& p, const Texture* tex, double rate, int N);
    static const Fft& stretchFft(int n);   // shared, read-only after prepare(): one per size

    // Wavetable: phasor bank like Voice::Strand.
    float  pc_[kTablePartials] = {}, ps_[kTablePartials] = {};
    float  rc_[kTablePartials] = {}, rs_[kTablePartials] = {};
    float  amp_[kTablePartials] = {}, ampStep_[kTablePartials] = {};
    int    active_ = 0;
    // Additive: per-partial shimmer and the cached tilt/odd-even shape
    Drifter shim_[kTablePartials];
    float  tiltCache_[kTablePartials] = {};
    float  cTilt_ = -1.0f, cOdd_ = -9.0f;
    int    cPartials_ = -1;
    // FM
    double phC_ = 0.0, phM_ = 0.0;
    float  hpX_ = 0.0f, hpY_ = 0.0f;   // DC blocker state
    // Texture grains
    // The window is a rotating phasor, not a cosine call: at 64 grains a std::cos per sample per
    // grain is the single most expensive thing in the voice.
    struct Grain { double pos = 0.0; double rate = 1.0; int len = 0; int age = 0; float gain = 0.0f;
                   float gl = 0.0f, gr = 0.0f; float wc = 1.0f, ws = 0.0f, rc = 1.0f, rs = 0.0f; bool on = false; };
    Grain  grains_[kSlotGrains];
    double spawnIn_ = 0.0;   // seconds until the next grain
public:
    // Noise: one generator per side, so the two channels are fully decorrelated -- which is what
    // makes a noise bed sit around the listener instead of in the middle of the head.
    struct NoiseState {
        float pink[7] = {};          // Kellet's economy pink filter
        float brown = 0.0f;
        float prev = 0.0f;           // for the differentiated colours
        float bp1 = 0.0f, bp2 = 0.0f;   // state-variable band pass
        float hold = 0.0f;           // sample and hold
        double holdLeft = 0.0;
        double nextGrain = 0.0;      // crackle
        float crackle = 0.0f, crackleDecay = 0.0f;
    };
private:
    NoiseState noise_[2];
    // Stretch: an overlap-add ring twice the longest window, the transform pair, and where the
    // analysis reads in the clip. Allocated in prepare(), never in render().
    // Bow: one period of string, its loop filter's state, and whether it has been started.
    static constexpr int kBowMax = 4096;   // 12 Hz at 48 kHz
    // In vectors, not in the object: two arrays of this size per slot is thirty-two kilobytes,
    // and four slots in each of many voices put an Engine built on the stack straight through it.
    std::vector<float> bowNut_, bowBridge_;
    int   bowW_ = 0;
    float bowLp_ = 0.0f;
    bool  bowReady_ = false;

    // Spectral: the two amplitude ramps per band (the partial and its noise), the state of the
    // band's noise filter, and how far the read head has travelled from Position, in frames.
    float specA_[kTablePartials] = {}, specAStep_[kTablePartials] = {};
    float specN_[kTablePartials] = {}, specNStep_[kTablePartials] = {};
    float specBp_[kTablePartials] = {}, specLp_[kTablePartials] = {};
    float specF_[kTablePartials] = {}, specQ_[kTablePartials] = {};
    double specAdvance_ = 0.0;

    struct StretchState {
        std::vector<float> out, re, im, win;
        int    n = 0;            // the window in use (0 = none yet)
        int    outPos = 0, hopLeft = 0;
        double advance = 0.0;    // how far the read has travelled from Position, in clip samples
    };
    StretchState st_;
    Drifter noiseDrift_;
    Drifter posDrift_, idxDrift_, pitchDrift_;
    Rng    rng_;
    double sr_ = 48000.0;
    float  gL_ = 0.0f, gR_ = 0.0f;
    float  scratch_[64] = {};
    SourceType lastType_ = SourceType::Off;
};

} // namespace ambient
