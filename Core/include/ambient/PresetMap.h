// AmbientSynth -- the preset map: every built-in preset is a point in a plane (PresetMeta);
// a cursor anywhere in the plane blends the presets around it, weighted by a Gaussian of
// the distance (radius = its sigma). On a point the blend is that preset; between points
// it is a new sound that has never been saved. The engine runs this at control rate when
// Map is active (MapX / MapY / MapRadius), so hands, OSC, MIDI or automation can wander
// the map, and the browser draws it.
#pragma once
#include "Params.h"
#include <cstdint>

namespace ambient {

class PresetMap {
public:
    static constexpr int kNeighbours = 6;

    // Builds the cached parameter vectors of all presets (allocates; call before the audio
    // thread needs blend(); the engine does it in prepare()).
    static void warmup();
    static bool ready();

    struct Blend {
        int   index[kNeighbours];
        float weight[kNeighbours];   // normalised, descending
        int   count = 0;
    };
    // Neighbour weights for a cursor (0..1 plane). Never allocates.
    static Blend neighbours(float x, float y, float radius);
    // Blended parameter vector (kNumParams values): floats in the skew domain, ints
    // rounded, choices and switches from the strongest neighbour. Never allocates.
    static void blend(const Blend& b, float* out);
    // The cached vector of one preset (kNumParams values), or nullptr before warmup.
    static const float* presetValues(int index);
};

} // namespace ambient
