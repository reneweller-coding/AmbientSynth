// AmbientSynth -- what the engine decides once per block: the modulation sources and the matrix,
// the clock, and readParams, which turns the parameter atomics into the structures the voices and
// the effects are given. Split out of Engine.cpp, which had grown past fourteen hundred lines.
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



// ---------------------------------------------------------------- modulation

void Engine::resetModulation()
{
    matrixPending_.clear();
    // A default shape every envelope starts from: up over a quarter of its length, down over the
    // rest. Time is scaled by the envelope's own Time parameter, so this is a shape, not a length.
    for (auto& e : envPending_) e.parse("0:0/1:1/4:0");
    modVersion_.fetch_add(1, std::memory_order_release);
}

bool Engine::applyPresetModulation(const Preset& p)
{
    resetModulation();
    bool ok = true;
    if (p.mod != nullptr && *p.mod) ok = matrixPending_.parse(p.mod) && ok;
    if (p.envs != nullptr && *p.envs) {
        const char* s = p.envs;
        for (int i = 0; i < kNumModEnvs && *s; ++i) {
            const char* end = s;
            while (*end && *end != '~') ++end;
            if (end > s) {
                char buf[256];
                const size_t len = static_cast<size_t>(end - s);
                if (len < sizeof(buf)) {
                    std::memcpy(buf, s, len);
                    buf[len] = 0;
                    ok = envPending_[i].parse(buf) && ok;
                }
            }
            s = (*end == '~') ? end + 1 : end;
        }
    }
    modVersion_.fetch_add(1, std::memory_order_release);
    return ok;
}

bool Engine::setModMatrixText(const char* text)
{
    if (!matrixPending_.parse(text)) return false;
    modVersion_.fetch_add(1, std::memory_order_release);
    return true;
}

bool Engine::setEnvShape(int index, const char* text)
{
    if (index < 0 || index >= kNumModEnvs) return false;
    if (!envPending_[index].parse(text)) return false;
    modVersion_.fetch_add(1, std::memory_order_release);
    return true;
}

int Engine::writeEnvShape(int index, char* buf, size_t cap) const
{
    if (index < 0 || index >= kNumModEnvs) return 0;
    return envPending_[index].write(buf, cap);
}

