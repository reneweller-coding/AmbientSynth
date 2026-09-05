#include "ambient/Engine.h"
#include "ambient/PresetMap.h"
#include "ambient/PresetMeta.h"
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
    for (int i = 0; i < kNumParams; ++i) {
        const float def = paramTable()[static_cast<size_t>(i)].def;
        params_[i].store(def, std::memory_order_relaxed);
        slotA_[i].store(def, std::memory_order_relaxed);
        slotB_[i].store(def, std::memory_order_relaxed);
    }
    for (int i = 0; i < kNumScaleChoices - 1; ++i) makeBuiltinScale(i, scales_[i]);
    makeBuiltinScale(0, scales_[kNumScaleChoices - 1]);
    std::strncpy(scales_[kNumScaleChoices - 1].name, "User (Scala)", sizeof(FixedScale::name) - 1);
    scale_ = &scales_[3];
    masterSmooth_.snap(dbToGain(-6.0f));
    for (auto& d : noteDistance_) d.store(-1.0f, std::memory_order_relaxed);
    for (auto& l : noteLevel_) l.store(0.0f, std::memory_order_relaxed);
}

void Engine::prepare(double sampleRate, int maxBlockSize)
{
    sr_ = sampleRate;
    maxBlock_ = std::max(maxBlockSize, kControlBlock);
    PresetMap::warmup();   // cached preset vectors for the map (allocates here, never in process)
    for (auto* s : { &smDelayMix_, &smDelayToFar_, &smDelay2Mix_, &smDelay2ToFar_, &smCloudSend_, &smCosmosSend_, &smCosmosReturn_, &smCosmosToFar_, &smFarLevel_ })
        s->setTime(0.02f, sr_);
    smDelayMix_.snap(getParam(ParamId::DelayMix)); smDelayToFar_.snap(getParam(ParamId::DelayToFar));
    smDelay2Mix_.snap(getParam(ParamId::Delay2Mix)); smDelay2ToFar_.snap(getParam(ParamId::Delay2ToFar));
    smCloudSend_.snap(getParam(ParamId::CloudSend)); smCosmosSend_.snap(getParam(ParamId::CosmosSend));
    smCosmosReturn_.snap(getParam(ParamId::CosmosReturn)); smCosmosToFar_.snap(getParam(ParamId::CosmosToFar));
    smFarLevel_.snap(getParam(ParamId::FarLevel));
    for (int i = 0; i < kNumParams; ++i) blendCur_[i].store(getParam(static_cast<ParamId>(i)), std::memory_order_relaxed);
    blendActive_.store(false, std::memory_order_relaxed);
    for (auto* b : { &nearL_, &nearR_, &farL_, &farR_, &wetL_, &wetR_, &cosL_, &cosR_, &nebL_, &nebR_, &shimL_, &shimR_, &fbInL_, &fbInR_, &fbMono_,
                     &roomInL_, &roomInR_, &roomOutL_, &roomOutR_ })
        b->assign(static_cast<size_t>(maxBlock_), 0.0f);
    {   // Room: pre-delay ring (>= 300 ms) and the convolver with a generated hall unless a file was set
        int ring = 1; while (ring < static_cast<int>(0.32 * sr_) + maxBlock_) ring <<= 1;
        roomDelayL_.assign(static_cast<size_t>(ring), 0.0f); roomDelayR_.assign(static_cast<size_t>(ring), 0.0f);
        roomDelayMask_ = ring - 1; roomDelayW_ = 0; roomLpL_ = roomLpR_ = 0.0f; roomLevelCur_ = 0.0f; roomTailLeft_ = 0;
        const bool keepUser = userImpulse_ && room_.hasImpulse() && std::fabs(room_.impulseSeconds()) > 0.0f;
        std::vector<float> keepL, keepR;   // a user impulse survives a sample-rate change only through the host reloading it
        room_.prepare(sr_, roomMaxSeconds_);
        if (!keepUser) { room_.generateDefault(static_cast<uint64_t>(seed_) + 7); userImpulse_ = false; }
        (void)keepL; (void)keepR;
    }
    {
        int ring = 1; while (ring < 2 * maxBlock_) ring <<= 1;
        fbRingL_.assign(static_cast<size_t>(ring), 0.0f);
        fbRingR_.assign(static_cast<size_t>(ring), 0.0f);
        fbMask_ = ring - 1; fbW_ = 0;
        fbLpL_ = fbLpR_ = fbEnv_ = 0.0f;
    }
    seed_ = static_cast<int>(getParam(ParamId::Seed));
    rng_.seed(static_cast<uint64_t>(seed_) + 1);
    for (auto& v : voices_) v.prepare(sr_, rng_.fork());
    arc_.init(rng_);
    shiftDrift_.init(rng_);
    ensemble_.prepare(sr_);
    delay_.prepare(sr_);
    delay2_.prepare(sr_);
    cloud_.prepare(sr_, rng_.fork());
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
    dcXL_ = dcXR_ = dcYL_ = dcYR_ = 0.0f;
    lastRootPc_ = -1;
    readParams();
    brain_.reset(rng_.fork(), rootNote_ - 12);   // the brain's root lives an octave below the key root
    for (auto& h : midiHeld_) h = false;
}

void Engine::reset()
{
    for (auto& v : voices_) v.kill();
    for (auto& h : midiHeld_) h = false;
    brain_.reset(rng_.fork(), rootNote_ - 12);
}

bool Engine::applyPreset(int index)
{
    if (index < 0 || index >= numPresets()) return false;
    return ambient::applyPreset(preset(index), [this](ParamId id, float v) { setParam(id, v); });
}

