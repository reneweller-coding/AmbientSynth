// AmbientSynth -- built-in presets. A preset is a name plus "key=value;key=value"
// over the parameter table; unspecified parameters take their defaults.
//
// Two independent layers can be loaded and combined:
//   Sound  = everything except the Cosmos section (voices, space, delays, reverbs, brain, tuning)
//   Cosmos = the Cosmos section only
// The 128 full presets carry both layers (they are the DAW programs); the Cosmos
// bank carries Cosmos-only settings.
#pragma once
#include "Params.h"
#include <cstring>

namespace ambient {

struct Preset {
    const char* name;
    const char* settings;   // "key=value;key=value", choice values may be given by name
};

enum class PresetScope { Full, Sound, Cosmos };

int numPresets();                         // full presets (128)
const Preset& preset(int index);
int numCosmosPresets();                   // Cosmos-only bank
const Preset& cosmosPreset(int index);

inline bool isCosmosParam(ParamId id) { return std::strcmp(paramDesc(id).section, "Cosmos") == 0; }
inline bool inScope(ParamId id, PresetScope scope)
{
    return scope == PresetScope::Full || (scope == PresetScope::Cosmos) == isCosmosParam(id);
}

// Parse a value for `d` from text: numbers, "on"/"off", or a choice name.
float paramValueFromText(const ParamDesc& d, const char* text);

// Calls set(ParamId, value) for every parameter in scope: defaults first, then the
// preset's settings. Returns false if the settings string names an unknown parameter.
template <class SetFn>
bool applyPreset(const Preset& p, SetFn&& set, PresetScope scope = PresetScope::Full)
{
    for (const ParamDesc& d : paramTable()) if (inScope(d.id, scope)) set(d.id, d.def);
    bool ok = true;
    const char* s = p.settings;
    while (s && *s) {
        const char* eq = s;
        while (*eq && *eq != '=' && *eq != ';') ++eq;
        if (*eq != '=') { ok = false; break; }
        char key[64];
        const int klen = static_cast<int>(eq - s);
        if (klen <= 0 || klen >= 64) { ok = false; break; }
        for (int i = 0; i < klen; ++i) key[i] = s[i];
        key[klen] = 0;
        const char* vs = eq + 1;
        const char* ve = vs;
        while (*ve && *ve != ';') ++ve;
        char val[64];
        const int vlen = static_cast<int>(ve - vs);
        if (vlen >= 64) { ok = false; break; }
        for (int i = 0; i < vlen; ++i) val[i] = vs[i];
        val[vlen] = 0;
        if (const ParamDesc* d = findParam(key)) { if (inScope(d->id, scope)) set(d->id, paramValueFromText(*d, val)); }
        else ok = false;
        s = (*ve == ';') ? ve + 1 : ve;
    }
    return ok;
}

} // namespace ambient