// One step of every modulator, then the matrix summed into modOut_. Called once per block, before
// readParams, so the values the parameters are read with already carry the modulation.
void Engine::stepModulation(float dt)
{
    // Pick up matrix or shape edits made on the message thread (fixed-size objects, no allocation).
    const int mv = modVersion_.load(std::memory_order_acquire);
    if (mv != modSeen_) {
        matrix_ = matrixPending_;
        for (int i = 0; i < kNumModEnvs; ++i) envShape_[i] = envPending_[i];
        modSeen_ = mv;
    }

    for (int i = 0; i < kNumLfos; ++i) {
        const int base = static_cast<int>(ParamId::Lfo1Shape) + i * 7;
        LfoSpec& sp = lfoSpec_[i];
        sp.shape  = static_cast<LfoShape>(clampv(static_cast<int>(std::lround(getParam(static_cast<ParamId>(base + 0)))), 0, kNumLfoShapes - 1));
        sp.rateHz = getParam(static_cast<ParamId>(base + 1));
        sp.phase  = getParam(static_cast<ParamId>(base + 2));
        sp.depth  = getParam(static_cast<ParamId>(base + 3));
        sp.mode   = static_cast<LfoMode>(clampv(static_cast<int>(std::lround(getParam(static_cast<ParamId>(base + 4)))), 0, kNumLfoModes - 1));
        sp.table  = static_cast<int>(std::lround(getParam(static_cast<ParamId>(base + 5))));
        const int sync = clampv(static_cast<int>(std::lround(getParam(static_cast<ParamId>(base + 6)))), 0, kNumSyncDivs - 1);
        if (syncOn(sync)) {
            // One cycle per division, and the phase follows the clock: the step below lands
            // exactly on the beat's position, so a synced LFO stays on the grid however long
            // it runs and wherever the transport jumps.
            sp.rateHz = static_cast<float>(syncHz(sync, bpm_));
            if (running_) lfo_[i].setPhase(static_cast<float>(syncPhase(sync, beat_)) - sp.rateHz * dt);
        }
        const Wavetable* table = userTable_.frames > 0 ? &userTable_ : nullptr;
        modSrc_[static_cast<int>(ModSource::Lfo1) + i] = lfo_[i].step(dt, sp, table);
    }

    envTime_ += dt;
    envHeld_ = false;
    for (const auto& v : voices_) if (v.isActive() && !v.isReleasing()) { envHeld_ = true; break; }
    for (int i = 0; i < kNumModEnvs; ++i) {
        const int base = static_cast<int>(ParamId::Env1Mode) + i * 4;
        ModEnvSpec& sp = envSpec_[i];
        sp.mode      = static_cast<EnvMode>(clampv(static_cast<int>(std::lround(getParam(static_cast<ParamId>(base + 0)))), 0, kNumEnvModes - 1));
        sp.timeScale = std::max(0.01f, getParam(static_cast<ParamId>(base + 1)));
        const int sync = clampv(static_cast<int>(std::lround(getParam(static_cast<ParamId>(base + 3)))), 0, kNumSyncDivs - 1);
        if (syncOn(sync))   // synced: the whole shape spans one division
            sp.timeScale = static_cast<float>(std::max(0.01, syncSeconds(sync, bpm_) / std::max(static_cast<double>(envShape_[i].length()), 1e-3)));
        sp.depth     = getParam(static_cast<ParamId>(base + 2));
        const float t = static_cast<float>(envTime_) / sp.timeScale;
        modSrc_[static_cast<int>(ModSource::Env1) + i] = envShape_[i].at(t, sp.mode, envHeld_) * sp.depth;
    }

    // Everything else that can drive something. Sources are bipolar; the ones that are naturally
    // 0..1 are mapped to -1..1 here, and a route's "unipolar" flag maps them back.
    const Voice* loud = loudestVoice();
    auto uni = [](float v) { return 2.0f * clampv(v, 0.0f, 1.0f) - 1.0f; };
    modSrc_[static_cast<int>(ModSource::Amp)] = uni(loud ? loud->level() : 0.0f);
    for (int m = 0; m < 8; ++m)
        modSrc_[static_cast<int>(ModSource::MacroA) + m] = uni(getParam(static_cast<ParamId>(static_cast<int>(ParamId::MacroA) + m)));
    for (int k = 0; k < 4; ++k)
        modSrc_[static_cast<int>(ModSource::Kura1) + k] = std::sin(kuraPhase_[k]);
    modSrc_[static_cast<int>(ModSource::Note)] = uni(loud ? (loud->note() - 24) / 84.0f : 0.5f);
    modSrc_[static_cast<int>(ModSource::Velocity)] = uni(loud ? loud->level() : 0.0f);
    modSrc_[static_cast<int>(ModSource::Distance)] = uni(loud ? loud->distance() : 0.5f);
    modSrc_[static_cast<int>(ModSource::RandomPerNote)] = randomPerNote_;
    modSrc_[static_cast<int>(ModSource::None)] = 0.0f;

    std::memset(modOut_, 0, sizeof(modOut_));
    matrix_.apply(modSrc_, modOut_);
    // Performance state is never a target: modulating the morph position or the map cursor from
    // inside would fight the hand that is holding them.
    for (const ParamDesc& d : paramTable())
        if (isPerformanceParam(d.id)) modOut_[static_cast<int>(d.id)] = 0.0f;
}

// ---------------------------------------------------------------- clock

