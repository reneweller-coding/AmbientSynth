// ambient_render -- offline renderer for AmbientSynth (no JUCE, deterministic).
// Renders the engine to a 32-bit float WAV and prints measurements, so a patch
// can be judged by numbers instead of by ear.
//
//   ambient_render [--out file.wav] [--seconds 60] [--sr 48000] [--block 256]
//                  [--preset "name"] [--set key=value]... [--notes 45,52,59]
//                  [--scl file.scl] [--stats] [--list] [--list-presets]
//                  [--texture file.wav [baseHz]] [--wavetable file.wav]
//                  [--mod "lfo1>cutoff:0.4;..."] [--env 1 "0:0/2:1/8:0"]
//                  [--map x y [radius]] (render the preset-map blend at a cursor) [--dump] (print all parameters)
//                  [--ir impulse.wav] (convolution room impulse, mono or stereo)
//                  [--route "name or text" [speed]] (walk a route over the map; --list-routes)
//                  [--set-file set.ambientset] (replay a recorded set; length = set + 20 s unless --seconds)
#include "ambient/Engine.h"
#include "ambient/Params.h"
#include "ambient/Presets.h"
#include "ambient/WavFile.h"
#include "ambient/Sources.h"
#include "ambient/PresetMap.h"
#include "ambient/Timeline.h"
#include "ambient/Score.h"
#include "ambient/ClusterBrain.h"   // peakRoughness: one Plomp-Levelt curve for the conductor and the map
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>

using namespace ambient;

namespace {

bool writeWav(const std::string& path, const std::vector<float>& interleaved, int channels, int sampleRate)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    auto u32 = [&](uint32_t v) { f.write(reinterpret_cast<const char*>(&v), 4); };
    auto u16 = [&](uint16_t v) { f.write(reinterpret_cast<const char*>(&v), 2); };
    const uint32_t dataBytes = static_cast<uint32_t>(interleaved.size() * sizeof(float));
    f.write("RIFF", 4); u32(36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(3); u16(static_cast<uint16_t>(channels));
    u32(static_cast<uint32_t>(sampleRate)); u32(static_cast<uint32_t>(sampleRate * channels * 4));
    u16(static_cast<uint16_t>(channels * 4)); u16(32);
    f.write("data", 4); u32(dataBytes);
    f.write(reinterpret_cast<const char*>(interleaved.data()), dataBytes);
    return static_cast<bool>(f);
}

} // namespace