bool Engine::applySoundPreset(int index)
{
    if (index < 0 || index >= numPresets()) return false;
    return ambient::applyPreset(preset(index), [this](ParamId id, float v) { setParam(id, v); }, PresetScope::Sound);
}

bool Engine::applyCosmosPreset(int index)
{
    if (index < 0 || index >= numCosmosPresets()) return false;
    return ambient::applyPreset(cosmosPreset(index), [this](ParamId id, float v) { setParam(id, v); }, PresetScope::Cosmos);
}

// ---------------------------------------------------------------- morph

void Engine::setMorphSlot(int slot, const float* values)
{
    auto& s = (slot == 0) ? slotA_ : slotB_;
    for (int i = 0; i < kNumParams; ++i) s[i].store(values[i], std::memory_order_relaxed);
}

void Engine::captureMorphSlot(int slot)
{
    auto& s = (slot == 0) ? slotA_ : slotB_;
    for (int i = 0; i < kNumParams; ++i) s[i].store(params_[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
}

void Engine::morphSlot(int slot, float* out) const
{
    const auto& s = (slot == 0) ? slotA_ : slotB_;
    for (int i = 0; i < kNumParams; ++i) out[i] = s[i].load(std::memory_order_relaxed);
}

bool Engine::routeStep(double dt, float& x, float& y, float& radius)
{
    const bool active = getParam(ParamId::RouteActive) >= 0.5f && route_.count() > 0;
    if (active && !routeWasActive_) {   // (re)start from where the cursor is now
        route_.start(getParam(ParamId::MapX), getParam(ParamId::MapY), getParam(ParamId::MapRadius), getParam(ParamId::RouteLoop) >= 0.5f);
    }
    if (!active && routeWasActive_) route_.stop();
    routeWasActive_ = active;
    if (!active) { x = getParam(ParamId::MapX); y = getParam(ParamId::MapY); radius = getParam(ParamId::MapRadius); return false; }
    const bool running = route_.update(static_cast<float>(dt) * getParam(ParamId::RouteSpeed), x, y, radius);
    setParam(ParamId::MapX, x); setParam(ParamId::MapY, y); setParam(ParamId::MapRadius, radius);
    if (getParam(ParamId::MapActive) < 0.5f) setParam(ParamId::MapActive, 1.0f);   // a route always plays through the map
    if (!running) setParam(ParamId::RouteActive, 0.0f);                              // ended (not looping): the cursor stays
    return running;
}

void Engine::updateBlend(int n)
{
    // Map mode: blend the presets around the cursor, glide every parameter toward it.
    const bool on = getParam(ParamId::MapActive) >= 0.5f && PresetMap::ready() && numPresetMeta() > 0;
    const bool was = blendActive_.load(std::memory_order_relaxed);
    if (!on) { if (was) blendActive_.store(false, std::memory_order_relaxed); return; }
    if (!was) {   // start from what is sounding now: no jump when the map takes over
        for (int i = 0; i < kNumParams; ++i) blendCur_[i].store(effectiveParam(static_cast<ParamId>(i)), std::memory_order_relaxed);
        blendActive_.store(true, std::memory_order_relaxed);
    }
    const PresetMap::Blend b = PresetMap::neighbours(getParam(ParamId::MapX), getParam(ParamId::MapY), getParam(ParamId::MapRadius));
    PresetMap::blend(b, blendTarget_);
    const float glide = std::max(getParam(ParamId::MorphGlide), 0.05f);
    const float coef = 1.0f - std::exp(-static_cast<float>(n / sr_) / (glide / 3.0f));   // ~95 % after `glide` seconds
    for (const ParamDesc& d : paramTable()) {
        const int i = static_cast<int>(d.id);
        if (isPerformanceParam(d.id)) { blendCur_[i].store(getParam(d.id), std::memory_order_relaxed); continue; }
        const float cur = blendCur_[i].load(std::memory_order_relaxed), tgt = blendTarget_[i];
        float next;
        switch (d.kind) {
        case ParamKind::Float: {
            const float span = std::max(d.max - d.min, 1e-9f);
            const float pc = std::pow(clampv((cur - d.min) / span, 0.0f, 1.0f), d.skew);
            const float pt = std::pow(clampv((tgt - d.min) / span, 0.0f, 1.0f), d.skew);
            next = d.min + span * std::pow(pc + (pt - pc) * coef, 1.0f / d.skew);
            break;
        }
        case ParamKind::Int:
            next = cur + (tgt - cur) * coef;
            break;
        default:
            next = tgt;   // choices and switches follow the strongest neighbour
        }
        blendCur_[i].store(next, std::memory_order_relaxed);
    }
}

float Engine::effectiveParam(ParamId id) const
{
    const float live = getParam(id);
    if (blendActive_.load(std::memory_order_relaxed) && !isPerformanceParam(id)) {
        const float v = blendCur_[static_cast<int>(id)].load(std::memory_order_relaxed);
        return paramDesc(id).kind == ParamKind::Int ? static_cast<float>(std::lround(v)) : v;
    }
    if (isMorphParam(id) || getParam(ParamId::MorphActive) < 0.5f) return live;
    const int i = static_cast<int>(id);
    const float a = slotA_[i].load(std::memory_order_relaxed), b = slotB_[i].load(std::memory_order_relaxed);
    const float t = morphCur_.load(std::memory_order_relaxed);
    const ParamDesc& d = paramDesc(id);
    switch (d.kind) {
    case ParamKind::Float: {
        // Interpolate in the skewed (perceptual) domain, like the host's knob travel.
        const float span = std::max(d.max - d.min, 1e-9f);
        const float pa = std::pow(clampv((a - d.min) / span, 0.0f, 1.0f), d.skew);
        const float pb = std::pow(clampv((b - d.min) / span, 0.0f, 1.0f), d.skew);
        const float p = pa + (pb - pa) * t;
        return d.min + span * std::pow(p, 1.0f / d.skew);
    }
    case ParamKind::Int:
        return static_cast<float>(std::lround(a + (b - a) * t));
    default:
        return t < 0.5f ? a : b;   // choices and switches flip halfway
    }
}

void Engine::setUserScale(const FixedScale& s)
{
    while (userBusy_.load(std::memory_order_acquire)) { /* audio thread is copying, microseconds */ }
    userPending_ = s;
    userVersion_.fetch_add(1, std::memory_order_release);
}

void Engine::setUserWavetable(const Wavetable& t)
{
    while (tableBusy_.load(std::memory_order_acquire)) { /* audio thread copying, microseconds */ }
    userTablePending_ = t;
    userTableFrames_.store(t.frames, std::memory_order_relaxed);
    tableVersion_.fetch_add(1, std::memory_order_release);
}

bool Engine::loadUserWavetable(const float* mono, int n, int frameLen)
{
    Wavetable t;
    if (!t.analyse(mono, n, frameLen)) return false;
    setUserWavetable(t);
    return true;
}

void Engine::setTexture(const float* mono, int n, double sampleRate, double baseHz)
{
    const int active = textureActive_.load(std::memory_order_acquire);
    const int target = active < 0 ? 0 : 1 - active;
    // Wait until the audio thread no longer holds the target buffer (it publishes the index it
    // used last); bounded, so a host without a running audio thread cannot hang us.
    for (int spin = 0; spin < 200000 && textureInUse_.load(std::memory_order_acquire) == target; ++spin) { }
    Texture& t = textures_[target];
    t.mono.assign(mono, mono + std::max(n, 0));
    t.sampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    t.baseHz = baseHz > 0.0 ? baseHz : 261.6256;
    textureActive_.store(target, std::memory_order_release);
}

double Engine::frequencyOf(int note) const
{
    // Purity blends between 12-TET (0) and the chosen scale (1) in the log domain: at 1 the
    // partials of different notes lock, at 0 they beat like a piano; in between the beating
    // slows down as the intervals close in on their ratios.
    const double pure = scaleFrequency(*scale_, note, rootNote_, refPitch_, snapKeys_);
    const double p = purityCur_;
    if (p >= 0.9999) return pure;
    const double et = refPitch_ * std::pow(2.0, (note - 69) / 12.0);
    return std::exp(std::log(et) + (std::log(pure) - std::log(et)) * clampv(p, 0.0, 1.0));
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
    const double hz = frequencyOf(note);
    v->noteOn(note, hz, velocity, owner, distance, vp_);
    if (owner == OwnerMidi) {   // portamento: a key slides in from the previous key
        if (portamento_ > 0.0f && lastKeyHz_ > 0.0 && std::fabs(lastKeyHz_ - hz) > 1e-6) v->glideFrom(lastKeyHz_, portamento_, portaGravity_);
        lastKeyHz_ = hz;
    }
}

void Engine::stopNote(int note, int owner)
{
    for (auto& v : voices_) if (v.isActive() && v.note() == note && v.owner() == owner) v.noteOff();
}

void Engine::noteOn(int note, float velocity)
{
    if (note < 0 || note > 127) return;
    if (hold_ && midiHeld_[note]) {   // Hold: pressing a sounding key releases it
        midiHeld_[note] = false;
        stopNote(note, OwnerMidi);
        return;
    }
    midiHeld_[note] = true;
    startNote(note, velocity, OwnerMidi, keysDepth_);
}

void Engine::noteOff(int note)
{
    if (note < 0 || note > 127) return;
    if (hold_) return;   // keys latch
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
    // Inertia: every float parameter glides to its effective value with the Inertia time
    // constant (skew domain), the analogue slew that keeps even a torn-open knob slow.
    const float inertia = getParam(ParamId::Inertia);
    const float inertiaCoef = inertia > 0.005f ? 1.0f - std::exp(-lastBlockSeconds_ / inertia) : 1.0f;
    auto g = [this, inertiaCoef](ParamId id) {
        const float target = effectiveParam(id);
        const int i = static_cast<int>(id);
        const ParamDesc& d = paramDesc(id);
        if (inertiaCoef >= 1.0f || d.kind != ParamKind::Float || isPerformanceParam(id)) { inertiaCur_[i] = target; return target; }
        const float span = std::max(d.max - d.min, 1e-9f);
        const float pc = std::pow(clampv((inertiaCur_[i] - d.min) / span, 0.0f, 1.0f), d.skew);
        const float pt = std::pow(clampv((target - d.min) / span, 0.0f, 1.0f), d.skew);
        inertiaCur_[i] = d.min + span * std::pow(pc + (pt - pc) * inertiaCoef, 1.0f / d.skew);
        return inertiaCur_[i];
    };
    vp_.level       = g(ParamId::OscLevel);
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
    vp_.bloom       = g(ParamId::Bloom);
    vp_.bloomTime   = g(ParamId::BloomTime);
    vp_.stack       = static_cast<int>(std::lround(g(ParamId::Stack)));
    vp_.rateWander  = g(ParamId::RateWander);
    {   // Source slots: 13 parameters each, laid out identically for Source 2 and Source 3.
        const ParamId first[kSlots] = { ParamId::Src2Type, ParamId::Src3Type };
        for (int k = 0; k < kSlots; ++k) {
            auto at = [&](int off) { return g(static_cast<ParamId>(static_cast<int>(first[k]) + off)); };
            SlotParams& s = vp_.slot[k];
            s.type          = static_cast<SourceType>(clampv(static_cast<int>(std::lround(at(0))), 0, kNumSourceTypes - 1));
            s.level         = at(1);
            s.octave        = static_cast<int>(std::lround(at(2)));
            s.ratio         = static_cast<int>(std::lround(at(3)));
            s.pan           = at(4);
            s.table         = static_cast<int>(std::lround(at(5)));
            s.position      = at(6);
            s.positionDrift = at(7);
            s.fmRatio       = at(8);
            s.fmIndex       = at(9);
            s.grainMs       = at(10);
            s.density       = at(11);
            s.follow        = at(12) >= 0.5f;
        }
        vp_.userTable = userTable_.frames > 0 ? &userTable_ : nullptr;
        const int a = textureActive_.load(std::memory_order_acquire);
        textureInUse_.store(a, std::memory_order_release);
        vp_.texture = (a >= 0 && !textures_[a].empty()) ? &textures_[a] : nullptr;
    }
    subLevel_       = g(ParamId::SubLevel);
    subOctave_      = std::lround(g(ParamId::SubOctave)) == 0 ? 1 : 2;
    subGlide_       = g(ParamId::SubGlide);
    subBinaural_    = g(ParamId::SubBinaural);
    subTone_        = g(ParamId::SubTone);
    subGhost_       = std::lround(g(ParamId::SubSource)) == 1;
    vp_.lowCut      = g(ParamId::PadLowCut);
    const bool hold = g(ParamId::Hold) >= 0.5f;
    if (hold_ && !hold) { for (int i = 0; i < 128; ++i) if (midiHeld_[i]) { midiHeld_[i] = false; stopNote(i, OwnerMidi); } }
    hold_ = hold;
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
    vp_.zMode       = static_cast<int>(std::lround(g(ParamId::ZMode)));
    vp_.zShape      = static_cast<int>(std::lround(g(ParamId::ZShape)));
    vp_.zX          = g(ParamId::ZX);
    vp_.zY          = g(ParamId::ZY);
    vp_.zRate       = g(ParamId::ZRate);
    vp_.zDepth      = g(ParamId::ZDepth);
    vp_.zRes        = g(ParamId::ZResonance);
    vp_.zKeyTrack   = g(ParamId::ZKeyTrack);
    vp_.zMix        = g(ParamId::ZMix);
    vp_.panDrift    = g(ParamId::PanDrift);
    vp_.itd         = g(ParamId::Itd);
    vp_.presence    = g(ParamId::Presence);
    vp_.breath      = g(ParamId::Breath);
    vp_.breathRate  = g(ParamId::BreathRate);

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
    delay2_.set(g(ParamId::Delay2TimeL), g(ParamId::Delay2TimeR), g(ParamId::Delay2Feedback), g(ParamId::Delay2Cross), g(ParamId::Delay2Damp));
    delay2Mix_   = g(ParamId::Delay2Mix);
    delay2ToFar_ = g(ParamId::Delay2ToFar);
    cloud_.set(g(ParamId::CloudDensity), g(ParamId::CloudSize), g(ParamId::CloudPitch), g(ParamId::CloudSpray), g(ParamId::CloudLevel));
    cloudSend_ = g(ParamId::CloudSend);
    nearReverb_.setSpace(0.3f, 20000.0f);
    nearReverb_.set(0.6f, g(ParamId::NearDecay), g(ParamId::NearDamp), 5.0f, false, g(ParamId::NearMix));
    farReverb_.setSpace(g(ParamId::FarAsym), g(ParamId::FarHighcut));
    farReverb_.set(g(ParamId::FarSize), g(ParamId::FarDecay), g(ParamId::FarDamp), g(ParamId::FarPreDelay), g(ParamId::FarFreeze) >= 0.5f, 1.0f);
    farLevel_ = g(ParamId::FarLevel);
    midSide_.set(g(ParamId::BassMono), g(ParamId::SideAir), g(ParamId::Width));
    roomLevel_    = g(ParamId::RoomLevel);
    roomSource_   = static_cast<int>(std::lround(g(ParamId::RoomSource)));
    roomPreDelay_ = static_cast<int>(g(ParamId::RoomPreDelay) * 0.001f * static_cast<float>(sr_));
    roomHighcut_  = g(ParamId::RoomHighcut);
    fbBus_   = g(ParamId::FeedbackBus);
    fbFm_    = g(ParamId::FeedbackFm);
    fbTone_  = g(ParamId::FeedbackTone);
    fbDrive_ = g(ParamId::FeedbackDrive);
    vp_.fmAmount = fbFm_;

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
    {
        const float purity = g(ParamId::TunePurity), drift = g(ParamId::TuneDrift);
        const float wander = drift > 0.0f ? 0.5f * drift * purityDrift_.value() : 0.0f;   // drifter is advanced in process()
        purityCur_ = clampv(purity + wander, 0.0f, 1.0f);
        retune_ = purityCur_ < 0.9999 || drift > 0.0f;
    }
    vp_.freeze = g(ParamId::Freeze) >= 0.5f;
    vp_.airGhost = std::lround(g(ParamId::AirMode)) == 1;
    vp_.rootHz = frequencyOf(brain_.root());
    portamento_ = g(ParamId::Portamento);
    portaGravity_ = g(ParamId::PortaGravity);
    fbTape_ = g(ParamId::FeedbackTape);
    {   // Coherence: four Kuramoto oscillators, coupled by K = Coherence; their sines become
        // offsets on brightness, depth, pan and the z-plane point, scaled by Depth.
        const float depth = g(ParamId::CoherenceDepth);
        vp_.cohBrightness = 0.15f * depth * std::sin(kuraPhase_[0]);
        depth_ = clampv(depth_ + 0.15f * depth * std::sin(kuraPhase_[1]), 0.0f, 1.0f);
        vp_.cohPan = 0.4f * depth * std::sin(kuraPhase_[2]);
        vp_.cohZ = 0.25f * depth * std::sin(kuraPhase_[3]);
    }
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
    // Pending user scale / wavetable (written from the message thread).
    const int uv = userVersion_.load(std::memory_order_acquire);
    if (uv != userSeen_) {
        userBusy_.store(true, std::memory_order_release);
        scales_[kNumScaleChoices - 1] = userPending_;
        userBusy_.store(false, std::memory_order_release);
        userSeen_ = uv;
    }
    const int tv = tableVersion_.load(std::memory_order_acquire);
    if (tv != tableSeen_) {
        tableBusy_.store(true, std::memory_order_release);
        userTable_ = userTablePending_;
        tableBusy_.store(false, std::memory_order_release);
        tableSeen_ = tv;
    }
    arc_.update(static_cast<float>(n / sr_), 1.0f / (60.0f * std::max(arcPeriodMin_, 0.5f)), rng_);
    arcValue_.store(arc_.value() * arcAmount_, std::memory_order_relaxed);
    shiftDrift_.update(static_cast<float>(n / sr_), 0.03f, rng_);
    purityDrift_.update(static_cast<float>(n / sr_), getParam(ParamId::TuneDriftRate), rng_);
    lastBlockSeconds_ = static_cast<float>(n / sr_);
    {   // Kuramoto bank: dθ_i = ω_i + K/N Σ sin(θ_j − θ_i). Natural periods 23/31/41/53 s over Rate;
        // K up to 0.6 rad/s locks them (the spread of ω is 0.15 rad/s), 0 leaves them independent.
        const float rate = std::max(getParam(ParamId::CoherenceRate), 0.05f);
        const float K = 0.6f * getParam(ParamId::Coherence) * rate;   // scales with the rate, so the lock is the same at every tempo
        const float dt = static_cast<float>(n / sr_);
        const float periods[4] = { 23.0f, 31.0f, 41.0f, 53.0f };
        float dth[4];
        for (int i = 0; i < 4; ++i) {
            float coupling = 0.0f;
            for (int j = 0; j < 4; ++j) coupling += std::sin(kuraPhase_[j] - kuraPhase_[i]);
            dth[i] = kTwoPi * rate / periods[i] + K * coupling * 0.25f;
        }
        for (int i = 0; i < 4; ++i) { kuraPhase_[i] += dth[i] * dt; if (kuraPhase_[i] > kTwoPi) kuraPhase_[i] -= kTwoPi; if (kuraPhase_[i] < 0.0f) kuraPhase_[i] += kTwoPi; }
    }
    {   // morph position glides toward its target at 1/glide per second (glide 0 = jump)
        const float target = clampv(getParam(ParamId::MorphPos), 0.0f, 1.0f);
        const float glide = getParam(ParamId::MorphGlide);
        float cur = morphCur_.load(std::memory_order_relaxed);
        if (glide <= 0.001f) cur = target;
        else {
            const float step = static_cast<float>(n / sr_) / glide;
            cur += clampv(target - cur, -step, step);
        }
        morphCur_.store(cur, std::memory_order_relaxed);
    }
    updateBlend(n);
    readParams();
    if (retune_)   // tuning purity / drift: every sounding voice glides to its current frequency
        for (auto& v : voices_) if (v.isActive()) v.setTargetFrequency(frequencyOf(v.note()));

    int pos = 0;
    while (pos < n) {
        const int chunk = std::min(n - pos, maxBlock_);
        renderChunk(L + pos, R + pos, chunk);
        pos += chunk;
    }
    // Sleep: nothing sounding and the tails gone for two seconds -> the next blocks skip the
    // effect chain (zeros out) until a voice starts. The brain keeps running inside renderChunk,
    // so a generative patch wakes itself; MIDI and OSC notes wake it through the voices.
    {
        bool anyVoice = false;
        for (auto& v : voices_) if (v.isActive()) { anyVoice = true; break; }
        float peak = 0.0f;
        for (int i = 0; i < n; ++i) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
        if (!anyVoice && peak < 3.2e-5f) silentSamples_ += n; else silentSamples_ = 0;
        asleep_ = silentSamples_ > static_cast<long>(2.0 * sr_);
    }

    uint64_t m0 = 0, m1 = 0;
    int active = 0;
    for (auto& d : noteDistance_) d.store(-1.0f, std::memory_order_relaxed);
    for (auto& l : noteLevel_) l.store(0.0f, std::memory_order_relaxed);
    for (auto& v : voices_) {
        if (!v.isActive()) continue;
        ++active;
        const int nt = v.note();
        if (nt >= 0 && nt < 64) m0 |= (1ull << nt);
        else if (nt >= 64 && nt < 128) m1 |= (1ull << (nt - 64));
        if (nt >= 0 && nt < 128) {
            noteDistance_[nt].store(v.distance(), std::memory_order_relaxed);
            if (v.level() > noteLevel_[nt].load(std::memory_order_relaxed)) noteLevel_[nt].store(v.level(), std::memory_order_relaxed);
        }
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

    // Feedback loop, input side: what the previous chunk wrote into the ring comes back as
    // phase modulation of the partials and/or as signal into the near bus (before ensemble,
    // delays and reverbs -- "after the reverb back before the filter").
    const bool fbOn = fbBus_ > 0.0f || fbFm_ > 0.0f;
    float* fbl = fbInL_.data(); float* fbr = fbInR_.data(); float* fbm = fbMono_.data();
    if (fbOn) {
        // The level throttle applies to the bus path only (that one adds energy and can run
        // away); phase modulation moves energy between partials without adding any, so it
        // gets the saturated signal unthrottled. The throttle ramps across the chunk.
        const float regTarget = clampv((0.1f - fbEnv_) / 0.1f, 0.0f, 1.0f);
        const float regStep = (regTarget - fbReg_) / static_cast<float>(n);
        // Tape: wow (slow, irregular, up to 3 ms) and flutter (6 Hz, up to 0.3 ms) move the read
        // position -- a fractional delay in the loop, so every pass through the loop smears a
        // little more, the way a tape loop goes soft with each generation.
        const float wowSamples = fbTape_ > 0.0f ? (0.003f * tapeWow_.update(static_cast<float>(n / sr_), 0.3f, rng_) * 0.5f + 0.0015f) * fbTape_ * static_cast<float>(sr_) : 0.0f;
        const double flutterInc = 6.0 / sr_;
        for (int i = 0; i < n; ++i) {
            float l, r;
            if (fbTape_ > 0.0f) {
                tapeFlutterPhase_ += flutterInc; if (tapeFlutterPhase_ >= 1.0) tapeFlutterPhase_ -= 1.0;
                const float delay = wowSamples + 0.0003f * fbTape_ * static_cast<float>(sr_) * (0.5f + 0.5f * sin01(tapeFlutterPhase_));
                const int di = static_cast<int>(delay); const float fr = delay - static_cast<float>(di);
                const int i0 = (fbW_ - n + i - di) & fbMask_, i1 = (i0 - 1) & fbMask_;
                l = fbRingL_[static_cast<size_t>(i0)] + fr * (fbRingL_[static_cast<size_t>(i1)] - fbRingL_[static_cast<size_t>(i0)]);
                r = fbRingR_[static_cast<size_t>(i0)] + fr * (fbRingR_[static_cast<size_t>(i1)] - fbRingR_[static_cast<size_t>(i0)]);
            } else {
                const int idx = (fbW_ - n + i) & fbMask_;
                l = fbRingL_[static_cast<size_t>(idx)]; r = fbRingR_[static_cast<size_t>(idx)];
            }
            const float reg = fbReg_ + regStep * static_cast<float>(i + 1);
            fbl[i] = l * reg;
            fbr[i] = r * reg;
            fbm[i] = 0.5f * (l + r);
        }
        fbReg_ = regTarget;
    }

    bool anyVoice = false;
    for (int p = 0; p < n; p += kControlBlock) {
        const int len = std::min(kControlBlock, n - p);
        brain_.update(len / sr_, bp_, anchor, freqOf, emit);
        const float* fm = (fbOn && fbFm_ > 0.0f) ? fbm + p : nullptr;
        for (auto& v : voices_) if (v.isActive()) { anyVoice = true; v.render(nl + p, nr + p, fl + p, fr + p, len, vp_, fm); }
    }
    if (asleep_ && !anyVoice) {   // sleeping: the whole effect chain is skipped, output stays silent
        std::memset(L, 0, bytes); std::memset(R, 0, bytes);
        return;
    }
    if (fbOn && fbBus_ > 0.0f)
        for (int i = 0; i < n; ++i) { nl[i] += fbl[i] * fbBus_; nr[i] += fbr[i] * fbBus_; }

    // Foreground plane: ensemble, asymmetric delay (echoes partly recede into the far plane), small room.
    ensemble_.process(nl, nr, n);
    delay_.process(nl, nr, wl, wr, n);
    for (int i = 0; i < n; ++i) {
        const float m = smDelayMix_.next(delayMix_), tf = smDelayToFar_.next(delayToFar_);
        nl[i] += wl[i] * m;  nr[i] += wr[i] * m;
        fl[i] += wl[i] * tf; fr[i] += wr[i] * tf;
    }
    // Second delay in series: it hears the first delay's echoes and spins longer chains.
    delay2_.process(nl, nr, wl, wr, n);
    for (int i = 0; i < n; ++i) {
        const float m = smDelay2Mix_.next(delay2Mix_), tf = smDelay2ToFar_.next(delay2ToFar_);
        nl[i] += wl[i] * m;  nr[i] += wr[i] * m;
        fl[i] += wl[i] * tf; fr[i] += wr[i] * tf;
    }
    // Granular cloud on the far plane: grains of the foreground, scattered and transposed,
    // dropped into the far reverb.
    if (cloudSend_ > 0.0f || smCloudSend_.value > 1e-4f) {
        float* cl = cosL_.data(); float* cr = cosR_.data();
        for (int i = 0; i < n; ++i) { const float s = smCloudSend_.next(cloudSend_); cl[i] = nl[i] * s; cr[i] = nr[i] * s; }
        cloud_.process(cl, cr, fl, fr, n);
    }

    // Cosmos: a parallel send off the near bus, returned to both planes; the dry path is untouched.
    if (cosmosSend_ > 0.0f || smCosmosSend_.value > 1e-4f) {
        float* cl = cosL_.data(); float* cr = cosR_.data();
        for (int i = 0; i < n; ++i) { const float s = smCosmosSend_.next(cosmosSend_); cl[i] = nl[i] * s; cr[i] = nr[i] * s; }
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
            const float ret = smCosmosReturn_.next(cosmosReturn_), tf = smCosmosToFar_.next(cosmosToFar_);
            nl[i] += cl[i] * ret; nr[i] += cr[i] * ret;
            fl[i] += cl[i] * tf;  fr[i] += cr[i] * tf;
        }
    }
    nearReverb_.process(nl, nr, n);

    // Background plane: 100 % wet, dark, wide. Shimmer feeds the pitch-shifted previous
    // block of the far reverb back into its input (the classic rising cloud).
    float* sl = shimL_.data(); float* sr = shimR_.data();
    if (cosmosShimmer_ > 0.0f)
        for (int i = 0; i < n; ++i) { fl[i] += sl[i]; fr[i] += sr[i]; }
    // Room: the convolution reverb hears the far sends (before the FDN) or the finished
    // near bus, through a pre-delay; it keeps running for one impulse length after its
    // level reaches zero so the tail can finish, and costs nothing while silent.
    const bool roomOn = roomLevel_ > 0.0005f || roomLevelCur_ > 0.0005f || roomTailLeft_ > 0;
    float* rl = roomInL_.data(); float* rr = roomInR_.data();
    if (roomOn) {
        const float* srcL = roomSource_ == 0 ? fl : nl; const float* srcR = roomSource_ == 0 ? fr : nr;
        for (int i = 0; i < n; ++i) {
            roomDelayL_[static_cast<size_t>(roomDelayW_)] = srcL[i]; roomDelayR_[static_cast<size_t>(roomDelayW_)] = srcR[i];
            const int rd = (roomDelayW_ - roomPreDelay_) & roomDelayMask_;
            rl[i] = roomDelayL_[static_cast<size_t>(rd)]; rr[i] = roomDelayR_[static_cast<size_t>(rd)];
            roomDelayW_ = (roomDelayW_ + 1) & roomDelayMask_;
        }
        roomTailLeft_ = roomLevel_ > 0.0005f ? static_cast<long>(room_.impulseSeconds() * sr_) + Convolver::kBlock : std::max(0L, roomTailLeft_ - n);
    }
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

    const float master = dbToGain(effectiveParam(ParamId::MasterGain));
    for (int i = 0; i < n; ++i) {
        const float far = smFarLevel_.next(farLevel_);
        L[i] = nl[i] + fl[i] * far;
        R[i] = nr[i] + fr[i] * far;
    }
    if (roomOn) {
        float* ol = roomOutL_.data(); float* orr = roomOutR_.data();
        room_.process(rl, rr, ol, orr, n);
        const float lpc = 1.0f - std::exp(-kTwoPi * roomHighcut_ / static_cast<float>(sr_));
        const float levelC = 1.0f - std::exp(-1.0f / (0.05f * static_cast<float>(sr_)));
        // An energy-normalised impulse returns the far sends at their own power, which is far
        // louder than the FDN's output at Far Level 1; 0.35 puts Room Level 1 in the same league
        // (measured: -15.8 dBFS raw vs. -23 dBFS for the FDN on the default patch).
        const float roomGain = 0.35f;
        for (int i = 0; i < n; ++i) {
            roomLevelCur_ += (roomLevel_ - roomLevelCur_) * levelC;
            roomLpL_ += lpc * (ol[i] - roomLpL_);
            roomLpR_ += lpc * (orr[i] - roomLpR_);
            L[i] += roomLpL_ * roomLevelCur_ * roomGain;
            R[i] += roomLpR_ * roomLevelCur_ * roomGain;
        }
    } else {
        roomLpL_ = roomLpR_ = 0.0f;
    }

    // Feedback loop, output side: the mix (before mid/side, sub and master) goes into the ring,
    // low-passed at Tone, driven into a soft saturation (tanh-like, gain-compensated), and
    // throttled by its own mean level like the shimmer loop: feedback -> 0 at a mean level of
    // 0.1 (about -20 dBFS), so a hot loop hisses and holds where a naked one would run into
    // the clipper -- and the loop can thicken a drone without taking it over (a higher
    // ceiling let Distant Storm climb 10 dB and collapse to a correlation of 0.6).
    if (fbOn) {
        const float lpc  = 1.0f - std::exp(-kTwoPi * fbTone_ / static_cast<float>(sr_));
        const float envC = 1.0f - std::exp(-1.0f / (0.05f * static_cast<float>(sr_)));
        const float drive = 1.0f + 9.0f * fbDrive_;
        const float comp  = 1.0f / std::sqrt(drive);
        auto sat = [](float x) {   // rational tanh, exact to 1e-3 up to |x| = 3
            x = clampv(x, -3.0f, 3.0f);
            const float x2 = x * x;
            return x * (27.0f + x2) / (27.0f + 9.0f * x2);
        };
        // DC blocker in the loop: a saturating loop with a DC gain above one locks onto a DC
        // operating point (Feedback Hiss sat at +0.11 before this), so nothing below 10 Hz circulates.
        const float hpc = 1.0f - kTwoPi * 10.0f / static_cast<float>(sr_);
        for (int i = 0; i < n; ++i) {
            fbLpL_ += lpc * (L[i] - fbLpL_);
            fbLpR_ += lpc * (R[i] - fbLpR_);
            const float xl = fbLpL_ - fbHpXL_ + hpc * fbHpYL_; fbHpXL_ = fbLpL_; fbHpYL_ = xl;
            const float xr = fbLpR_ - fbHpXR_ + hpc * fbHpYR_; fbHpXR_ = fbLpR_; fbHpYR_ = xr;
            const float mag = 0.5f * (std::fabs(L[i]) + std::fabs(R[i]));
            fbEnv_ += envC * (mag - fbEnv_);
            const int idx = (fbW_ + i) & fbMask_;
            if (fbTape_ > 0.0f) {
                // Tape: asymmetric saturation (an even-order term the DC blocker cleans up on the
                // next pass) and a noise floor that rises with the level in the loop.
                const float asym = 0.2f * fbTape_;
                const float noise = 0.02f * fbTape_ * fbEnv_;
                fbRingL_[static_cast<size_t>(idx)] = sat((xl + asym * xl * xl) * drive) * comp + noise * rng_.bipolar();
                fbRingR_[static_cast<size_t>(idx)] = sat((xr + asym * xr * xr) * drive) * comp + noise * rng_.bipolar();
            } else {
                fbRingL_[static_cast<size_t>(idx)] = sat(xl * drive) * comp;
                fbRingR_[static_cast<size_t>(idx)] = sat(xr * drive) * comp;
            }
        }
        fbW_ = (fbW_ + n) & fbMask_;
    } else {
        fbLpL_ = fbLpR_ = fbEnv_ = 0.0f;
        fbHpXL_ = fbHpXR_ = fbHpYL_ = fbHpYR_ = 0.0f;
        fbReg_ = 1.0f;
    }

    // Foundation: a dry sub voice on the brain's root, gliding between roots; the two ears
    // may run a few Hz apart (binaural beat), which Bass Mono leaves alone below its crossover
    // only in the mid channel -- so the beat is kept by feeding it after the mid/side stage.
    float* subL = wl; float* subR = wr;   // the delay scratch buffers are free by now
    const bool subOn = subLevel_ > 0.0f || subLevelCur_ > 1e-4f;
    if (subOn) {
        const double rootHz = frequencyOf(brain_.root());
        double subHz = rootHz / (subOctave_ == 1 ? 2.0 : 4.0);
        if (subGhost_) {
            // Ghost tone (Rich's combination tones): the difference between the two lowest sounding
            // voices is the tone the ear makes by itself in just intonation (3:2 -> f/2, 5:4 -> f/4,
            // 4:3 -> f/3). The sub doubles it, folded into the register the root mode would use,
            // so switching the source never changes the sub's range. With fewer than two distinct
            // pitches it falls back to the root.
            double f1 = 0.0, f2 = 0.0;
            for (const auto& v : voices_)
                if (v.isActive() && (f1 <= 0.0 || v.frequency() < f1)) f1 = v.frequency();
            if (f1 > 0.0)
                for (const auto& v : voices_)
                    if (v.isActive() && v.frequency() / f1 > 1.003 && (f2 <= 0.0 || v.frequency() < f2)) f2 = v.frequency();
            if (f1 > 0.0 && f2 > 0.0) {
                double ghost = f2 - f1;
                const double lo = subHz * 0.75, hi = lo * 2.0;   // the octave around the root sub
                while (ghost >= hi) ghost *= 0.5;
                while (ghost < lo) ghost *= 2.0;
                subHz = ghost;
            }
        }
        const double target = std::log(std::max(subHz, 10.0));
        if (subFreqCur_ <= 0.0) subFreqCur_ = target;
        const double glideC = 1.0 - std::exp(-1.0 / (std::max(subGlide_, 0.05f) * sr_));
        const float levelC = 1.0f - std::exp(-1.0f / (2.0f * static_cast<float>(sr_)));
        for (int i = 0; i < n; ++i) {
            subFreqCur_ += (target - subFreqCur_) * glideC;
            subLevelCur_ += (subLevel_ - subLevelCur_) * levelC;
            const double f = std::exp(subFreqCur_);
            const double fL = std::max(f - 0.5 * subBinaural_, 5.0), fR = std::max(f + 0.5 * subBinaural_, 5.0);
            subPhaseL_ += fL / sr_; if (subPhaseL_ >= 1.0) subPhaseL_ -= 1.0;
            subPhaseR_ += fR / sr_; if (subPhaseR_ >= 1.0) subPhaseR_ -= 1.0;
            const float triL = 4.0f * std::fabs(static_cast<float>(subPhaseL_) - 0.5f) - 1.0f;
            const float triR = 4.0f * std::fabs(static_cast<float>(subPhaseR_) - 0.5f) - 1.0f;
            const float g = subLevelCur_ * 0.45f;
            subL[i] = g * ((1.0f - subTone_) * sin01(subPhaseL_) + subTone_ * triL);
            subR[i] = g * ((1.0f - subTone_) * sin01(subPhaseR_) + subTone_ * triR);
        }
    }
    midSide_.process(L, R, n);
    if (subOn)
        for (int i = 0; i < n; ++i) { L[i] += subL[i]; R[i] += subR[i]; }
    // Output DC blocker at 4 Hz, below the lowest sub the Foundation can reach. Several paths
    // can leave an offset behind -- FM at an integer ratio, the asymmetric tape term, a granular
    // window over a clip that carries one, the shimmer's pitch shifter -- and an offset costs
    // headroom in the soft clipper without ever being heard. One filter at the end covers them all.
    const float dcR = 1.0f - kTwoPi * 4.0f / static_cast<float>(sr_);
    for (int i = 0; i < n; ++i) {
        const float g = masterSmooth_.next(master);
        const float yl = L[i] - dcXL_ + dcR * dcYL_; dcXL_ = L[i]; dcYL_ = yl;
        const float yr = R[i] - dcXR_ + dcR * dcYR_; dcXR_ = R[i]; dcYR_ = yr;
        L[i] = softClip(yl * g);
        R[i] = softClip(yr * g);
    }
}

} // namespace ambient
