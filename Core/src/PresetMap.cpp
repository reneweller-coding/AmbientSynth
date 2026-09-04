#include "ambient/PresetMap.h"
#include "ambient/PresetMeta.h"
#include "ambient/Presets.h"
#include "ambient/Dsp.h"
#include <vector>
#include <cmath>
#include <mutex>

namespace ambient {

namespace {
std::vector<float> g_values;   // numPresets * kNumParams
bool g_ready = false;
std::once_flag g_once;
}

void PresetMap::warmup()
{
    std::call_once(g_once, [] {
        const int n = numPresets();
        g_values.assign(static_cast<size_t>(n) * kNumParams, 0.0f);
        for (int p = 0; p < n; ++p) {
            float* v = g_values.data() + static_cast<size_t>(p) * kNumParams;
            for (const ParamDesc& d : paramTable()) v[static_cast<int>(d.id)] = d.def;
            applyPreset(preset(p), [&](ParamId id, float val) { v[static_cast<int>(id)] = val; });
        }
        g_ready = true;
    });
}

bool PresetMap::ready() { return g_ready; }

const float* PresetMap::presetValues(int index)
{
    if (!g_ready || index < 0 || index >= numPresets()) return nullptr;
    return g_values.data() + static_cast<size_t>(index) * kNumParams;
}

PresetMap::Blend PresetMap::neighbours(float x, float y, float radius)
{
    Blend b;
    const int n = std::min(numPresetMeta(), numPresets());
    const float sigma = std::max(radius, 1e-3f);
    // Keep the kNeighbours closest points (insertion into a small sorted list).
    float dist[kNeighbours];
    for (int i = 0; i < n; ++i) {
        const PresetMeta& m = presetMeta(i);
        const float dx = m.x - x, dy = m.y - y;
        const float d = dx * dx + dy * dy;
        int pos = b.count;
        while (pos > 0 && dist[pos - 1] > d) --pos;
        if (pos >= kNeighbours) continue;
        const int last = std::min(b.count, kNeighbours - 1);
        for (int k = last; k > pos; --k) { dist[k] = dist[k - 1]; b.index[k] = b.index[k - 1]; }
        dist[pos] = d; b.index[pos] = i;
        if (b.count < kNeighbours) ++b.count;
    }
    float sum = 0.0f;
    for (int k = 0; k < b.count; ++k) { b.weight[k] = std::exp(-dist[k] / (2.0f * sigma * sigma)); sum += b.weight[k]; }
    if (sum <= 1e-12f) {   // far from everything: the nearest point alone
        for (int k = 0; k < b.count; ++k) b.weight[k] = (k == 0) ? 1.0f : 0.0f;
    } else {
        for (int k = 0; k < b.count; ++k) b.weight[k] /= sum;
    }
    return b;
}

void PresetMap::blend(const Blend& b, float* out)
{
    for (const ParamDesc& d : paramTable()) {
        const int i = static_cast<int>(d.id);
        if (!g_ready || b.count == 0) { out[i] = d.def; continue; }
        const float* strongest = presetValues(b.index[0]);
        switch (d.kind) {
        case ParamKind::Float: {
            const float span = std::max(d.max - d.min, 1e-9f);
            float p = 0.0f;
            for (int k = 0; k < b.count; ++k) {
                const float v = presetValues(b.index[k])[i];
                p += b.weight[k] * std::pow(clampv((v - d.min) / span, 0.0f, 1.0f), d.skew);
            }
            out[i] = d.min + span * std::pow(clampv(p, 0.0f, 1.0f), 1.0f / d.skew);
            break;
        }
        case ParamKind::Int: {
            float v = 0.0f;
            for (int k = 0; k < b.count; ++k) v += b.weight[k] * presetValues(b.index[k])[i];
            out[i] = static_cast<float>(std::lround(v));
            break;
        }
        default:
            out[i] = strongest[i];
        }
    }
}

} // namespace ambient