void Engine::stepClock(double dt)
{
    const int src = clampv(static_cast<int>(std::lround(getParam(ParamId::ClockSource))), 0, kNumClockSources - 1);
    const double internal = clampv(getParam(ParamId::Tempo), 20.0f, 300.0f);
    const bool run = getParam(ParamId::ClockRun) >= 0.5f;
    midiSilence_ += dt;
    const bool useHost = src == static_cast<int>(ClockSource::Host) && hostSeen_ && hostBpm_ > 1.0;
    const bool useMidi = src == static_cast<int>(ClockSource::Midi) && midiTicks_ >= 24 && midiBpm_ > 1.0 && midiSilence_ < 2.0;
    if (useHost)      { bpm_ = hostBpm_; beat_ = hostBeat_; running_ = hostPlaying_; }
    else if (useMidi) { bpm_ = midiBpm_; beat_ = midiBeat_; running_ = midiRunning_; }
    else {
        // The internal clock: the Tempo knob, counting beats while Run is on. Also what Host and
        // MIDI fall back to when nothing arrives -- the standalone has no play head.
        bpm_ = internal;
        running_ = run;
        if (run) intBeat_ += bpm_ / 60.0 * dt;
        beat_ = intBeat_;
    }
    tempoOut_.store(bpm_, std::memory_order_relaxed);
    beatOut_.store(beat_, std::memory_order_relaxed);
    runningOut_.store(running_, std::memory_order_relaxed);
}

void Engine::midiClockTick(double interval)
{
    // 24 ticks a quarter. The tempo settles over a beat's worth of ticks, so jitter in the
    // interface does not wobble every synced LFO.
    if (interval > 0.002 && interval < 2.0) {
        const double bpm = 60.0 / (24.0 * interval);
        midiBpm_ = midiTicks_ < 24 || midiBpm_ <= 1.0 ? bpm : midiBpm_ + (bpm - midiBpm_) * 0.08;
    }
    if (midiRunning_) midiBeat_ += 1.0 / 24.0;
    ++midiTicks_;
    midiSilence_ = 0.0;
}

