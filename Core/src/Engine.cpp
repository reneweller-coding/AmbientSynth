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
    for (auto& d : noteDistance_) d.store(-1.0f, std::memory_order_relaxed);
}

void Engine::prepare(double sampleRate, int maxBlockSize)
{
    sr_ = sampleRate;
    maxBlock_ = std::max(maxBlockSize, kControlBlock);
    for (auto* b : { &nearL_, &nearR_, &farL_, &farR_, &wetL_, &wetR_, &cosL_, &cosR_, &nebL_, &nebR_, &shimL_, &shimR_ })
        b->assign(static_cast<size_t>(maxBlock_), 0.0f);
    seed_ = static_cast<int>(getParam(ParamId::Seed));
    rng_.seed(static_cast<uint64_t>(seed_) + 1);
    for (auto& v : voices_) v.prepare(sr_, rng_.fork());
    arc_.init(rng_);
    shiftDrift_.init(rng_);
    ensemble_.prepare(sr_);
    delay_.prepare(sr_);
    nearReverb_.prepare(sr_);
    farReverb_.prepare(sr_);
    midSide_.prepare(sr_);
    shifter_.prepare(sr_);
    resonator_.prepare(sr_);
    vowel_.prepare(sr_, rng_.fork());
    nebula_.prepare(sr_, rng_.fork());
    shimmerL_.prepare(sr_);
    shimmerR_.prepare(sr_);
    shimmerLpL_ = shimmerLpR_ = 0.0f;
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

bool Engine::applyPreset(int index)
{
    if (index < 0 || index >= numPresets()) return false;
    return ambient::applyPreset(preset(index), [this](ParamId id, float v) { setParam(id, v); });
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

float Engine::noteDistance(int note) const
{
    if (note < 0 || note > 127) return -1.0f;
    return noteDistance_[note].load(std::memory_order_relaxed);
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

void Engine::startNote(int note, float velocity, int owner, float distance)
{
    if (note < 0 || note > 127) return;
    Voice* v = allocate(note, owner);
    v->order = ++order_;
    v->noteOn(note, frequencyOf(note), velocity, owner, distance, vp_);
}

void Engine::stopNote(int note, int owner)
{
    for (auto& v : voices_) if (v.isActive() && v.note() == note && v.owner() == owner) v.noteOff();
}

void Engine::noteOn(int note, float velocity)
{
    if (note < 0 || note > 127) return;
    midiHeld_[note] = true;
    startNote(note, velocity, OwnerMidi, keysDepth_);
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
    vp_.air         = g(ParamId::Air);
    vp_.airColor    = g(ParamId::AirColor);
    vp_.airQ        = g(ParamId::AirQ);
    vp_.attack      = g(ParamId::Attack);
    vp_.decay       = g(ParamId::Decay);
    vp_.sustain     = g(ParamId::Sustain);
    vp_.release     = g(ParamId::Release);
    vp_.cutoff      = g(ParamId::Cutoff);
    vp_.resonance   = g(ParamId::Resonance);
    vp_.filterEnv   = g(ParamId::FilterEnv);
    vp_.filterDrift = g(ParamId::FilterDrift);
    vp_.keyTrack    = g(ParamId::KeyTrack);
    vp_.panDrift    = g(ParamId::PanDrift);
    vp_.itd         = g(ParamId::Itd);

    depth_       = g(ParamId::Depth);
    keysDepth_   = g(ParamId::KeysDepth);
    arcAmount_   = g(ParamId::ArcAmount);
    arcPeriodMin_ = g(ParamId::ArcPeriod);

    bp_.on          = g(ParamId::BrainOn) >= 0.5f;
    bp_.density     = static_cast<int>(std::lround(g(ParamId::BrainDensity)));
    bp_.rateSeconds = g(ParamId::BrainRate);
    bp_.holdMin     = g(ParamId::BrainHoldMin);
    bp_.holdMax     = g(ParamId::BrainHoldMax);
    bp_.low         = static_cast<int>(std::lround(g(ParamId::BrainLow)));
    bp_.high        = static_cast<int>(std::lround(g(ParamId::BrainHigh)));
    bp_.consonance  = g(ParamId::BrainConsonance);
    bp_.wander      = g(ParamId::BrainWander);

    // Hour-scale arc: a very slow drift that leans on density, brightness and depth.
    const float a = arc_.value() * arcAmount_;
    bp_.density = clampv(bp_.density + static_cast<int>(std::lround(a * 2.0f)), 1, ClusterBrain::kSlots);
    vp_.brightness = clampv(vp_.brightness * (1.0f + 0.25f * a), 0.0f, 1.0f);
    depth_ = clampv(depth_ * (1.0f + 0.3f * a), 0.0f, 1.0f);

    ensemble_.set(g(ParamId::EnsembleMix), g(ParamId::EnsembleDepth), g(ParamId::EnsembleRate));
    delay_.set(g(ParamId::DelayTimeL), g(ParamId::DelayTimeR), g(ParamId::DelayFeedback), g(ParamId::DelayCross), g(ParamId::DelayDamp));
    delayMix_   = g(ParamId::DelayMix);
    delayToFar_ = g(ParamId::DelayToFar);
    nearReverb_.setSpace(0.3f, 20000.0f);
    nearReverb_.set(0.6f, g(ParamId::NearDecay), g(ParamId::NearDamp), 5.0f, false, g(ParamId::NearMix));
    farReverb_.setSpace(g(ParamId::FarAsym), g(ParamId::FarHighcut));
    farReverb_.set(g(ParamId::FarSize), g(ParamId::FarDecay), g(ParamId::FarDamp), g(ParamId::FarPreDelay), g(ParamId::FarFreeze) >= 0.5f, 1.0f);
    farLevel_ = g(ParamId::FarLevel);
    midSide_.set(g(ParamId::BassMono), g(ParamId::SideAir), g(ParamId::Width));

    // Cosmos
    cosmosSend_   = g(ParamId::CosmosSend);
    cosmosReturn_ = g(ParamId::CosmosReturn);
    cosmosToFar_  = g(ParamId::CosmosToFar);
    cosmosNebula_ = g(ParamId::CosmosNebula);
    const float shiftHz = g(ParamId::CosmosShift) * (1.0f + 0.5f * g(ParamId::CosmosShiftDrift) * shiftDrift_.value());
    shifter_.set(shiftHz, shiftHz == 0.0f ? 0.0f : 1.0f);
    const float rootHz = static_cast<float>(scaleFrequency(*scale_, brain_.root(), 60 + clampv(static_cast<int>(std::lround(g(ParamId::RootNote))), 0, 11), g(ParamId::RefPitch), snapKeys_));
    resonator_.set(rootHz * g(ParamId::CosmosResPitch), g(ParamId::CosmosResFeedback), g(ParamId::CosmosRes));
    vowel_.set(g(ParamId::CosmosVowel), g(ParamId::CosmosVowelRate));
    nebula_.set(g(ParamId::CosmosSmear));
    cosmosShimmer_ = g(ParamId::CosmosShimmer);
    const int sp = clampv(static_cast<int>(std::lround(g(ParamId::CosmosShimmerPitch))), 0, kNumShimmerPitches - 1);
    shimmerL_.setSemitones(kShimmerPitchSemitones[sp]);
    shimmerR_.setSemitones(kShimmerPitchSemitones[sp]);

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
        arc_.init(rng_);
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
    arc_.update(static_cast<float>(n / sr_), 1.0f / (60.0f * std::max(arcPeriodMin_, 0.5f)), rng_);
    arcValue_.store(arc_.value() * arcAmount_, std::memory_order_relaxed);
    shiftDrift_.update(static_cast<float>(n / sr_), 0.03f, rng_);
    readParams();

    int pos = 0;
    while (pos < n) {
        const int chunk = std::min(n - pos, maxBlock_);
        renderChunk(L + pos, R + pos, chunk);
        pos += chunk;
    }

    uint64_t m0 = 0, m1 = 0;
    int active = 0;
    for (auto& d : noteDistance_) d.store(-1.0f, std::memory_order_relaxed);
    for (auto& v : voices_) {
        if (!v.isActive()) continue;
        ++active;
        const int nt = v.note();
        if (nt >= 0 && nt < 64) m0 |= (1ull << nt);
        else if (nt >= 64 && nt < 128) m1 |= (1ull << (nt - 64));
        if (nt >= 0 && nt < 128) noteDistance_[nt].store(v.distance(), std::memory_order_relaxed);
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
    float* nl = nearL_.data(); float* nr = nearR_.data();
    float* fl = farL_.data();  float* fr = farR_.data();
    float* wl = wetL_.data();  float* wr = wetR_.data();
    const size_t bytes = sizeof(float) * static_cast<size_t>(n);
    std::memset(nl, 0, bytes); std::memset(nr, 0, bytes);
    std::memset(fl, 0, bytes); std::memset(fr, 0, bytes);

    int anchor = -1;
    for (int i = 0; i < 128; ++i) if (midiHeld_[i]) { anchor = i; break; }

    auto freqOf = [this](int note) { return frequencyOf(note); };
    auto emit = [this](const BrainEvent& e) {
        if (e.type == BrainEvent::Type::NoteOn) {
            // Rich's contrast: some notes intimately close, most of them deep in the background.
            const float u = rng_.uniform();
            const float d = (u < 0.4f) ? depth_ * 0.15f * rng_.uniform()
                                       : depth_ * (0.55f + 0.45f * rng_.uniform());
            startNote(e.note, e.velocity, OwnerBrain, d);
        } else {
            stopNote(e.note, OwnerBrain);
        }
    };

    for (int p = 0; p < n; p += kControlBlock) {
        const int len = std::min(kControlBlock, n - p);
        brain_.update(len / sr_, bp_, anchor, freqOf, emit);
        for (auto& v : voices_) if (v.isActive()) v.render(nl + p, nr + p, fl + p, fr + p, len, vp_);
    }

    // Foreground plane: ensemble, asymmetric delay (echoes partly recede into the far plane), small room.
    ensemble_.process(nl, nr, n);
    delay_.process(nl, nr, wl, wr, n);
    for (int i = 0; i < n; ++i) {
        nl[i] += wl[i] * delayMix_;  nr[i] += wr[i] * delayMix_;
        fl[i] += wl[i] * delayToFar_; fr[i] += wr[i] * delayToFar_;
    }

    // Cosmos: a parallel send off the near bus, returned to both planes; the dry path is untouched.
    if (cosmosSend_ > 0.0f) {
        float* cl = cosL_.data(); float* cr = cosR_.data();
        for (int i = 0; i < n; ++i) { cl[i] = nl[i] * cosmosSend_; cr[i] = nr[i] * cosmosSend_; }
        shifter_.process(cl, cr, n);
        resonator_.process(cl, cr, n);
        vowel_.process(cl, cr, n);
        if (cosmosNebula_ > 0.0f) {
            float* bl = nebL_.data(); float* br = nebR_.data();
            nebula_.process(cl, cr, bl, br, n);
            const float m = cosmosNebula_;
            for (int i = 0; i < n; ++i) { cl[i] = cl[i] * (1.0f - m) + bl[i] * m; cr[i] = cr[i] * (1.0f - m) + br[i] * m; }
        }
        for (int i = 0; i < n; ++i) {
            nl[i] += cl[i] * cosmosReturn_; nr[i] += cr[i] * cosmosReturn_;
            fl[i] += cl[i] * cosmosToFar_;  fr[i] += cr[i] * cosmosToFar_;
        }
    }
    nearReverb_.process(nl, nr, n);

    // Background plane: 100 % wet, dark, wide. Shimmer feeds the pitch-shifted previous
    // block of the far reverb back into its input (the classic rising cloud).
    float* sl = shimL_.data(); float* sr = shimR_.data();
    if (cosmosShimmer_ > 0.0f)
        for (int i = 0; i < n; ++i) { fl[i] += sl[i]; fr[i] += sr[i]; }
    farReverb_.process(fl, fr, n);
    if (cosmosShimmer_ > 0.0f) {
        shimmerL_.process(fl, sl, n);
        shimmerR_.process(fr, sr, n);
        const float lpc = 1.0f - std::exp(-kTwoPi * 4000.0f / static_cast<float>(sr_));
        const float amt = cosmosShimmer_ * 0.5f;
        // Self-regulating loop: the feedback is throttled by the level of the far reverb
        // itself (the reverb integrates every injection), so the cloud blooms up to a fixed
        // ceiling and holds there instead of running into the clipper.
        const float envC = 1.0f - std::exp(-1.0f / (0.1f * static_cast<float>(sr_)));
        for (int i = 0; i < n; ++i) {
            shimmerLpL_ += lpc * (sl[i] - shimmerLpL_);
            shimmerLpR_ += lpc * (sr[i] - shimmerLpR_);
            const float mag = 0.5f * (std::fabs(fl[i]) + std::fabs(fr[i]));
            shimmerEnv_ += envC * (mag - shimmerEnv_);
            const float reg = clampv((0.12f - shimmerEnv_) / 0.12f, 0.0f, 1.0f);   // 1 when quiet, 0 at the ceiling
            sl[i] = shimmerLpL_ * amt * reg;
            sr[i] = shimmerLpR_ * amt * reg;
        }
    } else {
        shimmerLpL_ = shimmerLpR_ = 0.0f;
        shimmerEnv_ = 0.0f;
        std::memset(sl, 0, bytes); std::memset(sr, 0, bytes);
    }

    const float master = dbToGain(getParam(ParamId::MasterGain));
    for (int i = 0; i < n; ++i) {
        L[i] = nl[i] + fl[i] * farLevel_;
        R[i] = nr[i] + fr[i] * farLevel_;
    }
    midSide_.process(L, R, n);
    for (int i = 0; i < n; ++i) {
        const float g = masterSmooth_.next(master);
        L[i] = softClip(L[i] * g);
        R[i] = softClip(R[i] * g);
    }
}

} // namespace ambient
