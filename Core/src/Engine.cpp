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
#if defined(__aarch64__) || defined(_M_ARM64)
  #define AMBIENT_HAS_FPCR 1
#else
  #define AMBIENT_HAS_FPCR 0
#endif

namespace ambient {

Engine::Engine()
{
    // The envelopes start from a usable shape rather than from nothing.
    for (auto& e : envPending_) e.parse("0:0/1:1/4:0");
    for (auto& e : envShape_) e.parse("0:0/1:1/4:0");
    for (int i = 0; i < kNumParams; ++i) {
        const float def = paramTable()[static_cast<size_t>(i)].def;
        params_[i].store(def, std::memory_order_relaxed);
        slotA_[i].store(def, std::memory_order_relaxed);
        slotB_[i].store(def, std::memory_order_relaxed);
    }
    for (int i = 0; i < kUserScaleIndex; ++i) makeBuiltinScale(i, scales_[i]);
    makeBuiltinScale(0, scales_[kUserScaleIndex]);
    std::strncpy(scales_[kUserScaleIndex].name, "User (Scala)", sizeof(FixedScale::name) - 1);
    // The timbre scale starts as twelve equal steps and is replaced the moment it is asked for.
    makeBuiltinScale(0, scales_[kTimbreScaleIndex]);
    std::strncpy(scales_[kTimbreScaleIndex].name, "Timbre (Sethares)", sizeof(FixedScale::name) - 1);
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
    for (auto* s : { &smDelayMix_, &smDelayToFar_, &smDelay2Mix_, &smDelay2ToFar_, &smCloudSend_, &smCosmosSend_, &smCosmosReturn_, &smCosmosToFar_, &smFarLevel_, &smFarWidth_, &smEnvelop_ })
        s->setTime(0.02f, sr_);
    smDelayMix_.snap(getParam(ParamId::DelayMix)); smDelayToFar_.snap(getParam(ParamId::DelayToFar));
    smDelay2Mix_.snap(getParam(ParamId::Delay2Mix)); smDelay2ToFar_.snap(getParam(ParamId::Delay2ToFar));
    smCloudSend_.snap(getParam(ParamId::CloudSend)); smCosmosSend_.snap(getParam(ParamId::CosmosSend));
    smCosmosReturn_.snap(getParam(ParamId::CosmosReturn)); smCosmosToFar_.snap(getParam(ParamId::CosmosToFar));
    smFarLevel_.snap(getParam(ParamId::FarLevel)); smFarWidth_.snap(getParam(ParamId::FarWidth));
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
    loudness_.prepare(sr_);
    smBlur_.setTime(0.02f, sr_);
    smBody_.setTime(0.02f, sr_);
    unmask_.prepare(sr_);
    haas_.prepare(sr_);
    early_.prepare(sr_);
    diffuser_.prepare(sr_);
    coupleBuf_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    roomB_.prepare(sr_, roomMaxSeconds_);
    roomBL_.assign(static_cast<size_t>(maxBlock_ + Convolver::kBlock), 0.0f);
    roomBR_.assign(static_cast<size_t>(maxBlock_ + Convolver::kBlock), 0.0f);
    smRoomMorph_.setTime(0.05f, sr_);
    body_.prepare(sr_, 0xB0D1B0D1ull);
    patina_.prepare(sr_, 0x9A7104ull);
    blur_.prepare(sr_, 0x5EED5EEDull);
    auxRng_.seed(0xA5A5A5A5ull);
    tideDrift_.init(auxRng_); rotDrift_.init(auxRng_);
    vecDriftX_.init(auxRng_); vecDriftY_.init(auxRng_);
    dcXL_ = dcXR_ = dcYL_ = dcYR_ = 0.0f;
    lastRootPc_ = -1;
    readParams();
    brain_.reset(rng_.fork(), rootNote_ - 12);   // the brain's root lives an octave below the key root
    brain2_.reset(0x2B2A1Full, rootNote_ - 12);  // its own stream, so switching it on never moves the first one
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
    const Preset& p = preset(index);
    const bool ok = ambient::applyPreset(p, [this](ParamId id, float v) { setParam(id, v); });
    return applyPresetModulation(p) && ok;
}

bool Engine::applySoundPreset(int index)
{
    if (index < 0 || index >= numPresets()) return false;
    const Preset& p = preset(index);
    // Modulation belongs to the sound layer, not to Cosmos: loading a sound preset brings its
    // matrix and its envelope shapes with it.
    const bool ok = ambient::applyPreset(p, [this](ParamId id, float v) { setParam(id, v); }, PresetScope::Sound);
    return applyPresetModulation(p) && ok;
}

bool Engine::applyCosmosPreset(int index)
{
    if (index < 0 || index >= numCosmosPresets()) return false;
    return ambient::applyPreset(cosmosPreset(index), [this](ParamId id, float v) { setParam(id, v); }, PresetScope::Cosmos);
}

bool Engine::applyZPreset(int index)
{
    if (index < 0 || index >= numZPresets()) return false;
    return ambient::applyPreset(zPreset(index), [this](ParamId id, float v) { setParam(id, v); }, PresetScope::ZPlane);
}

bool Engine::applyStrikePreset(int index)
{
    if (index < 0 || index >= numStrikePresets()) return false;
    return ambient::applyPreset(strikePreset(index), [this](ParamId id, float v) { setParam(id, v); }, PresetScope::Strike);
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
    // Pick up an edit made on the message thread (fixed-size object, no allocation), and restart
    // from the cursor so a route changed while it plays does not jump.
    const int rv = routeVersion_.load(std::memory_order_acquire);
    if (rv != routeSeen_) { route_ = routePending_; routeSeen_ = rv; routeWasActive_ = false; }
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

void Engine::setTexture(int slot, const float* mono, int n, double sampleRate, double baseHz, bool seamless)
{
    if (slot < 0 || slot >= kSlots) return;
    const int active = textureActive_[slot].load(std::memory_order_acquire);
    const int target = active < 0 ? 0 : 1 - active;
    // Wait until the audio thread no longer holds the target buffer (it publishes the index it
    // used last); bounded, so a host without a running audio thread cannot hang us.
    for (int spin = 0; spin < 200000 && textureInUse_[slot].load(std::memory_order_acquire) == target; ++spin) { }
    Texture& t = textures_[slot][target];
    t.mono.assign(mono, mono + std::max(n, 0));
    t.sampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    t.baseHz = baseHz > 0.0 ? baseHz : 261.6256;
    t.seamless = seamless;
    t.measure();
    textureActive_[slot].store(target, std::memory_order_release);
}

void Engine::clearTexture(int slot)
{
    if (slot < 0 || slot >= kSlots) return;
    textureActive_[slot].store(-1, std::memory_order_release);
}

// Where Match would put partial h: on the degree of the current scale nearest to it, counted in
// periods of that scale from the fundamental. For a twelve-tone scale the seventh partial moves
// from 3369 cents to 3400 and the fifth from 2786 to 2800 -- small moves, and after them a chord
// in that scale does not beat. The timbre scale is excluded, because it is itself computed from
// the spectrum: a scale that follows the partials and partials that follow the scale would chase
// each other round in a circle, and neither would mean anything.
double Engine::matchedPartialRatio(int h) const
{
    if (h < 1) return 1.0;
    const FixedScale& s = *scale_;
    if (scale_ == &scales_[kTimbreScaleIndex] || s.count <= 0 || !(s.period > 1.0)) return static_cast<double>(h);
    const double logP = std::log(s.period);
    const double x = std::log(static_cast<double>(h)) / logP;         // the partial, in periods above f0
    const int n = static_cast<int>(std::floor(x));
    const double within = std::pow(s.period, x - n);                     // 1 .. period
    double best = s.ratios[0], bestErr = 1e9;
    for (int d = 0; d <= s.count; ++d) {
        const double r = d < s.count ? s.ratios[d] : s.period;
        const double err = std::fabs(std::log(r / within));
        if (err < bestErr) { bestErr = err; best = r; }
    }
    return std::pow(s.period, n) * best;
}

double Engine::frequencyOf(int note) const
{
    // Purity blends between 12-TET (0) and the chosen scale (1) in the log domain: at 1 the
    // partials of different notes lock, at 0 they beat like a piano; in between the beating
    // slows down as the intervals close in on their ratios.
    const double pure = scaleFrequency(*scale_, note, rootNote_, refPitch_, snapKeys_);
    const double p = purityCur_;
    double f = pure;
    if (p < 0.9999) {
        const double et = refPitch_ * std::pow(2.0, (note - 69) / 12.0);
        f = std::exp(std::log(et) + (std::log(pure) - std::log(et)) * clampv(p, 0.0, 1.0));
    }
    // Adaptive intonation: the note's own offset and the shared comma offset, both in cents,
    // scaled by the amount so that the knob glides everything home rather than switching it.
    if (adaptAmt_ > 0.0f && note >= 0 && note < 128)
        f *= std::pow(2.0, static_cast<double>(adaptAmt_) * (static_cast<double>(adaptCents_[note]) + commaCents_) / 1200.0);
    // The stretched octave. Listeners prefer octaves a little wider than 2:1 -- ten to twenty
    // cents at the extremes of the range (Ward 1954; Terhardt) -- and a piano is tuned that
    // way (the Railsback curve). Every octave away from the reference pitch is widened by
    // Stretch cents, in both directions, so the reference itself does not move:
    //     log2 f' = log2 A4 + (1 + s/1200) (log2 f - log2 A4)
    if (stretchCents_ > 0.0f && f > 0.0 && refPitch_ > 0.0)
        f = refPitch_ * std::pow(f / refPitch_, 1.0 + stretchCents_ / 1200.0);
    return f;
}

// The pure offset for a note arriving into a chord. Against every sounding voice the interval
// is reduced to an octave and matched to the nearest small-integer ratio (5-limit and the
// septimal tritone); the offset that would make that interval exact is weighted by the ratio's
// simplicity -- the fifth speaks louder than the minor seventh -- and the weighted mean is the
// answer, capped at thirty cents so a note that fits nothing is not thrown across a quarter tone.
// The sounding voices' frequencies come through frequencyOf, so they carry their own offsets
// and the shared comma, and a note tuned against them lands pure in the world as it is now.
float Engine::adaptiveOffset(int note) const
{
    if (note < 0 || note > 127) return 0.0f;
    static const struct { int num, den; } kRatios[] = {
        { 1, 1 }, { 16, 15 }, { 9, 8 }, { 6, 5 }, { 5, 4 }, { 4, 3 }, { 7, 5 }, { 3, 2 }, { 8, 5 }, { 5, 3 }, { 9, 5 }, { 15, 8 } };
    // The plain frequency: what the note gets with no offset of its own (the comma still applies,
    // and cancels in every ratio below because the sounding voices carry it too).
    double plain = frequencyOf(note);
    if (adaptAmt_ > 0.0f) plain /= std::pow(2.0, static_cast<double>(adaptAmt_) * static_cast<double>(adaptCents_[note]) / 1200.0);
    double sum = 0.0, weight = 0.0;
    for (const auto& v : voices_) {
        if (!v.isActive() || v.note() == note) continue;
        const double g = frequencyOf(v.note());
        if (!(g > 0.0) || !(plain > 0.0)) continue;
        double r = plain / g;
        int oct = 0;
        while (r >= 2.0) { r *= 0.5; ++oct; }
        while (r < 1.0)  { r *= 2.0; --oct; }
        double bestCents = 1e9; int bestNum = 1, bestDen = 1;
        for (const auto& q : kRatios) {
            const double c = 1200.0 * std::log2(static_cast<double>(q.num) / q.den / r);   // + : the pure ratio is above
            if (std::fabs(c) < std::fabs(bestCents)) { bestCents = c; bestNum = q.num; bestDen = q.den; }
        }
        {   // the octave above the last entry is the next unison
            const double c = 1200.0 * std::log2(2.0 / r);
            if (std::fabs(c) < std::fabs(bestCents)) { bestCents = c; bestNum = 1; bestDen = 1; }
        }
        if (std::fabs(bestCents) > 35.0) continue;   // fits nothing: this voice has no say
        const double w = 1.0 / std::max(1.0, std::log2(static_cast<double>(bestNum * bestDen)));
        sum += w * bestCents;
        weight += w;
    }
    if (weight <= 0.0) return 0.0f;
    return static_cast<float>(clampv(sum / weight, -30.0, 30.0));   // raw: frequencyOf scales it by the amount
}

const char* Engine::stemName(int i)
{
    static const char* const names[kNumStems] = { "near", "far", "cosmos", "room" };
    return (i >= 0 && i < kNumStems) ? names[i] : "";
}

const Voice* Engine::loudestVoice() const
{
    const Voice* best = nullptr;
    for (const auto& v : voices_)
        if (v.isActive() && (best == nullptr || v.level() > best->level())) best = &v;
    return best;
}

int Engine::displayPartials(float* out, int maxCount) const
{
    const Voice* v = loudestVoice();
    return v != nullptr ? v->displayPartials(out, maxCount) : 0;
}

float Engine::displayFrequency() const
{
    const Voice* v = loudestVoice();
    return v != nullptr ? static_cast<float>(v->frequency()) : 0.0f;
}

int Engine::displaySlotPartials(int slot, float* out, int maxCount) const
{
    const Voice* v = loudestVoice();
    return v != nullptr ? v->displaySlotPartials(slot, out, maxCount) : 0;
}

int Engine::displayGrains(int slot, SourceSlot::GrainInfo* out, int maxCount) const
{
    const Voice* v = loudestVoice();
    const int k = slot < 0 ? 0 : (slot >= kSlots ? kSlots - 1 : slot);
    const int a = textureActive_[k].load(std::memory_order_relaxed);
    const int len = a >= 0 ? static_cast<int>(textures_[k][a].mono.size()) : 0;
    return (v != nullptr && len > 0) ? v->displayGrains(slot, out, maxCount, len) : 0;
}

int Engine::voiceStage(VoiceStage* out, int maxCount) const
{
    int n = 0;
    for (const auto& v : voices_) {
        if (n >= maxCount) break;
        if (!v.isActive()) continue;
        out[n++] = { v.pan(), v.distance(), v.level(), v.note(), v.owner() };
    }
    return n;
}

int Engine::cosmosTap(float* out, int n) const
{
    n = clampv(n, 0, 4096);
    const int w = cosTapW_;
    for (int i = 0; i < n; ++i) out[i] = cosTap_[(w - n + i) & 4095];
    return n;
}

int Engine::outputTap(float* out, int n) const
{
    n = clampv(n, 0, kOutTapLen);
    const int w = outTapW_;
    for (int i = 0; i < n; ++i) out[i] = outTap_[(w - n + i) & (kOutTapLen - 1)];
    return n;
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
    // The modulation envelopes run on a phrase clock: it restarts when a note arrives into
    // silence, not on every note of a cluster, or a slow shape would never get anywhere.
    bool anySounding = false;
    for (const auto& v : voices_) if (v.isActive()) { anySounding = true; break; }
    if (!anySounding) for (double& t : envTime_) t = 0.0;
    randomPerNote_ = rng_.bipolar();
    Voice* v = allocate(note, owner);
    v->order = ++order_;
    // Adaptive intonation: the offset is decided once, as the note arrives, against what is
    // sounding then; a note already sounding in another voice keeps the offset it has.
    if (adaptAmt_ > 0.0f) {
        bool already = false;
        for (const auto& x : voices_) if (&x != v && x.isActive() && x.note() == note) already = true;
        if (!already) adaptCents_[note] = adaptiveOffset(note);
    } else adaptCents_[note] = 0.0f;
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

void Engine::setPressure(int note, float v)
{
    for (auto& x : voices_) if (x.isActive() && (note < 0 || x.note() == note)) x.setPressure(v);
}

void Engine::setSlide(int note, float v)
{
    for (auto& x : voices_) if (x.isActive() && (note < 0 || x.note() == note)) x.setSlide(v);
}

void Engine::setHeadYaw(float degrees)
{
    headYawDeg_.store(degrees, std::memory_order_relaxed);
}

void Engine::setWheel(float v)
{
    wheelTarget_ = clampv(v, 0.0f, 1.0f);
}

void Engine::setBend(int note, float normalised)
{
    const float semis = clampv(normalised, -1.0f, 1.0f) * bendRange_;
    for (auto& x : voices_) if (x.isActive() && (note < 0 || x.note() == note)) x.setBend(semis);
}

void Engine::allNotesOff()
{
    for (auto& h : midiHeld_) h = false;
    for (auto& v : voices_) v.noteOff();
}

// ---------------------------------------------------------------- parameters

} // namespace ambient
