// AmbientSynth -- what the engine renders: process() with its denormal control and its sleep
// check, and renderChunk, which is the signal path from the voices through both planes to the
// master. Split out of Engine.cpp.
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

// ---------------------------------------------------------------- audio

void Engine::process(float* L, float* R, int n)
{
#if AMBIENT_HAS_MXCSR
    const unsigned int savedCsr = _mm_getcsr();
    _mm_setcsr(savedCsr | 0x8040);   // flush-to-zero + denormals-are-zero
#endif
#if AMBIENT_HAS_FPCR
    // The same thing on ARM: FPCR bit 24 is flush-to-zero. Without it the Quest ran every decaying
    // reverb tail and envelope into denormals, where the FPU falls off a cliff.
    uint64_t savedFpcr = 0;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(savedFpcr));
    const uint64_t fpcrFz = savedFpcr | (1ull << 24);
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcrFz));
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
    if (tide_ > 0.0f) tideDrift_.update(static_cast<float>(n / sr_), 1.0f / (60.0f * std::max(tidePeriod_, 0.5f)), auxRng_);
    if (farRotate_ > 0.0f) rotDrift_.update(static_cast<float>(n / sr_), 0.008f, auxRng_);
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
        // The coupling is deliberately NOT symmetric. With every pair pulling on the other
        // equally, a high Coherence settles into exact synchrony and stays there: four
        // oscillators behaving as one, which is the opposite of what this section is for. An
        // antisymmetric perturbation of the weights -- each oscillator pulled a little harder by
        // the one behind it in the ring than by the one in front -- has no such fixed point, so
        // the bank locks in frequency and keeps a slowly turning spread of phase. That is what a
        // ring of coupled biological oscillators does, and it is why they never look identical.
        for (int i = 0; i < 4; ++i) {
            float coupling = 0.0f;
            for (int j = 0; j < 4; ++j) {
                const float w = 1.0f + 0.22f * std::sin(kTwoPi * static_cast<float>(j - i) / 4.0f);
                coupling += w * std::sin(kuraPhase_[j] - kuraPhase_[i]);
            }
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
    stepClock(n / sr_);
    stepModulation(static_cast<float>(n / sr_));
    readParams();
    if (retune_)   // tuning purity / drift: every sounding voice glides to its current frequency
        for (auto& v : voices_) if (v.isActive()) v.setTargetFrequency(frequencyOf(v.note()));

    int pos = 0;
    while (pos < n) {
        const int chunk = std::min(n - pos, maxBlock_);
        stemPos_ = pos;
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
#if AMBIENT_HAS_FPCR
    __asm__ __volatile__("msr fpcr, %0" : : "r"(savedFpcr));
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
    // The second conductor plays on one plane, deep, so it reads as a background line rather
    // than as more of the same cluster.
    auto emit2 = [this](const BrainEvent& e) {
        if (e.type == BrainEvent::Type::NoteOn) startNote(e.note, e.velocity, OwnerBrain2, clampv(depth_ * brain2Depth_, 0.0f, 1.0f));
        else stopNote(e.note, OwnerBrain2);
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
        {   // Quantize: the decisions are held back and made at the next note value of the clock,
            // so the conductor lands on the grid instead of wherever the dice fell. All of the
            // waiting time is handed over at the tick, so the mean rate is unchanged.
            const double dt = len / sr_;
            if (syncOn(brainQuant_) && running_) {
                quantAcc_ += dt;
                const double b = syncBeats(brainQuant_);
                const double now = std::floor(beat_ / b), before = std::floor(lastBeat_ / b);
                lastBeat_ = beat_;
                if (now != before) { brain_.update(quantAcc_, bp_, anchor, freqOf, emit); quantAcc_ = 0.0; }
            } else {
                quantAcc_ = 0.0; lastBeat_ = beat_;
                brain_.update(dt, bp_, anchor, freqOf, emit);
            }
            if (brain2On_) {
                brain2_.setRoot(clampv(brain_.root() + brain2Interval_, 0, 127));
                brain2_.update(dt, bp2_, -1, freqOf, emit2);
            }
        }
        const float* fm = (fbOn && fbFm_ > 0.0f) ? fbm + p : nullptr;
        const float* couple = sympathy_ > 0.0f ? coupleBuf_.data() + p : nullptr;
        for (auto& v : voices_) if (v.isActive()) { anyVoice = true; v.render(nl + p, nr + p, fl + p, fr + p, len, vp_, fm, couple); }
    }
    if (asleep_ && !anyVoice) {   // sleeping: the whole effect chain is skipped, output stays silent
        std::memset(L, 0, bytes); std::memset(R, 0, bytes);
        return;
    }
    if (fbOn && fbBus_ > 0.0f)
        for (int i = 0; i < n; ++i) { nl[i] += fbl[i] * fbBus_; nr[i] += fbr[i] * fbBus_; }

    // Blur: the near bus through a spectral smear before anything else hears it, so a note's
    // attack is wiped into texture and one note flows into the next.
    if (blurMix_ > 0.0f || smBlur_.value > 1.0e-4f) {
        float* bl = nebL_.data(); float* br = nebR_.data();
        blur_.process(nl, nr, bl, br, n);
        for (int i = 0; i < n; ++i) { const float m = smBlur_.next(blurMix_); nl[i] = nl[i] * (1.0f - m) + bl[i] * m; nr[i] = nr[i] * (1.0f - m) + br[i] * m; }
    }
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
            cosTap_[(cosTapW_ + i) & 4095] = 0.5f * (cl[i] + cr[i]) * ret;   // what the return adds, for the picture
            if (stems_ != nullptr) { stems_[4][stemPos_ + i] = cl[i] * ret; stems_[5][stemPos_ + i] = cr[i] * ret; }
        }
        cosTapW_ = (cosTapW_ + n) & 4095;
    } else {
        for (int i = 0; i < n; ++i) cosTap_[(cosTapW_ + i) & 4095] = 0.0f;
        cosTapW_ = (cosTapW_ + n) & 4095;
        if (stems_ != nullptr) for (int i = 0; i < n; ++i) { stems_[4][stemPos_ + i] = 0.0f; stems_[5][stemPos_ + i] = 0.0f; }
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
    diffuser_.process(fl, fr, n);
    {
        // Air saturates. A real room is not linear at a peak -- the medium itself gives a little,
        // and a dense cluster fired into a hall comes back thickened rather than reflected. This
        // is a very gentle asymmetric shaper between the diffuser and the reverb's own feedback
        // network, so only the peaks are touched and the tail is what changes character, not the
        // level. Amount rides on Diffuse, which is already the "how much room" control, so there
        // is no new knob for a thing nobody would know how to set.
        const float amt = 0.35f * farDiffuse_;
        if (amt > 0.001f) {
            const float pre = 1.0f + 2.2f * amt, post = 1.0f / (1.0f + 0.55f * amt);
            for (int i = 0; i < n; ++i) {
                const float a = fl[i] * pre, b = fr[i] * pre;
                // Asymmetric on purpose: a touch of second harmonic reads as warmth, where the
                // symmetric curve of a plain tanh only ever reads as compression.
                fl[i] = (std::tanh(a) + 0.06f * a * a * (a > 0.0f ? 1.0f : -1.0f) * (1.0f - std::fabs(std::tanh(a)))) * post;
                fr[i] = (std::tanh(b) + 0.06f * b * b * (b > 0.0f ? 1.0f : -1.0f) * (1.0f - std::fabs(std::tanh(b)))) * post;
            }
        }
    }
    farReverb_.process(fl, fr, n);
    if (farRotate_ > 0.0f) {   // the background slowly turns: left and right rotate into each other
        const float a = rotDrift_.value() * farRotate_ * 0.6f, c = std::cos(a), sn = std::sin(a);
        for (int i = 0; i < n; ++i) { const float l = fl[i], r = fr[i]; fl[i] = l * c - r * sn; fr[i] = l * sn + r * c; }
    }
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

    // Unmasking: the background gives way to the foreground band by band, before the two planes
    // are summed (the near bus is the side chain, and it is finished by now).
    unmask_.process(nl, nr, fl, fr, n);
    // The Haas band, on the foreground only: the background has the reverb's own width and does
    // not need help. After the unmask, so what the side chain measured is the plane as it was.
    haas_.process(nl, nr, n);
    // Sympathy: this block's foreground is what the voices will hear of each other in the next
    // one. A block of delay is what makes the loop safe, and at these depths inaudible.
    if (sympathy_ > 0.0f && static_cast<int>(coupleBuf_.size()) >= n)
        for (int i = 0; i < n; ++i) coupleBuf_[static_cast<size_t>(i)] = 0.5f * (nl[i] + nr[i]);

    const float master = dbToGain(masterGain_);
    for (int i = 0; i < n; ++i) {
        const float far = smFarLevel_.next(farLevel_);
        // The width of the background alone. A mix in which everything is spread as wide as it
        // will go has no depth left: the far plane pulled in towards the centre while the
        // foreground stays wide is the funnel that reads as distance rather than as width. At 1
        // the background is as the reverb made it, which is where every existing preset is.
        const float fw = smFarWidth_.next(farWidth_);
        float bgL = fl[i], bgR = fr[i];
        if (fw != 1.0f) {
            const float mid = 0.5f * (bgL + bgR), side = 0.5f * (bgL - bgR) * fw;
            bgL = mid + side; bgR = mid - side;
        }
        L[i] = nl[i] + bgL * far;
        R[i] = nr[i] + bgR * far;
        if (stems_ != nullptr) {
            // The Cosmos return was added into the near bus above, so it has to come out of the
            // near stem or it would be counted twice and the four would no longer sum to the mix.
            // What the Cosmos sent into the far plane cannot be separated here at all -- the
            // reverb has already mixed it with everything else -- and belongs to the far stem,
            // which is where it is heard.
            stems_[0][stemPos_ + i] = nl[i] - stems_[4][stemPos_ + i];
            stems_[1][stemPos_ + i] = nr[i] - stems_[5][stemPos_ + i];
            stems_[2][stemPos_ + i] = bgL * far;   // the far stem as it is heard: width included
            stems_[3][stemPos_ + i] = bgR * far;
        }
    }
    if (roomOn) {
        float* ol = roomOutL_.data(); float* orr = roomOutR_.data();
        room_.process(rl, rr, ol, orr, n);
        // Morph: the second impulse's answer to the same input, faded in. Only computed while
        // the morph is actually between the two rooms.
        if (roomMorph_ > 0.0005f || smRoomMorph_.value > 1.0e-4f) {
            float* bl = roomBL_.data(); float* br = roomBR_.data();
            roomB_.process(rl, rr, bl, br, n);
            for (int i = 0; i < n; ++i) {
                const float x = smRoomMorph_.next(roomMorph_);
                ol[i] += (bl[i] - ol[i]) * x;
                orr[i] += (br[i] - orr[i]) * x;
            }
        }
        const float lpc = 1.0f - std::exp(-kTwoPi * roomHighcut_ / static_cast<float>(sr_));
        const float rhpc = roomLowcut_ <= 21.0f ? 0.0f
                         : 1.0f - std::exp(-kTwoPi * (roomLowcut_ / 1.5538f) / static_cast<float>(sr_));
        const float levelC = 1.0f - std::exp(-1.0f / (0.05f * static_cast<float>(sr_)));
        // An energy-normalised impulse returns the far sends at their own power, which is far
        // louder than the FDN's output at Far Level 1; 0.35 puts Room Level 1 in the same league
        // (measured: -15.8 dBFS raw vs. -23 dBFS for the FDN on the default patch).
        const float roomGain = 0.35f;
        for (int i = 0; i < n; ++i) {
            roomLevelCur_ += (roomLevel_ - roomLevelCur_) * levelC;
            roomLpL_ += lpc * (ol[i] - roomLpL_);
            roomLpR_ += lpc * (orr[i] - roomLpR_);
            float tL = roomLpL_, tR = roomLpR_;
            if (rhpc > 0.0f) {   // the low end out of the room, 12 dB/oct
                roomHpL1_ += rhpc * (tL - roomHpL1_);  const float aL = tL - roomHpL1_;
                roomHpR1_ += rhpc * (tR - roomHpR1_);  const float aR = tR - roomHpR1_;
                roomHpL2_ += rhpc * (aL - roomHpL2_);  tL = aL - roomHpL2_;
                roomHpR2_ += rhpc * (aR - roomHpR2_);  tR = aR - roomHpR2_;
            }
            const float rL = tL * roomLevelCur_ * roomGain, rR = tR * roomLevelCur_ * roomGain;
            L[i] += rL;
            R[i] += rR;
            if (stems_ != nullptr) { stems_[6][stemPos_ + i] = rL; stems_[7][stemPos_ + i] = rR; }
        }
    } else {
        roomLpL_ = roomLpR_ = 0.0f;
        if (stems_ != nullptr) for (int i = 0; i < n; ++i) { stems_[6][stemPos_ + i] = 0.0f; stems_[7][stemPos_ + i] = 0.0f; }
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
        const double target = std::log(std::max(subHz * static_cast<double>(vp_.pitchMul), 10.0));   // the tide moves the sub with the voices
        if (subFreqCur_ <= 0.0) subFreqCur_ = target;
        const double glideC = 1.0 - std::exp(-1.0 / (std::max(subGlide_, 0.05f) * sr_));
        const float levelC = 1.0f - std::exp(-1.0f / (2.0f * static_cast<float>(sr_)));
        // The glide is a one-pole in the log domain, so its end point over this block is known in
        // closed form: take exp twice and walk the frequency linearly, instead of an exp per sample.
        const double reach = 1.0 - std::pow(1.0 - glideC, static_cast<double>(n));
        const double fBegin = std::exp(subFreqCur_);
        const double freqEnd = subFreqCur_ + (target - subFreqCur_) * reach;
        const double fEnd = std::exp(freqEnd);
        const double fStep = n > 0 ? (fEnd - fBegin) / static_cast<double>(n) : 0.0;
        double f = fBegin;
        subFreqCur_ = freqEnd;
        for (int i = 0; i < n; ++i) {
            subLevelCur_ += (subLevel_ - subLevelCur_) * levelC;
            f += fStep;
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
    // The body: the whole mix passes through a bank of modes and their answer is added back. It
    // sits before the mid/side stage, so its own width is treated like everything else's.
    if (bodyLevel_ > 0.0f || smBody_.value > 1.0e-4f) {
        float* mono = nl;   // the near bus is finished with; reuse it
        // The bank is linear, so the level rides on its input: one smoothed multiply per sample
        // and the body can be turned up mid-note without a step.
        for (int i = 0; i < n; ++i) mono[i] = 0.5f * (L[i] + R[i]) * smBody_.next(bodyLevel_);
        body_.process(mono, L, R, n, 1.0f);
    }
    midSide_.process(L, R, n);
    if (subOn)
        for (int i = 0; i < n; ++i) { L[i] += subL[i]; R[i] += subR[i]; }
    // The master's age, before the gain and the clipper: tape wow, lost highs, a noise floor.
    patina_.process(L, R, n);
    // Output DC blocker at 4 Hz, below the lowest sub the Foundation can reach. Several paths
    // can leave an offset behind -- FM at an integer ratio, the asymmetric tape term, a granular
    // window over a clip that carries one, the shimmer's pitch shifter -- and an offset costs
    // headroom in the soft clipper without ever being heard. One filter at the end covers them all.
    const float dcR = 1.0f - kTwoPi * 4.0f / static_cast<float>(sr_);
    // Subsonic: four cascaded one-pole high-passes, 24 dB/oct, on top of the 4 Hz DC blocker.
    // Off at zero, and zero is the default -- this instrument's Foundation reaches lower than the
    // frequency a mastering engineer cuts at, so the cut has to be the player's decision.
    // Four one-poles reach -3 dB at 2.299 times their own corner, so the pole goes there and the
    // knob keeps meaning the frequency the output is 3 dB down at.
    const float sub = subsonicHz_;
    const float subC = sub <= 0.5f ? 0.0f
                     : 1.0f - std::exp(-kTwoPi * (sub / 2.299f) / static_cast<float>(sr_));
    for (int i = 0; i < n; ++i) {
        const float g = masterSmooth_.next(master);
        float yl = L[i] - dcXL_ + dcR * dcYL_; dcXL_ = L[i]; dcYL_ = yl;
        float yr = R[i] - dcXR_ + dcR * dcYR_; dcXR_ = R[i]; dcYR_ = yr;
        if (subC > 0.0f) {
            subL1_ += subC * (yl - subL1_); yl -= subL1_;
            subR1_ += subC * (yr - subR1_); yr -= subR1_;
            subL2_ += subC * (yl - subL2_); yl -= subL2_;
            subR2_ += subC * (yr - subR2_); yr -= subR2_;
            subL3_ += subC * (yl - subL3_); yl -= subL3_;
            subR3_ += subC * (yr - subR3_); yr -= subR3_;
            subL4_ += subC * (yl - subL4_); yl -= subL4_;
            subR4_ += subC * (yr - subR4_); yr -= subR4_;
        }
        L[i] = softClip(yl * g);
        R[i] = softClip(yr * g);
        outTap_[(outTapW_ + i) & (kOutTapLen - 1)] = 0.5f * (L[i] + R[i]);
    }
    outTapW_ = (outTapW_ + n) & (kOutTapLen - 1);
    // The loudness meter sees exactly what a file would: after the master gain and the clipper.
    loudness_.process(L, R, n);
}

} // namespace ambient
