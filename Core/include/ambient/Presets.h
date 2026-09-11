// AmbientSynth -- built-in presets. A preset is a name plus "key=value;key=value"
// over the parameter table; unspecified parameters take their defaults.
//
// Two independent layers can be loaded and combined:
//   Sound  = everything except the Cosmos section (voices, space, delays, reverbs, brain, tuning)
//   Cosmos = the Cosmos section only
// The 168 full presets carry both layers (they are the DAW programs); the Cosmos
// bank carries Cosmos-only settings.
#pragma once
#include <string>
#include "Params.h"
#include <cstring>

namespace ambient {

struct Preset {
    const char* name;
    const char* settings;             // "key=value;key=value", choice values may be given by name
    const char* texture = nullptr;    // file the host should load into the Texture slots (pack presets)
    const char* wavetable = nullptr;  // file the host should load into the User wavetable
    const char* impulse = nullptr;    // file the host should load into the convolution Room
    // Modulation is data, not parameters (see Modulation.h): the matrix rows, and the envelope
    // shapes separated by '~' -- the six modulation envelopes, then the four sources' own (for a
    // slot whose Env is Own). Empty, or a list that stops early, means "the defaults" for the rest.
    const char* mod = nullptr;
    const char* envs = nullptr;
};

// A preset can be the whole instrument or one section of it. The section scopes are layers: a
// bank that lands on top of whatever sound is loaded, resetting only its own section first.
enum class PresetScope { Full, Sound, Cosmos, ZPlane, Strike };

// The preset list is the 168 built-in presets followed by every loaded pack, so everything that
// walks presets by index (DAW programs, the map, routes, the browser) sees packs automatically.
int numPresets();
// A line of prose about a preset, in the manner of u-he's browsers: what it sounds like (from the
// measured descriptors) and what is in it (from its own settings). Generated, never stored, and
// never a guess -- see Core/src/PresetText.cpp.
std::string presetDescription(int index);
// The whole card, in the manner of u-he's PRESET INFO: the description, how it paces itself, what
// the macros and the wheel are wired to in this preset, and where it is filed. Lines, not a
// paragraph -- the browser draws the headings.
std::string presetInfoText(int index);
const Preset& preset(int index);
int builtinPresetCount();                 // the compiled-in presets (the first ones)
const Preset& builtinPreset(int index);
// The three section layers. Each has its own bank, its own families for the list, and touches
// nothing outside its section.
int numCosmosPresets();                   // Cosmos-only bank
const Preset& cosmosPreset(int index);
int cosmosPresetCategory(int index);      // 255 for the Off entry, which belongs to no family
int numCosmosPresetFamilies();
const char* cosmosPresetFamily(int family);

int numZPresets();                        // Z-plane-only bank: one preset per filter shape
const Preset& zPreset(int index);
int zPresetCategory(int index);           // the shape's family (see ZPlane.h), 255 for Off

int numStrikePresets();                   // Strike-only bank (the Karplus-Strong pluck)
const Preset& strikePreset(int index);
int strikePresetCategory(int index);
int numStrikePresetFamilies();
const char* strikePresetFamily(int family);

inline bool isCosmosParam(ParamId id) { return sectionOf(id) == ParamSection::Cosmos; }
inline bool isZPlaneParam(ParamId id) { return sectionOf(id) == ParamSection::ZPlane; }
inline bool isStrikeParam(ParamId id) { return sectionOf(id) == ParamSection::Strike; }
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
    switch (scope) {
        case PresetScope::Cosmos: return isCosmosParam(id);
        case PresetScope::ZPlane: return isZPlaneParam(id);
        case PresetScope::Strike: return isStrikeParam(id);
        case PresetScope::Sound:  return !isCosmosParam(id);   // everything a sound preset owns
        case PresetScope::Full:   break;
    }
    return true;
}

// ---------------------------------------------------------------- preset packs
//
// A pack is a UTF-8 text file (.ambientpack) of presets, loaded at runtime instead of compiled
// in, so a library of thousands does not live in the binary:
//
//   # comment
//   pack <pack name>
//   <name>|<settings>|<x y bright motion width noisy bass density tagbits>|<texture>|<wavetable>|
//   <impulse>|<mod matrix>|<env shapes, '~' between them: Env 1..6, then Source 1..4's own>
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
