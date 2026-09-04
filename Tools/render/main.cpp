// ambient_render -- offline renderer for AmbientSynth (no JUCE, deterministic).
// Renders the engine to a 32-bit float WAV and prints measurements, so a patch
// can be judged by numbers instead of by ear.
//
//   ambient_render [--out file.wav] [--seconds 60] [--sr 48000] [--block 256]
//                  [--preset "name"] [--set key=value]... [--notes 45,52,59]
//                  [--scl file.scl] [--stats] [--list] [--list-presets]
//                  [--texture file.wav [baseHz]] [--wavetable file.wav]
#include "ambient/Engine.h"
#include "ambient/Params.h"
#include "ambient/Presets.h"
#include "ambient/WavFile.h"
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

int main(int argc, char** argv)
{
    std::string out = "ambient.wav";
    double seconds = 60.0;
    int sr = 48000, block = 256;
    bool stats = false;
    std::vector<int> notes;
    std::string sclPath;
    Engine engine;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return (i + 1 < argc) ? argv[++i] : ""; };
        if (a == "--out") out = next();
        else if (a == "--seconds") seconds = std::atof(next().c_str());
        else if (a == "--sr") sr = std::atoi(next().c_str());
        else if (a == "--block") block = std::atoi(next().c_str());
        else if (a == "--stats") stats = true;
        else if (a == "--scl") sclPath = next();
        else if (a == "--texture") {
            const std::string path = next();
            double baseHz = 261.6256;
            if (i + 1 < argc && std::atof(argv[i + 1]) > 0.0) baseHz = std::atof(argv[++i]);
            std::vector<float> mono; int rate = 0;
            if (!readWavMono(path.c_str(), mono, rate)) { std::fprintf(stderr, "cannot read texture %s\n", path.c_str()); return 2; }
            engine.setTexture(mono.data(), static_cast<int>(mono.size()), rate, baseHz);
            std::printf("texture: %s (%.1f s @ %d Hz, base %.1f Hz)\n", path.c_str(), mono.size() / static_cast<double>(rate), rate, baseHz);
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
        else if (a == "--list-presets") {
            for (int p = 0; p < numPresets(); ++p) std::printf("%s\n", preset(p).name);
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

    engine.prepare(sr, block);
    for (int n : notes) engine.noteOn(n, 0.8f);

    const long total = static_cast<long>(seconds * sr);
    std::vector<float> L(static_cast<size_t>(block)), R(static_cast<size_t>(block));
    std::vector<float> wav; wav.reserve(static_cast<size_t>(total) * 2);

    double sumSq[2] = { 0, 0 }; float peak = 0.0f; long nans = 0;
    double secSq[2] = { 0, 0 }; long secCount = 0; int sec = 0;
    if (stats) std::printf("sec, rmsL_dB, rmsR_dB, peak, voices, root, arc\n");
    float secPeak = 0.0f;

    for (long done = 0; done < total; done += block) {
        const int n = static_cast<int>(std::min<long>(block, total - done));
        engine.process(L.data(), R.data(), n);
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
    if (!writeWav(out, wav, 2, sr)) { std::fprintf(stderr, "cannot write %s\n", out.c_str()); return 1; }
    std::printf("wrote %s\n", out.c_str());
    return nans == 0 ? 0 : 1;
}