// ---------------------------------------------------------------- --measure
//
// Renders and prints the descriptors instead of writing a file. The measurement pass over the
// library used to render each preset to a temporary WAV and read it straight back: five thousand
// presets times twelve seconds of stereo float is twenty-three gigabytes written and read again
// for nothing, and all of it stayed in the file cache afterwards.
namespace {

// In-place radix-2 FFT on interleaved real/imag, n a power of two.
void fft(std::vector<float>& re, std::vector<float>& im)
{
    const int n = static_cast<int>(re.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * 3.14159265358979323846 / len;
        const float wr = static_cast<float>(std::cos(ang)), wi = static_cast<float>(std::sin(ang));
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                const int a = i + k, b = i + k + len / 2;
                const float xr = re[b] * cr - im[b] * ci;
                const float xi = re[b] * ci + im[b] * cr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr;        im[a] += xi;
                const float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}

// Centroid, flatness, flux, low-band share, stereo width and level of the settled half -- and,
// for the preset map, three descriptors that say what a drone is like rather than what a note is
// like: how much it changes over a minute, how rough its spectrum is, and how wet it stands.
// stemE holds the energy of the four buses (near, far, cosmos, room) or is null.
void printMeasurements(const std::vector<float>& L, const std::vector<float>& R, int sr, int voices,
                       const double* stemE = nullptr)
{
    const size_t n = L.size();
    if (n < 4096) { std::printf("measure: rms=-120 centroid=0 flatness=0 flux=0 bass=0 width=0 voices=%d peak=0 jump=0 dc=0 monoloss=0\n", voices); return; }
    const size_t half = n / 2;
    const int win = 2048, hop = 1024;
    std::vector<float> re(win), im(win), mag(win / 2 + 1), prev(win / 2 + 1, 0.0f), hann(win);
    for (int i = 0; i < win; ++i) hann[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(6.28318530718f * i / (win - 1));
    double centroid = 0.0, flatness = 0.0, flux = 0.0, bass = 0.0;
    int frames = 0;
    std::vector<double> avgMag(static_cast<size_t>(win / 2 + 1), 0.0);   // for the roughness
    for (size_t start = half; start + static_cast<size_t>(win) < n; start += static_cast<size_t>(hop)) {
        for (int i = 0; i < win; ++i) {
            re[static_cast<size_t>(i)] = 0.5f * (L[start + static_cast<size_t>(i)] + R[start + static_cast<size_t>(i)]) * hann[static_cast<size_t>(i)];
            im[static_cast<size_t>(i)] = 0.0f;
        }
        fft(re, im);
        double sum = 0.0, wsum = 0.0, logsum = 0.0, low = 0.0, d = 0.0;
        for (int k = 0; k <= win / 2; ++k) {
            const float m = std::sqrt(re[static_cast<size_t>(k)] * re[static_cast<size_t>(k)] + im[static_cast<size_t>(k)] * im[static_cast<size_t>(k)]) + 1e-12f;
            mag[static_cast<size_t>(k)] = m;
            const double f = static_cast<double>(k) * sr / win;
            const double p = static_cast<double>(m) * m;
            sum += p; wsum += p * f; logsum += std::log(static_cast<double>(m));
            if (f < 150.0) low += p;
        }
        if (frames > 0) {
            double na = 0.0, nb = 0.0;
            for (int k = 0; k <= win / 2; ++k) { na += mag[static_cast<size_t>(k)]; nb += prev[static_cast<size_t>(k)]; }
            for (int k = 0; k <= win / 2; ++k)
                d += std::fabs(mag[static_cast<size_t>(k)] / std::max(na, 1e-12) - prev[static_cast<size_t>(k)] / std::max(nb, 1e-12));
            flux += d;
        }
        prev = mag;
        for (int k = 0; k <= win / 2; ++k) avgMag[static_cast<size_t>(k)] += mag[static_cast<size_t>(k)];
        centroid += wsum / std::max(sum, 1e-20);
        flatness += std::exp(logsum / (win / 2 + 1)) / std::max(sum > 0.0 ? std::sqrt(sum / (win / 2 + 1)) : 1e-12, 1e-12);
        bass += low / std::max(sum, 1e-20);
        ++frames;
    }
    const double inv = 1.0 / std::max(frames, 1);
    double sq = 0.0, ml = 0.0, mr = 0.0, vl = 0.0, vr = 0.0, cov = 0.0;
    for (size_t i = half; i < n; ++i) { sq += L[i] * L[i] + R[i] * R[i]; ml += L[i]; mr += R[i]; }
    const double cnt = static_cast<double>(n - half);
    ml /= cnt; mr /= cnt;
    for (size_t i = half; i < n; ++i) { const double a = L[i] - ml, b = R[i] - mr; vl += a * a; vr += b * b; cov += a * b; }
    const double corr = (vl > 1e-18 && vr > 1e-18) ? cov / std::sqrt(vl * vr) : 1.0;
    const double rms = std::sqrt(sq / (2.0 * cnt));
    // The sound test's numbers as well: peak, the largest sample-to-sample step (a click), the
    // offset of the settled half, and how much level the mono sum loses against the stereo signal.
    double peak = 0.0, jump = 0.0, monoSq = 0.0, stSq = 0.0;
    float pl = 0.0f, pr = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        const float l = L[i], r = R[i];
        peak = std::max(peak, static_cast<double>(std::max(std::fabs(l), std::fabs(r))));
        const float m = 0.5f * (l + r), pm = 0.5f * (pl + pr);
        if (i > 0) jump = std::max(jump, static_cast<double>(std::fabs(m - pm)));
        pl = l; pr = r;
        if (i >= half) { monoSq += m * m; stSq += 0.5 * (l * l + r * r); }
    }
    const double dc = std::fabs(0.5 * (ml + mr));
    const double monoLoss = 20.0 * std::log10((std::sqrt(stSq / cnt) + 1e-12) / (std::sqrt(monoSq / cnt) + 1e-12));
    // A fingerprint of the audio itself. The descriptors are averages: two renders can agree on
    // every one of them and still not be the same sound. The samples are quantised to about
    // -120 dB first, so this catches a change in what is played and not the last bit of a sum
    // (the vectorised partial bank moves those, and moving them is not a change).
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) {
        const int32_t q[2] = { static_cast<int32_t>(std::lround(L[i] * 1048576.0f)),
                               static_cast<int32_t>(std::lround(R[i] * 1048576.0f)) };
        for (int k = 0; k < 2; ++k)
            for (int b = 0; b < 4; ++b) { h ^= static_cast<uint64_t>((q[k] >> (b * 8)) & 0xff); h *= 1099511628211ull; }
    }
    // ---- what a drone is like -------------------------------------------------------------
    // Evolution: how far the sound travels over the whole render, not from one block to the next.
    // A second at a time, the level in decibels and the centroid in octaves; the spread of those
    // two series is what separates a preset that stands still from one that goes somewhere.
    // Flux cannot say this -- a fast tremolo has flux and goes nowhere.
    double evoTone = 0.0, evoLevel = 0.0;
    {
        const size_t sec = static_cast<size_t>(sr);
        const int fftN = 4096;
        std::vector<double> tone, level;
        std::vector<float> fr(static_cast<size_t>(fftN)), fi(static_cast<size_t>(fftN));
        for (size_t s = 0; s + sec <= n; s += sec) {
            double e = 0.0;
            for (size_t i = s; i < s + sec; ++i) e += static_cast<double>(L[i]) * L[i] + static_cast<double>(R[i]) * R[i];
            level.push_back(10.0 * std::log10(e / (2.0 * static_cast<double>(sec)) + 1e-20));
            if (s + static_cast<size_t>(fftN) > n) continue;
            for (int i = 0; i < fftN; ++i) {
                const double w = 0.5 - 0.5 * std::cos(6.28318530718 * i / (fftN - 1));
                fr[static_cast<size_t>(i)] = static_cast<float>(0.5 * (L[s + static_cast<size_t>(i)] + R[s + static_cast<size_t>(i)]) * w);
                fi[static_cast<size_t>(i)] = 0.0f;
            }
            fft(fr, fi);
            double su = 0.0, ws = 0.0;
            for (int k = 1; k <= fftN / 2; ++k) {
                const double m = std::sqrt(static_cast<double>(fr[static_cast<size_t>(k)]) * fr[static_cast<size_t>(k)] + static_cast<double>(fi[static_cast<size_t>(k)]) * fi[static_cast<size_t>(k)]);
                const double p = m * m, f = static_cast<double>(k) * sr / fftN;
                su += p; ws += p * f;
            }
            tone.push_back(std::log2(std::max(ws / std::max(su, 1e-20), 20.0)));
        }
        auto spread = [](const std::vector<double>& v) {
            if (v.size() < 3) return 0.0;
            double m = 0.0;
            for (double x : v) m += x;
            m /= static_cast<double>(v.size());
            double s = 0.0;
            for (double x : v) s += (x - m) * (x - m);
            return std::sqrt(s / static_cast<double>(v.size()));
        };
        // The loudest second is dropped from the level series: a preset whose first note arrives
        // late otherwise reads as "evolving" when all it did was start.
        evoTone = spread(tone);
        evoLevel = spread(level);
    }
    // Roughness: the Plomp-Levelt curve over the peaks of the average spectrum, the same one the
    // conductor judges its chords with (ambient::peakRoughness). Smooth and fused against
    // beating and grinding -- the axis a drone lives on.
    double rough = 0.0;
    {
        double pf[32] = {}, pa[32] = {};
        int np = 0;
        double loudest = 0.0;
        for (int k = 1; k < win / 2; ++k) loudest = std::max(loudest, avgMag[static_cast<size_t>(k)]);
        for (int k = 2; k < win / 2 - 1 && np < 32; ++k) {
            const double m = avgMag[static_cast<size_t>(k)];
            if (m < loudest * 0.02) continue;
            if (m <= avgMag[static_cast<size_t>(k - 1)] || m < avgMag[static_cast<size_t>(k + 1)]) continue;
            pf[np] = static_cast<double>(k) * sr / win;
            pa[np] = m;
            ++np;
        }
        rough = ambient::peakRoughness(pf, pa, np);
    }
    // ---- the fingerprint --------------------------------------------------------------------
    // Nine numbers say what a preset is LIKE -- dark, still, wide, far. They cannot say what it
    // IS: a goods yard and a beehive can agree on every one of them. This is the usual answer to
    // that in the literature, a mel-cepstral fingerprint: forty bands on a mel scale, the log of
    // their energy, a discrete cosine transform of that, and the first sixteen coefficients kept
    // with their spread over the render. Thirty-two numbers that carry the shape of the spectrum
    // rather than its averages, computed from the frames the descriptors already cost.
    constexpr int kMel = 40, kCeps = 16;
    double mfccMean[kCeps] = {}, mfccVar[kCeps] = {};
    {
        auto hzToMel = [](double f) { return 2595.0 * std::log10(1.0 + f / 700.0); };
        auto melToHz = [](double m) { return 700.0 * (std::pow(10.0, m / 2595.0) - 1.0); };
        const double melLo = hzToMel(30.0), melHi = hzToMel(std::min(16000.0, sr * 0.45));
        int centre[kMel + 2];
        for (int b = 0; b < kMel + 2; ++b) {
            const double hz = melToHz(melLo + (melHi - melLo) * b / (kMel + 1));
            centre[b] = std::max(1, std::min(win / 2, static_cast<int>(hz * win / sr + 0.5)));
        }
        std::vector<double> ceps(static_cast<size_t>(kCeps), 0.0);
        std::vector<float> fre(static_cast<size_t>(win)), fim(static_cast<size_t>(win));
        int used = 0;
        for (size_t start = half; start + static_cast<size_t>(win) < n; start += static_cast<size_t>(hop) * 2) {
            for (int i = 0; i < win; ++i) {
                fre[static_cast<size_t>(i)] = 0.5f * (L[start + static_cast<size_t>(i)] + R[start + static_cast<size_t>(i)]) * hann[static_cast<size_t>(i)];
                fim[static_cast<size_t>(i)] = 0.0f;
            }
            fft(fre, fim);
            double band[kMel];
            for (int b = 0; b < kMel; ++b) {
                double e = 0.0;
                for (int k = centre[b]; k <= centre[b + 2]; ++k) {
                    const double m = std::sqrt(static_cast<double>(fre[static_cast<size_t>(k)]) * fre[static_cast<size_t>(k)]
                                             + static_cast<double>(fim[static_cast<size_t>(k)]) * fim[static_cast<size_t>(k)]);
                    const double w = k <= centre[b + 1] ? (k - centre[b] + 1.0) / (centre[b + 1] - centre[b] + 1.0)
                                                        : (centre[b + 2] - k + 1.0) / (centre[b + 2] - centre[b + 1] + 1.0);
                    e += m * m * w;
                }
                band[b] = std::log(e + 1e-12);
            }
            for (int c = 0; c < kCeps; ++c) {
                double s2 = 0.0;
                for (int b = 0; b < kMel; ++b)
                    s2 += band[b] * std::cos(3.14159265358979323846 * (c + 1) * (b + 0.5) / kMel);
                ceps[static_cast<size_t>(c)] = s2 / kMel;
                mfccMean[c] += ceps[static_cast<size_t>(c)];
                mfccVar[c] += ceps[static_cast<size_t>(c)] * ceps[static_cast<size_t>(c)];
            }
            ++used;
        }
        const double inv2 = 1.0 / std::max(used, 1);
        for (int c = 0; c < kCeps; ++c) {
            mfccMean[c] *= inv2;
            mfccVar[c] = std::sqrt(std::max(0.0, mfccVar[c] * inv2 - mfccMean[c] * mfccMean[c]));
        }
    }
    // Wet: how much of what is heard comes back from the far reverb, the room and the Cosmos
    // rather than standing in the near plane. The spatial model's own axis, near to far.
    double wet = 0.0;
    if (stemE != nullptr) {
        const double all = stemE[0] + stemE[1] + stemE[2] + stemE[3];
        if (all > 1e-18) wet = (stemE[1] + stemE[2] + stemE[3]) / all;
    }
    std::printf("measure: rms=%.3f centroid=%.1f flatness=%.6f flux=%.6f bass=%.6f width=%.6f voices=%d "
                "peak=%.4f jump=%.4f dc=%.5f monoloss=%.3f evo_tone=%.5f evo_level=%.4f rough=%.6f wet=%.5f hash=%016llx\n",
                20.0 * std::log10(rms + 1e-12), centroid * inv, flatness * inv,
                frames > 1 ? flux / (frames - 1) : 0.0, bass * inv, 1.0 - std::fabs(corr), voices,
                peak, jump, dc, monoLoss, evoTone, evoLevel, rough, wet, static_cast<unsigned long long>(h));
    // The fingerprint on its own line, so nothing that parses the measure line has to learn a
    // new field: 16 cepstral means, then their 16 spreads.
    std::printf("timbre:");
    for (int c = 0; c < kCeps; ++c) std::printf(" %.4f", mfccMean[c]);
    for (int c = 0; c < kCeps; ++c) std::printf(" %.4f", mfccVar[c]);
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv)
{
    std::string out = "ambient.wav";
    double seconds = 60.0;
    int sr = 48000, block = 256;
    bool stats = false, dump = false, useMap = false, measure = false, loudness = false;
    std::string tapPath;
    double mapX = 0.5, mapY = 0.5, mapRadius = 0.08;
    std::vector<std::vector<float>> irChannels; int irRate = 0; std::string irPath;
    std::string routeText; double routeSpeed = 1.0;
    SetTimeline setFile; bool haveSet = false; bool secondsGiven = false;
    Score score; bool haveScore = false;
    std::string stemPrefix;
    int presetIndex = -1;
    loadDefaultPresetPacks();   // $AMBIENT_PACKS or ~/Documents/AmbientSynth/Packs; --packs adds more
    std::vector<int> notes;
    std::string sclPath;
    Engine engine;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--out") out = next();
        else if (a == "--seconds") { seconds = std::atof(next().c_str()); secondsGiven = true; }
        else if (a == "--sr") sr = std::atoi(next().c_str());
        else if (a == "--block") block = std::atoi(next().c_str());
        else if (a == "--stats") stats = true;
        else if (a == "--measure") measure = true;   // print descriptors instead of writing a file
        // A short mono excerpt of the settled part, for whatever wants to listen to the render
        // rather than read its numbers -- a learned audio embedding, say. 22.05 kHz is plenty for
        // that and keeps a whole library's worth of excerpts to a few gigabytes.
        else if (a == "--tap") tapPath = next();
        else if (a == "--loudness") loudness = true; // and a second line to BS.1770 (its own line so
                                                     // nothing that parses the measure line has to change)
        else if (a == "--dump") dump = true;
        else if (a == "--map") {   // render at a map position: x y [radius]
            mapX = std::atof(next().c_str()); mapY = std::atof(next().c_str());
            if (i + 1 < argc && std::atof(argv[i + 1]) > 0.0) mapRadius = std::atof(argv[++i]);
            useMap = true;
        }
        else if (a == "--scl") sclPath = next();
        else if (a == "--texture") {
            const std::string path = next();
            double baseHz = baseHzFromName(path.c_str());   // "_A3" suffix (TextureGen), else C4
            if (baseHz <= 0.0) baseHz = 261.6256;
            if (i + 1 < argc && std::atof(argv[i + 1]) > 0.0) baseHz = std::atof(argv[++i]);
            std::vector<float> mono; int rate = 0;
            if (!readWavMono(path.c_str(), mono, rate)) { std::fprintf(stderr, "cannot read texture %s\n", path.c_str()); return 2; }
            engine.setTexture(mono.data(), static_cast<int>(mono.size()), rate, baseHz, loopFromName(path.c_str()));
            std::printf("texture: %s (%.1f s @ %d Hz, base %.1f Hz%s)\n", path.c_str(), mono.size() / static_cast<double>(rate), rate, baseHz,
                        loopFromName(path.c_str()) ? ", seamless" : "");
        }
        else if (a == "--route") {   // walk a route preset (by name) or a route text over the map
            const std::string spec = next();
            int found = -1;
            for (int r = 0; r < numRoutePresets(); ++r) if (spec == routePreset(r).name) found = r;
            routeText = found >= 0 ? routePreset(found).points : spec;
            if (i + 1 < argc && std::atof(argv[i + 1]) > 0.0) routeSpeed = std::atof(argv[++i]);
            std::printf("route: %s (speed %g)\n", found >= 0 ? routePreset(found).name : "custom", routeSpeed);
        }
        else if (a == "--packs") { const std::string dir = next(); std::printf("packs: %d loaded from %s\n", loadPresetPacksIn(dir.c_str()), dir.c_str()); }
        else if (a == "--list-packs") {
            loadDefaultPresetPacks();
            for (int k = 0; k < numPresetPacks(); ++k) std::printf("%s\n", presetPackName(k));
            std::printf("%d presets total (%d built in)\n", numPresets(), builtinPresetCount());
            return 0;
        }
        else if (a == "--mod") {   // modulation matrix, see Core/include/ambient/Modulation.h
            const std::string text = next();
            if (!engine.setModMatrixText(text.c_str())) { std::fprintf(stderr, "bad matrix text\n"); return 2; }
            std::printf("matrix: %d route(s)\n", engine.modMatrix().count());
        }
        else if (a == "--env") {   // --env <1..6> "<breakpoints>"
            const int idx = std::atoi(next().c_str()) - 1;
            const std::string text = next();
            if (!engine.setEnvShape(idx, text.c_str())) { std::fprintf(stderr, "bad envelope %d\n", idx + 1); return 2; }
            std::printf("env %d: %d points\n", idx + 1, engine.envShape(idx).count());
        }
        else if (a == "--score") {   // a written score of timed ramps; length defaults to the score's
            const std::string path = next();
            if (!score.load(path.c_str())) { std::fprintf(stderr, "cannot read the score %s\n", path.c_str()); return 1; }
            haveScore = true;
            std::printf("score: %d events, %.0f s\n", score.count(), score.length());
        }
        else if (a == "--stems") stemPrefix = next();   // also write near/far/cosmos/room
        else if (a == "--set-file") {   // play a recorded set (.ambientset) while rendering; length defaults to the set's
            const std::string path = next();
            if (!setFile.load(path.c_str())) { std::fprintf(stderr, "cannot read set %s\n", path.c_str()); return 2; }
            haveSet = true;
            std::printf("set: %s (%zu events, %.1f s)\n", path.c_str(), setFile.size(), setFile.length());
        }
        else if (a == "--ir") {   // impulse response for the Room (mono or stereo WAV)
            const std::string path = next();
            std::vector<std::vector<float>> ch; int rate = 0;
            if (!readWavChannels(path.c_str(), ch, rate) || ch.empty()) { std::fprintf(stderr, "cannot read impulse %s\n", path.c_str()); return 2; }
            irChannels = ch; irRate = rate; irPath = path;
        }
        else if (a == "--wavetable") {
            const std::string path = next();
            std::vector<float> mono; int rate = 0;
            if (!readWavMono(path.c_str(), mono, rate) || !engine.loadUserWavetable(mono.data(), static_cast<int>(mono.size())))
            { std::fprintf(stderr, "cannot read wavetable %s (needs 2048-sample frames)\n", path.c_str()); return 2; }
            std::printf("wavetable: %s (%d frames)\n", path.c_str(), engine.userWavetableFrames());
        }
        else if (a == "--notes") {
            std::stringstream ss(next()); std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) notes.push_back(std::atoi(tok.c_str()));
        }
        else if (a == "--preset") {
            const std::string name = next();
            int found = -1;
            for (int p = 0; p < numPresets(); ++p) if (name == preset(p).name) found = p;
            if (found < 0) { std::fprintf(stderr, "unknown preset '%s' (see --list-presets)\n", name.c_str()); return 2; }
            engine.applyPreset(found);
            presetIndex = found;
            std::printf("preset: %s\n", preset(found).name);
        }
        else if (a == "--set") {
            std::string kv = next();
            const size_t eq = kv.find('=');
            if (eq == std::string::npos) { std::fprintf(stderr, "bad --set %s\n", kv.c_str()); return 2; }
            const ParamDesc* d = findParam(kv.substr(0, eq).c_str());
            if (!d) { std::fprintf(stderr, "unknown parameter '%s'\n", kv.substr(0, eq).c_str()); return 2; }
            engine.setParam(d->id, paramValueFromText(*d, kv.substr(eq + 1).c_str()));
        }
        else if (a == "--list-choices") {
            // Every choice parameter with its value names, so a tool never has to keep its own copy.
            for (const auto& d : paramTable()) {
                if (d.kind != ParamKind::Choice) continue;
                std::printf("%s:", d.key);
                for (int c = 0; c < d.numChoices; ++c) std::printf("%s%s", c ? "|" : "", d.choices[c]);
                std::printf("\n");
            }
            return 0;
        }
        else if (a == "--list") {
            for (const auto& d : paramTable())
                std::printf("%-18s %-14s [%g .. %g] default %g %s\n", d.key, d.section, d.min, d.max, d.def, d.unit);
            return 0;
        }
        else if (a == "--sound-preset" || a == "--cosmos-preset") {
            const bool cosmos = (a == "--cosmos-preset");
            const std::string name = next();
            const int count = cosmos ? numCosmosPresets() : numPresets();
            int found = -1;
            for (int p = 0; p < count; ++p) if (name == (cosmos ? cosmosPreset(p) : preset(p)).name) found = p;
            if (found < 0) { std::fprintf(stderr, "unknown %s preset '%s'\n", cosmos ? "cosmos" : "sound", name.c_str()); return 2; }
            if (cosmos) engine.applyCosmosPreset(found); else engine.applySoundPreset(found);
            std::printf("%s preset: %s\n", cosmos ? "cosmos" : "sound", name.c_str());
        }
        else if (a == "--describe") {   // the browser's line of prose for a preset, or for all of them
            const std::string want = next();
            for (int p = 0; p < numPresets(); ++p)
                if (want == "all" || want == preset(p).name)
                    std::printf("%-30s %s\n", preset(p).name, presetDescription(p).c_str());
            return 0;
        }
        else if (a == "--list-presets") {
            for (int p = 0; p < numPresets(); ++p) std::printf("%s\n", preset(p).name);
            return 0;
        }
        else if (a == "--list-routes") {
            for (int r = 0; r < numRoutePresets(); ++r) std::printf("%-22s %s\n", routePreset(r).name, routePreset(r).points);
            return 0;
        }
        else if (a == "--list-cosmos-presets") {
            for (int p = 0; p < numCosmosPresets(); ++p) std::printf("%s\n", cosmosPreset(p).name);
            return 0;
        }
        else if (a == "--bench") {
            // Realtime factor per preset (rendered seconds per wall second). Run on the device via adb
            // to see what the core costs there; below ~3 the headset would be at its limit.
            const double secs = 10.0;
            double worst = 1e9; const char* worstName = "";
            std::printf("%-24s %8s %8s\n", "preset", "rms dB", "x rt");
            for (int p = 0; p < numPresets(); ++p) {
                Engine e;
                e.applyPreset(p);
                e.setParam(ParamId::BrainRate, 3.0f);
                e.prepare(sr, block);
                e.noteOn(48, 0.8f); e.noteOn(55, 0.8f); e.noteOn(64, 0.8f);
                std::vector<float> bl(static_cast<size_t>(block)), br(static_cast<size_t>(block));
                double sq = 0; long cnt = 0;
                const auto t0 = std::chrono::steady_clock::now();
                for (long done = 0; done < static_cast<long>(secs * sr); done += block) {
                    e.process(bl.data(), br.data(), block);
                    for (int k = 0; k < block; ++k) { sq += bl[static_cast<size_t>(k)] * bl[static_cast<size_t>(k)]; ++cnt; }
                }
                const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                const double rt = secs / std::max(wall, 1e-6);
                if (rt < worst) { worst = rt; worstName = preset(p).name; }
                std::printf("%-24s %8.1f %8.1f\n", preset(p).name, 10.0 * std::log10(sq / std::max<long>(cnt, 1) + 1e-20), rt);
            }
            std::printf("slowest: %s at %.1f x realtime\n", worstName, worst);
            return 0;
        }
        else { std::fprintf(stderr, "unknown option %s\n", a.c_str()); return 2; }
    }

    if (!sclPath.empty()) {
        std::ifstream f(sclPath);
        std::stringstream ss; ss << f.rdbuf();
        FixedScale s;
        if (!parseScala(ss.str().c_str(), s)) { std::fprintf(stderr, "cannot parse %s\n", sclPath.c_str()); return 2; }
        engine.setUserScale(s);
        engine.setParam(ParamId::Scale, static_cast<float>(kNumScaleChoices - 1));
        std::printf("scale: %s (%d degrees, period %.4f)\n", s.name, s.count, s.period);
    }

    if (useMap) {
        engine.setParam(ParamId::MapActive, 1.0f);
        engine.setParam(ParamId::MapX, static_cast<float>(mapX));
        engine.setParam(ParamId::MapY, static_cast<float>(mapY));
        engine.setParam(ParamId::MapRadius, static_cast<float>(mapRadius));
        engine.setParam(ParamId::MorphGlide, 0.1f);   // arrive at the blend right away
        PresetMap::warmup();
        const PresetMap::Blend b = PresetMap::neighbours(static_cast<float>(mapX), static_cast<float>(mapY), static_cast<float>(mapRadius));
        std::printf("map %.3f %.3f (radius %.3f):", mapX, mapY, mapRadius);
        for (int k = 0; k < b.count; ++k) std::printf(" %s %.2f;", preset(b.index[k]).name, b.weight[k]);
        std::printf("\n");
    }
    if (dump) {   // every parameter as key = value (choices by name), for tools that analyse presets
        for (const ParamDesc& d : paramTable()) {
            const float v = engine.getParam(d.id);
            if (d.kind == ParamKind::Choice) std::printf("param %s = %s\n", d.key, d.choices[clampv(static_cast<int>(std::lround(v)), 0, d.numChoices - 1)]);
            else if (d.kind == ParamKind::Bool) std::printf("param %s = %s\n", d.key, v >= 0.5f ? "on" : "off");
            else std::printf("param %s = %g\n", d.key, v);
        }
    }

    // A pack preset may name a sample, a wavetable and an impulse of its own.
    if (presetIndex >= 0) {
        const char* tex = presetFilePath(presetIndex, 0);
        const char* tab = presetFilePath(presetIndex, 1);
        std::vector<float> mono; int rate = 0;
        if (tex && *tex) {
            // One path goes into every slot; up to four separated by ';' go one per slot, an empty
            // one meaning that slot has none -- the same reading the plugin gives the field.
            const std::string all(tex);
            const bool perSlot = all.find(';') != std::string::npos;
            size_t start = 0;
            for (int slot = 0; slot < ambient::kSlots; ++slot) {
                const size_t semi = all.find(';', start);
                std::string one = all.substr(start, semi == std::string::npos ? std::string::npos : semi - start);
                while (!one.empty() && one.front() == ' ') one.erase(one.begin());
                while (!one.empty() && one.back() == ' ') one.pop_back();
                if (!one.empty()) {
                    if (readWavMono(one.c_str(), mono, rate)) {
                        double base = baseHzFromName(one.c_str()); if (base <= 0.0) base = 261.6256;
                        const bool seamless = loopFromName(one.c_str());
                        if (perSlot) engine.setTexture(slot, mono.data(), static_cast<int>(mono.size()), rate, base, seamless);
                        else         engine.setTexture(mono.data(), static_cast<int>(mono.size()), rate, base, seamless);
                        std::printf("preset texture%s: %s\n", perSlot ? (" " + std::to_string(slot + 1)).c_str() : "", one.c_str());
                    } else std::fprintf(stderr, "preset texture missing: %s\n", one.c_str());
                }
                if (semi == std::string::npos) break;
                start = semi + 1;
            }
        }
        if (tab && *tab) {
            if (readWavMono(tab, mono, rate) && engine.loadUserWavetable(mono.data(), static_cast<int>(mono.size())))
                std::printf("preset wavetable: %s (%d frames)\n", tab, engine.userWavetableFrames());
            else std::fprintf(stderr, "preset wavetable missing or unusable: %s\n", tab);
        }
    }

    engine.prepare(sr, block);
    if (!irChannels.empty()) {   // after prepare: the convolver's buffers exist now
        engine.setImpulse(irChannels[0].data(), irChannels.size() > 1 ? irChannels[1].data() : nullptr, static_cast<int>(irChannels[0].size()), irRate);
        std::printf("impulse: %s (%zu ch, %.2f s @ %d Hz -> %.2f s used)\n", irPath.c_str(), irChannels.size(), irChannels[0].size() / static_cast<double>(irRate), irRate, engine.impulseSeconds());
    }
    for (int n : notes) engine.noteOn(n, 0.8f);
    if (!routeText.empty()) {
        if (!engine.setRouteText(routeText.c_str())) { std::fprintf(stderr, "bad route text (unknown preset name or malformed point)\n"); return 2; }
        engine.setParam(ParamId::RouteSpeed, static_cast<float>(routeSpeed));
        engine.setParam(ParamId::RouteLoop, 1.0f);
        engine.setParam(ParamId::RouteActive, 1.0f);
        const Waypoint& w0 = engine.routeEdit().point(0);
        engine.setParam(ParamId::MapX, w0.x); engine.setParam(ParamId::MapY, w0.y); engine.setParam(ParamId::MapRadius, w0.radius);   // start on the first point
        engine.setParam(ParamId::MorphGlide, 20.0f);
    }

    if (haveSet) { if (!secondsGiven) seconds = setFile.length() + 20.0; setFile.seek(0.0); }
    if (haveScore) { if (!secondsGiven) seconds = score.length() + 20.0; score.rewind(); }
    const long total = static_cast<long>(seconds * sr);
    std::vector<float> L(static_cast<size_t>(block)), R(static_cast<size_t>(block));
    std::vector<float> wav; wav.reserve(static_cast<size_t>(total) * 2);
    // Stems: four stereo pairs written alongside the mix, so a piece can be balanced afterwards.
    std::vector<std::vector<float>> stemOut(static_cast<size_t>(Engine::kNumStems) * 2);
    std::vector<float> stemChunk(static_cast<size_t>(Engine::kNumStems) * 2 * static_cast<size_t>(block), 0.0f);
    float* stemPtr[Engine::kNumStems * 2] = {};
    // The measurement wants them too, though it writes no files: the near / far / room / cosmos
    // energies are how wet a preset stands, which is the spatial model's own axis on the map.
    double stemE[Engine::kNumStems] = {};
    if (!stemPrefix.empty() || measure) {
        for (int c = 0; c < Engine::kNumStems * 2; ++c)
            stemPtr[c] = stemChunk.data() + static_cast<size_t>(c) * static_cast<size_t>(block);
        engine.setStemBuffers(stemPtr);
    }

    double sumSq[2] = { 0, 0 }; float peak = 0.0f; long nans = 0;
    double secSq[2] = { 0, 0 }; long secCount = 0; int sec = 0;
    if (stats) std::printf("sec, rmsL_dB, rmsR_dB, peak, voices, root, arc\n");
    float secPeak = 0.0f;

    for (long done = 0; done < total; done += block) {
        const int n = static_cast<int>(std::min<long>(block, total - done));
        if (!routeText.empty()) { float rx, ry, rr; engine.routeStep(static_cast<double>(n) / sr, rx, ry, rr); }
        if (haveSet) {
            const double t0 = static_cast<double>(done) / sr, t1 = static_cast<double>(done + n) / sr;
            setFile.step(t0, t1, [&](const TimelineEvent& e) {
                switch (e.type) {
                case TimelineEvent::Type::Param:   engine.setParam(static_cast<ParamId>(e.a), e.v); break;
                case TimelineEvent::Type::NoteOn:  engine.noteOn(e.a, e.v); break;
                case TimelineEvent::Type::NoteOff: engine.noteOff(e.a); break;
                }
            });
        }
        if (haveScore)
            score.step(static_cast<double>(n) / sr,
                       [&](ParamId id) { return engine.getParam(id); },
                       [&](ParamId id, float v) { engine.setParam(id, v); });
        engine.process(L.data(), R.data(), n);
        if (!stemPrefix.empty())
            for (int c = 0; c < Engine::kNumStems * 2; ++c)
                stemOut[static_cast<size_t>(c)].insert(stemOut[static_cast<size_t>(c)].end(), stemPtr[c], stemPtr[c] + n);
        if (measure && done >= total / 2)   // the settled half, as every other descriptor
            for (int st = 0; st < Engine::kNumStems; ++st)
                for (int i = 0; i < n; ++i)
                    stemE[st] += static_cast<double>(stemPtr[st * 2][i]) * stemPtr[st * 2][i]
                               + static_cast<double>(stemPtr[st * 2 + 1][i]) * stemPtr[st * 2 + 1][i];
        for (int i = 0; i < n; ++i) {
            const float l = L[static_cast<size_t>(i)], r = R[static_cast<size_t>(i)];
            if (std::isnan(l) || std::isnan(r) || std::isinf(l) || std::isinf(r)) ++nans;
            wav.push_back(l); wav.push_back(r);
            sumSq[0] += l * l; sumSq[1] += r * r;
            secSq[0] += l * l; secSq[1] += r * r;
            peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
            secPeak = std::max(secPeak, std::max(std::fabs(l), std::fabs(r)));
            if (++secCount >= sr) {
                if (stats) std::printf("%d, %.1f, %.1f, %.3f, %d, %d, %.2f\n", sec,
                    10.0 * std::log10(secSq[0] / secCount + 1e-20), 10.0 * std::log10(secSq[1] / secCount + 1e-20),
                    secPeak, engine.activeVoices(), engine.brainRoot(), engine.arcValue());
                secSq[0] = secSq[1] = 0; secCount = 0; secPeak = 0.0f; ++sec;
            }
        }
    }

    const double rmsL = std::sqrt(sumSq[0] / std::max<long>(total, 1)), rmsR = std::sqrt(sumSq[1] / std::max<long>(total, 1));
    std::printf("rendered %.1f s @ %d Hz: rms %.1f / %.1f dBFS, peak %.3f, non-finite %ld, voices at end %d\n",
                seconds, sr, 20.0 * std::log10(rmsL + 1e-20), 20.0 * std::log10(rmsR + 1e-20), peak, nans, engine.activeVoices());
    if (loudness) {
        // The engine's own meter, which has seen every sample of the render rather than a window
        // of it. Its own line, so nothing that parses the measure line has to learn a new field.
        const LoudnessReading ld = engine.loudness();
        // Sones as well as LUFS. The two disagree whenever the spectrum changes, which for an
        // instrument that makes beds rather than tracks is most of the time, and the sone figure
        // is the one that says whether a preset will feel loud after an hour of it.
        std::printf("loudness: lufs_i=%.2f lufs_s=%.2f lufs_m=%.2f lra=%.2f truepeak=%.2f crest=%.2f"
                    " sone=%.2f sone_n5=%.2f sone_max=%.2f seconds=%.1f\n",
                    ld.integrated, ld.shortTerm, ld.momentary, ld.range, ld.truePeak, ld.crest,
                    ld.sones, ld.sonesN5, ld.sonesMax, ld.seconds);
    }
    if (!tapPath.empty()) {
        // The last twelve seconds, mono, decimated to about 22 kHz by averaging -- a plain box
        // filter, which is enough for an embedding and costs nothing.
        const int step = std::max(1, sr / 22050);
        const long want = std::min<long>(total, static_cast<long>(12.0 * sr));
        const size_t frames = wav.size() / 2;
        const size_t from = frames > static_cast<size_t>(want) ? frames - static_cast<size_t>(want) : 0;
        std::vector<float> tap;
        tap.reserve((frames - from) / static_cast<size_t>(step) + 1);
        for (size_t i = from; i + static_cast<size_t>(step) <= frames; i += static_cast<size_t>(step)) {
            float acc = 0.0f;
            for (int k = 0; k < step; ++k) acc += 0.5f * (wav[(i + static_cast<size_t>(k)) * 2] + wav[(i + static_cast<size_t>(k)) * 2 + 1]);
            tap.push_back(acc / static_cast<float>(step));
        }
        if (!writeWav(tapPath, tap, 1, sr / step)) std::fprintf(stderr, "cannot write tap %s\n", tapPath.c_str());
    }
    if (measure) {   // descriptors straight from the buffer: no temporary file at all
        std::vector<float> ml(wav.size() / 2), mr(wav.size() / 2);
        for (size_t k = 0; k + 1 < wav.size(); k += 2) { ml[k / 2] = wav[k]; mr[k / 2] = wav[k + 1]; }
        printMeasurements(ml, mr, sr, engine.activeVoices(), stemE);
        return nans == 0 ? 0 : 1;
    }
    if (!writeWav(out, wav, 2, sr)) { std::fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
    if (!stemPrefix.empty()) {
        for (int st = 0; st < Engine::kNumStems; ++st) {
            std::vector<float> inter;
            inter.reserve(stemOut[static_cast<size_t>(st) * 2].size() * 2);
            for (size_t k = 0; k < stemOut[static_cast<size_t>(st) * 2].size(); ++k) {
                inter.push_back(stemOut[static_cast<size_t>(st) * 2][k]);
                inter.push_back(stemOut[static_cast<size_t>(st) * 2 + 1][k]);
            }
            const std::string path = stemPrefix + "_" + Engine::stemName(st) + ".wav";
            if (!writeWav(path, inter, 2, sr)) { std::fprintf(stderr, "cannot write a stem\n"); return 1; }
            std::printf("stem: %s\n", path.c_str());
        }
    }
    std::printf("wrote %s\n", out.c_str());
    return nans == 0 ? 0 : 1;
}