void Engine::midiClockStart()    { midiBeat_ = 0.0; midiRunning_ = true; }
void Engine::midiClockContinue() { midiRunning_ = true; }
void Engine::midiClockStop()     { midiRunning_ = false; }

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
        // Modulation is added after the inertia glide: a modulator moves at its own rate, it is
        // not slewed by the setting that exists to slow down the performer's hand.
        const float mod = modOut_[i];
        if (inertiaCoef >= 1.0f || d.kind != ParamKind::Float || isPerformanceParam(id)) {
            inertiaCur_[i] = target;
            return mod != 0.0f ? clampv(target + mod, d.min, d.max) : target;
        }
        const float span = std::max(d.max - d.min, 1e-9f);
        const float pc = std::pow(clampv((inertiaCur_[i] - d.min) / span, 0.0f, 1.0f), d.skew);
        const float pt = std::pow(clampv((target - d.min) / span, 0.0f, 1.0f), d.skew);
        inertiaCur_[i] = d.min + span * std::pow(pc + (pt - pc) * inertiaCoef, 1.0f / d.skew);
        return mod != 0.0f ? clampv(inertiaCur_[i] + mod, d.min, d.max) : inertiaCur_[i];
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
    {   // Source slots: 24 fields each. Source 2 and 3 are laid out consecutively from their Type;
        // Source 1's fields are scattered (its level and spectrum are the classic Oscillator
        // parameters, read above), so every slot goes through one table of ids.
        // The ids come from Params.h; they used to be written out here and again in the editor.
        for (int k = 0; k < kSlots; ++k) {
            // Source 1's level and spectrum were read into vp_ above; reading them again would step
            // their inertia twice per block, so they are copied instead.
            const ParamId* ids = slotParamIds(k);
            auto at = [&](int off) { return g(ids[off]); };
            SlotParams& s = vp_.slot[k];
            if (k == 0) {
                s.level = vp_.level; s.partials = vp_.partials; s.tilt = vp_.tilt; s.bright = vp_.brightness;
                s.oddEven = vp_.oddEven; s.inharm = vp_.inharmonic; s.shimmer = vp_.shimmer; s.shimmerRate = vp_.shimmerRate;
            } else {
                s.level       = at(1);
                s.partials    = static_cast<int>(std::lround(at(17)));
                s.tilt        = at(18);
                s.bright      = at(19);
                s.oddEven     = at(20);
                s.inharm      = at(21);
                s.shimmer     = at(22);
                s.shimmerRate = at(23);
            }
            s.type          = static_cast<SourceType>(clampv(static_cast<int>(std::lround(at(0))), 0, kNumSourceTypes - 1));
            s.drift         = at(25);
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
            {   // Sync: a grain (or a crackle) per division instead of per second
                const int dsync = clampv(static_cast<int>(std::lround(getParam(ids[24]))), 0, kNumSyncDivs - 1);
                if (syncOn(dsync)) s.density = static_cast<float>(syncHz(dsync, bpm_));
            }
            s.follow        = at(12) >= 0.5f;
            s.grains        = static_cast<int>(std::lround(at(13)));
            s.spread        = at(14);
            s.noise         = static_cast<NoiseKind>(clampv(static_cast<int>(std::lround(at(15))), 0, kNumNoiseKinds - 1));
            s.noiseQ        = at(16);
        }
        vp_.userTable = userTable_.frames > 0 ? &userTable_ : nullptr;
        const int a = textureActive_.load(std::memory_order_acquire);
        textureInUse_.store(a, std::memory_order_release);
        vp_.texture = (a >= 0 && !textures_[a].empty()) ? &textures_[a] : nullptr;
    }
    masterGain_     = g(ParamId::MasterGain);   // through g(), so the matrix can reach it
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
    vp_.filterOn    = g(ParamId::FilterOn) >= 0.5f;
    vp_.filterParallel = std::lround(g(ParamId::ZRoute)) == 1;
    vp_.filterModel = static_cast<int>(std::lround(g(ParamId::FilterModel)));
    vp_.filterDrive = g(ParamId::FilterDrive);
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
    vp_.phaseWidth  = g(ParamId::PhaseWidth);
    vp_.phaseRate   = g(ParamId::PhaseRate);
    vp_.doppler     = g(ParamId::Doppler);
    vp_.externalise = g(ParamId::Externalise);
    vp_.strikeLevel = g(ParamId::StrikeLevel);
    vp_.strikeType  = static_cast<int>(std::lround(g(ParamId::StrikeType)));
    vp_.strikeDecay = g(ParamId::StrikeDecay);
    vp_.strikeDamp  = g(ParamId::StrikeDamp);
    vp_.strikeBrain = std::lround(g(ParamId::StrikeWho)) == 1;
    tide_       = g(ParamId::Tide);
    tidePeriod_ = g(ParamId::TidePeriod);
    vp_.pitchMul = tide_ > 0.0f ? std::pow(2.0f, tide_ * tideDrift_.value() / 1200.0f) : 1.0f;
    blurMix_    = g(ParamId::BlurMix);
    blur_.set(g(ParamId::BlurSmear));
    farRotate_  = g(ParamId::FarRotate);

    depth_       = g(ParamId::Depth);
    keysDepth_   = g(ParamId::KeysDepth);
    arcAmount_   = g(ParamId::ArcAmount);
    arcPeriodMin_ = g(ParamId::ArcPeriod);
    // Sync choices: when set, the division at the current tempo replaces the free knob.
    auto syncedSeconds = [this](ParamId sync, float free) {
        const int d = clampv(static_cast<int>(std::lround(getParam(sync))), 0, kNumSyncDivs - 1);
        return syncOn(d) ? static_cast<float>(syncSeconds(d, bpm_)) : free;
    };
    auto syncedHz = [this](ParamId sync, float free) {
        const int d = clampv(static_cast<int>(std::lround(getParam(sync))), 0, kNumSyncDivs - 1);
        return syncOn(d) ? static_cast<float>(syncHz(d, bpm_)) : free;
    };
    arcPeriodMin_ = syncedSeconds(ParamId::ArcSync, arcPeriodMin_ * 60.0f) / 60.0f;

    bp_.on          = g(ParamId::BrainOn) >= 0.5f;
    bp_.density     = static_cast<int>(std::lround(g(ParamId::BrainDensity)));
    bp_.rateSeconds = syncedSeconds(ParamId::BrainSync, g(ParamId::BrainRate));
    bp_.holdMin     = g(ParamId::BrainHoldMin);
    bp_.holdMax     = g(ParamId::BrainHoldMax);
    bp_.low         = static_cast<int>(std::lround(g(ParamId::BrainLow)));
    bp_.high        = static_cast<int>(std::lround(g(ParamId::BrainHigh)));
    bp_.consonance  = g(ParamId::BrainConsonance);
    bp_.wander      = g(ParamId::BrainWander);
    brainQuant_     = clampv(static_cast<int>(std::lround(g(ParamId::BrainQuantize))), 0, kNumSyncDivs - 1);
    // Autoplay. In Chords the conductor keeps the cluster full and exchanges one voice at a time;
    // the rate can come from the clock instead of the seconds knob.
    bp_.mode         = static_cast<BrainMode>(clampv(static_cast<int>(std::lround(g(ParamId::AutoMode))), 0, kNumBrainModes - 1));
    bp_.voiceLead    = g(ParamId::AutoLead);
    bp_.chordTension = g(ParamId::AutoTension);
    bp_.rootMove     = g(ParamId::AutoRootMove);
    if (bp_.mode == BrainMode::Chords) bp_.rateSeconds = syncedSeconds(ParamId::AutoSync, g(ParamId::AutoRate));
    {   // Step is a trigger: it fires on the rising edge and the host's switch is left alone.
        const bool now = g(ParamId::AutoStep) >= 0.5f;
        if (now && !autoStepWas_) brain_.requestStep();
        autoStepWas_ = now;
        if (autoStepAsked_.exchange(false, std::memory_order_relaxed)) brain_.requestStep();
    }
    vp_.pressDistance = g(ParamId::PressDistance);
    vp_.pressBright   = g(ParamId::PressBright);
    vp_.pressLevel    = g(ParamId::PressLevel);
    vp_.slideCutoff   = g(ParamId::SlideCutoff);
    vp_.slideZ        = g(ParamId::SlideZ);
    bendRange_        = g(ParamId::BendRange);
    // The second conductor. Its root follows the first one's plus an interval, so the two stay
    // in one harmony however far the first one's root wanders.
    brain2On_        = g(ParamId::Brain2On) >= 0.5f;
    bp2_.on          = brain2On_;
    bp2_.density     = static_cast<int>(std::lround(g(ParamId::Brain2Density)));
    bp2_.rateSeconds = g(ParamId::Brain2Rate);
    bp2_.holdMin     = g(ParamId::Brain2HoldMin);
    bp2_.holdMax     = g(ParamId::Brain2HoldMax);
    bp2_.low         = static_cast<int>(std::lround(g(ParamId::Brain2Low)));
    bp2_.high        = static_cast<int>(std::lround(g(ParamId::Brain2High)));
    bp2_.consonance  = g(ParamId::Brain2Consonance);
    bp2_.wander      = 0.0f;   // it follows the first conductor's root instead of wandering itself
    brain2Depth_     = g(ParamId::Brain2Depth);
    brain2Interval_  = static_cast<int>(std::lround(g(ParamId::Brain2Interval)));

    // Hour-scale arc: a very slow drift that leans on density, brightness and depth.
    const float a = arc_.value() * arcAmount_;
    bp_.density = clampv(bp_.density + static_cast<int>(std::lround(a * 2.0f)), 1, ClusterBrain::kSlots);
    vp_.brightness = clampv(vp_.brightness * (1.0f + 0.25f * a), 0.0f, 1.0f);
    depth_ = clampv(depth_ * (1.0f + 0.3f * a), 0.0f, 1.0f);

    ensemble_.set(g(ParamId::EnsembleMix), g(ParamId::EnsembleDepth), syncedHz(ParamId::EnsembleSync, g(ParamId::EnsembleRate)));
    delay_.set(syncedSeconds(ParamId::DelaySyncL, g(ParamId::DelayTimeL)), syncedSeconds(ParamId::DelaySyncR, g(ParamId::DelayTimeR)),
               g(ParamId::DelayFeedback), g(ParamId::DelayCross), g(ParamId::DelayDamp), g(ParamId::DelayAbsorb));
    delayMix_   = g(ParamId::DelayMix);
    delayToFar_ = g(ParamId::DelayToFar);
    delay2_.set(syncedSeconds(ParamId::Delay2SyncL, g(ParamId::Delay2TimeL)), syncedSeconds(ParamId::Delay2SyncR, g(ParamId::Delay2TimeR)),
                g(ParamId::Delay2Feedback), g(ParamId::Delay2Cross), g(ParamId::Delay2Damp), g(ParamId::Delay2Absorb));
    delay2Mix_   = g(ParamId::Delay2Mix);
    delay2ToFar_ = g(ParamId::Delay2ToFar);
    cloud_.set(syncedHz(ParamId::CloudSync, g(ParamId::CloudDensity)), g(ParamId::CloudSize), g(ParamId::CloudPitch), g(ParamId::CloudSpray), g(ParamId::CloudLevel));
    cloudSend_ = g(ParamId::CloudSend);
    nearReverb_.setSpace(0.3f, 20000.0f);
    nearReverb_.set(0.6f, g(ParamId::NearDecay), g(ParamId::NearDamp), 5.0f, false, g(ParamId::NearMix));
    unmask_.set(g(ParamId::FarUnmask));
    bodyLevel_ = g(ParamId::BodyLevel);
    bodyPitch_ = g(ParamId::BodyPitch);
    body_.set(static_cast<BodyMaterial>(clampv(static_cast<int>(std::lround(g(ParamId::BodyMaterial))), 0, kNumBodyMaterials - 1)),
              static_cast<float>(frequencyOf(brain_.root())) * bodyPitch_ * vp_.pitchMul,
              g(ParamId::BodyDecay), g(ParamId::BodyTone), g(ParamId::BodySpread));
    patina_.set(g(ParamId::PatinaAmount), g(ParamId::PatinaWow), g(ParamId::PatinaHiss), g(ParamId::PatinaAge));
    farReverb_.setSpace(g(ParamId::FarAsym), g(ParamId::FarHighcut));
    farReverb_.set(g(ParamId::FarSize), g(ParamId::FarDecay), g(ParamId::FarDamp), g(ParamId::FarPreDelay), g(ParamId::FarFreeze) >= 0.5f, 1.0f);
    farLevel_ = g(ParamId::FarLevel);
    midSide_.set(g(ParamId::BassMono), g(ParamId::SideAir), g(ParamId::Width));
    midSide_.setTilt(g(ParamId::Tilt2), g(ParamId::TiltPivot));
    diffuser_.set(g(ParamId::FarDiffuse));
    sympathy_ = g(ParamId::Sympathy);
    vp_.sympathy = sympathy_;
    roomLevel_    = g(ParamId::RoomLevel);
    roomSource_   = static_cast<int>(std::lround(g(ParamId::RoomSource)));
    roomPreDelay_ = static_cast<int>(g(ParamId::RoomPreDelay) * 0.001f * static_cast<float>(sr_));
    roomHighcut_  = g(ParamId::RoomHighcut);
    roomMorph_    = hasImpulseB_ ? g(ParamId::RoomMorph) : 0.0f;
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
        brain2_.reset(0x2B2A1Full + static_cast<uint64_t>(seed_), 48 + rootPc);
        arc_.init(rng_);
    }
}

} // namespace ambient
