// AmbientSynth -- built-in presets. A preset is a name plus "key=value;key=value"
// over the parameter table; unspecified parameters take their defaults.
//
// Two independent layers can be loaded and combined:
//   Sound  = everything except the Cosmos section (voices, space, delays, reverbs, brain, tuning)
//   Cosmos = the Cosmos section only
// The 168 full presets carry both layers (they are the DAW programs); the Cosmos
// bank carries Cosmos-only settings.
#pragma once
#include "Params.h"
#include <cstring>

namespace ambient {

struct Preset {
    const char* name;
    const char* settings;             // "key=value;key=value", choice values may be given by name
    const char* texture = nullptr;    // file the host should load into the Texture slots (pack presets)
    const char* wavetable = nullptr;  // file the host should load into the User wavetable
    const char* impulse = nullptr;    // file the host should load into the convolution Room
    // Modulation is data, not parameters (see Modulation.h): the matrix rows, and the six
    // envelope shapes separated by '~'. Empty means "the defaults".
    const char* mod = nullptr;
    const char* envs = nullptr;
};

enum class PresetScope { Full, Sound, Cosmos };

// The preset list is the 168 built-in presets followed by every loaded pack, so everything that
// walks presets by index (DAW programs, the map, routes, the browser) sees packs automatically.
int numPresets();
const Preset& preset(int index);
int builtinPresetCount();                 // the compiled-in presets (the first ones)
const Preset& builtinPreset(int index);
int numCosmosPresets();                   // Cosmos-only bank
const Preset& cosmosPreset(int index);

inline bool isCosmosParam(ParamId id) { return sectionOf(id) == ParamSection::Cosmos; }
inline bool isMorphParam(ParamId id)  { return sectionOf(id) == ParamSection::Morph; }
inline bool isMacroParam(ParamId id)  { return sectionOf(id) == ParamSection::Macros; }
inline bool isMapParam(ParamId id)    { return sectionOf(id) == ParamSection::Map; }
inline bool isRouteParam(ParamId id)  { return sectionOf(id) == ParamSection::Route; }
inline bool isClockParam(ParamId id)  { return sectionOf(id) == ParamSection::Clock; }
// Morph controls, macros, the map cursor, the route and the clock are performance state, never part of any preset.
inline bool isPerformanceParam(ParamId id) { return isMorphParam(id) || isMacroParam(id) || isMapParam(id) || isRouteParam(id) || isClockParam(id); }
inline bool inScope(ParamId id, PresetScope scope)
{
    if (isPerformanceParam(id)) return false;
    return scope == PresetScope::Full || (scope == PresetScope::Cosmos) == isCosmosParam(id);
}

// ---------------------------------------------------------------- preset packs
//
// A pack is a UTF-8 text file (.ambientpack) of presets, loaded at runtime instead of compiled
// in, so a library of thousands does not live in the binary:
//
//   # comment
//   pack <pack name>
//   <name>|<settings>|<x y bright motion width noisy bass density tagbits>|<texture>|<wavetable>|
//   <impulse>|<mod matrix>|<env shapes, '~' between them>
//
// Everything after the settings is optional. The metadata field feeds the browser and the map
// (see PresetMeta.h); the file fields name a sample and a wavetable relative to the pack file,
// which the host loads when the preset is applied. Message thread only.
bool loadPresetPack(const char* path);      // false if the file is missing or a line is malformed
int  loadPresetPacksIn(const char* dir);    // every *.ambientpack in a directory; returns how many loaded
// $AMBIENT_PACKS (';'-separated) if it finds anything, else the user's own
// Documents/AmbientSynth/Packs and the folders an installer writes to (Windows: ProgramData and
// LocalAppData; elsewhere /usr/local/share and /usr/share). A pack found twice loads once.
int  loadDefaultPresetPacks();
void clearPresetPacks();
int  numPresetPacks();
const char* presetPackName(int pack);
// Absolute path of the file a pack preset names, or an empty string.
// `which`: 0 texture, 1 wavetable, 2 impulse.
constexpr int kPresetFiles = 3;
const char* presetFilePath(int presetIndex, int which);

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
