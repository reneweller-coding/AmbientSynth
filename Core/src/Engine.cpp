#include "ambient/Engine.h"
#include <cmath>
#include <cstring>
#include <algorithm>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
  #include <xmmintrin.h>
  #include <pmmintrin.h>
  #define AMBIENT_HAS_MXCSR 1
#else
  #define AMBIENT_HAS_MXCSR 0
#endif

namespace ambient {

Engine::Engine()
{
    for (int i = 0; i < kNumParams; ++i) params_[i].store(paramTable()[static_cast<size_t>(i)].def, std::memory_order_relaxed);
    for (int i = 0; i < kNumScaleChoices - 1; ++i) makeBuiltinScale(i, scales_[i]);
    makeBuiltinScale(0, scales_[kNumScaleChoices - 1]);
    std::strncpy(scales_[kNumScaleChoices - 1].name, "User (Scala)", sizeof(FixedScale::name) - 1);
    scale_ = &scales_[3];
    masterSmooth_.snap(dbToGain(-6.0f));
}

void Engine::prepare(double sampleRate, int maxBlockSize)
{
    sr_ = sampleRate;
    maxBlock_ = std::max(maxBlockSize, kControlBlock);
    busL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    busR_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    seed_ = static_cast<int>(getParam(ParamId::Seed));
    rng_.seed(static_cast<uint64_t>(seed_) + 1);
    for (auto& v : voices_) v.prepare(sr_, rng_.fork());
    ensemble_.prepare(sr_);
    reverb_.prepare(sr_);
    masterSmooth_.setTime(0.02f, sr_);
    lastRootPc_ = -1;
    readParams();
    brain_.reset(rng_.fork(), rootNote_);
    for (auto& h : midiHeld_) h = false;
}

void Engine::reset()
{
    for (auto& v : voices_) v.kill();
    for (auto& h : midiHeld_) h = false;
    brain_.reset(rng_.fork(), rootNote_);
}

void Engine::setUserScale(const FixedScale& s)
{
    while (userBusy_.load(std::memory_order_acquire)) { /* audio thread is copying, microseconds */ }
    userPending_ = s;
    userVersion_.fetch_add(1, std::memory_order_release);
}

double Engine::frequencyOf(int note) const
{
    return scaleFrequency(*scale_, note, rootNote_, refPitch_, snapKeys_);
}

void Engine::soundingNotes(bool (&out)[128]) const
{
    const uint64_t m0 = mask_[0].load(std::memory_order_relaxed), m1 = mask_[1].load(std::memory_order_relaxed);
    for (int i = 0; i < 64; ++i)  out[i] = (m0 >> i) & 1u;
    for (int i = 0; i < 64; ++i)  out[64 + i] = (m1 >> i) & 1u;
}

// ---------------------------------------------------------------- notes

Voice* Engine::allocate(int note, int owner)
{
    for (auto& v : voices_) if (v.isActive() && v.note() == note && v.owner() == owner) return &v;
    for (auto& v : voices_) if (!v.isActive()) return &v;
    Voice* best = nullptr;
    for (auto& v : voices_) if (v.isReleasing() && (best == nullptr || v.level() < best->level())) best = &v;
    if (best) return best;
    for (auto& v : voices_) if (best == nullptr || v.order < best->order) best = &v;
    return best;
}

void Engine::startNote(int note, float velocity, int owner)
{
    if (note < 0 || note > 127) return;
    Voice* v = allocate(note, owner);
    v->order = ++order_;
    v->noteOn(note, frequencyOf(note), velocity, owner, vp_);
}

void Engine::stopNote(int note, int owner)
{
    for (auto& v : voices_) if (v.isActive() && v.note() == note && v.owner() == owner) v.noteOff();
}

void Engine::noteOn(int note, float velocity)
{
    if (note < 0 || note > 127) return;
    midiHeld_[note] = true;
    startNote(note, velocity, OwnerMidi);
}

void Engine::noteOff(int note)
{
    if (note < 0 || note > 127) return;
    midiHeld_[note] = false;
    stopNote(note, OwnerMidi);
}

void Engine::allNotesOff()
{
    for (auto& h : midiHeld_) h = false;
    for (auto& v : voices_) v.noteOff();
}

// ---------------------------------------------------------------- parameters

void Engine::readParams()
{
    auto g = [this](ParamId id) { return getParam(id); };
    vp_.partials    = static_cast<int>(std::lround(g(ParamId::Partials)));
    vp_.tilt        = g(ParamId::Tilt);
    vp_.brightness  = g(ParamId::Brightness);
    vp_.oddEven     = g(ParamId::OddEven);
    vp_.inharmonic  = g(ParamId::Inharmonic);
    vp_.shimmer     = g(ParamId::Shimmer);
    vp_.shimmerRate = g(ParamId::ShimmerRate);
    vp_.unison      = static_cast<int>(std::lround(g(ParamId::Unison)));
    vp_.detune      = g(ParamId::Detune);
    vp_.drift       = g(ParamId::Drift);
    vp_.driftRate   = g(ParamId::DriftRate);
    vp_.spread      = g(ParamId::Spread);
    vp_.attack      = g(ParamId::Attack);
    vp_.decay       = g(ParamId::Decay);
    vp_.sustain     = g(ParamId::Sustain);
    vp_.release     = g(ParamId::Release);
    vp_.cutoff      = g(ParamId::Cutoff);
    vp_.resonance   = g(ParamId::Resonance);
    vp_.filterEnv   = g(ParamId::FilterEnv);
    vp_.filterDrift = g(ParamId::FilterDrift);
    vp_.keyTrack    = g(ParamId::KeyTrack);

    bp_.on          = g(ParamId::BrainOn) >= 0.5f;
    bp_.density     = static_cast<int>(std::lround(g(ParamId::BrainDensity)));
    bp_.rateSeconds = g(ParamId::BrainRate);
    bp_.holdMin     = g(ParamId::BrainHoldMin);
    bp_.holdMax     = g(ParamId::BrainHoldMax);
    bp_.low         = static_cast<int>(std::lround(g(ParamId::BrainLow)));
    bp_.high        = static_cast<int>(std::lround(g(ParamId::BrainHigh)));
    bp_.consonance  = g(ParamId::BrainConsonance);
    bp_.wander      = g(ParamId::BrainWander);

    ensemble_.set(g(ParamId::EnsembleMix), g(ParamId::EnsembleDepth), g(ParamId::EnsembleRate));
    reverb_.set(g(ParamId::ReverbSize), g(ParamId::ReverbDecay), g(ParamId::ReverbDamp),
                g(ParamId::ReverbPreDelay), g(ParamId::ReverbFreeze) >= 0.5f, g(ParamId::ReverbMix));
    width_ = g(ParamId::Width);

    // Tuning
    const int scaleIdx = clampv(static_cast<int>(std::lround(g(ParamId::Scale))), 0, kNumScaleChoices - 1);
    scale_ = &scales_[scaleIdx];
    refPitch_ = g(ParamId::RefPitch);
    snapKeys_ = std::lround(g(ParamId::KeyMap)) == 0;
    const int rootPc = clampv(static_cast<int>(std::lround(g(ParamId::RootNote))), 0, 11);
    rootNote_ = 60 + rootPc;
    if (rootPc != lastRootPc_) {
        lastRootPc_ = rootPc;
        brain_.setRoot(48 + rootPc);
    }
    const int seed = static_cast<int>(std::lround(g(ParamId::Seed)));
    if (seed != seed_) {
        seed_ = seed;
        rng_.seed(static_cast<uint64_t>(seed_) + 1);
        brain_.reset(rng_.fork(), 48 + rootPc);
    }
}

// ---------------------------------------------------------------- audio

void Engine::process(float* L, float* R, int n)
{
#if AMBIENT_HAS_MXCSR
    const unsigned int savedCsr = _mm_getcsr();
    _mm_setcsr(savedCsr | 0x8040);   // flush-to-zero + denormals-are-zero
#endif
    // Pending user scale (written from the message thread).
    const int uv = userVersion_.load(std::memory_order_acquire);
    if (uv != userSeen_) {
        userBusy_.store(true, std::memory_order_release);
        scales_[kNumScaleChoices - 1] = userPending_;
        userBusy_.store(false, std::memory_order_release);
        userSeen_ = uv;
    }
    readParams();

    int pos = 0;
    while (pos < n) {
        const int chunk = std::min(n - pos, maxBlock_);
        renderChunk(L + pos, R + pos, chunk);
        pos += chunk;
    }

    uint64_t m0 = 0, m1 = 0;
    int active = 0;
    for (auto& v : voices_) {
        if (!v.isActive()) continue;
        ++active;
        const int nt = v.note();
        if (nt >= 0 && nt < 64) m0 |= (1ull << nt);
        else if (nt >= 64 && nt < 128) m1 |= (1ull << (nt - 64));
    }
    mask_[0].store(m0, std::memory_order_relaxed);
    mask_[1].store(m1, std::memory_order_relaxed);
    activeVoices_.store(active, std::memory_order_relaxed);
    brainRoot_.store(brain_.root(), std::memory_order_relaxed);
#if AMBIENT_HAS_MXCSR
    _mm_setcsr(savedCsr);
#endif
}

void Engine::renderChunk(float* L, float* R, int n)
{
    float* bl = busL_.data();
    float* br = busR_.data();
    std::memset(bl, 0, sizeof(float) * static_cast<size_t>(n));
    std::memset(br, 0, sizeof(float) * static_cast<size_t>(n));

    int anchor = -1;
    for (int i = 0; i < 128; ++i) if (midiHeld_[i]) { anchor = i; break; }

    auto freqOf = [this](int note) { return frequencyOf(note); };
    auto emit = [this](const BrainEvent& e) {
        if (e.type == BrainEvent::Type::NoteOn) startNote(e.note, e.velocity, OwnerBrain);
        else stopNote(e.note, OwnerBrain);
    };

    for (int p = 0; p < n; p += kControlBlock) {
        const int len = std::min(kControlBlock, n - p);
        brain_.update(len / sr_, bp_, anchor, freqOf, emit);
        for (auto& v : voices_) if (v.isActive()) v.render(bl + p, br + p, len, vp_);
    }

    ensemble_.process(bl, br, n);
    reverb_.process(bl, br, n);

    const float master = dbToGain(getParam(ParamId::MasterGain));
    const float width = width_;
    for (int i = 0; i < n; ++i) {
        const float g = masterSmooth_.next(master);
        const float m = 0.5f * (bl[i] + br[i]);
        const float s = 0.5f * (bl[i] - br[i]) * width;
        L[i] = softClip((m + s) * g);
        R[i] = softClip((m - s) * g);
    }
}

} // namespace ambient
