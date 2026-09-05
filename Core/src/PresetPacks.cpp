// AmbientSynth -- runtime preset packs. The preset list the rest of the program sees is the
// built-in presets followed by every loaded pack, so the DAW programs, the map, the browser and
// routes-by-name all pick packs up without knowing they exist.
#include "ambient/Presets.h"
#include "ambient/PresetMeta.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ambient {

namespace {

struct PackEntry {
    std::string name, settings, texture, wavetable, impulse, mod, envs;
    PresetMeta  meta{ 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0, 0, 0.0f };
};
struct Pack {
    std::string name;
    std::string dir;                  // the pack file's folder, for relative sample paths
    std::vector<PackEntry> entries;
};

std::vector<Pack>& packs() { static std::vector<Pack> p; return p; }
// Preset objects handed out point into the pack strings, so they stay valid until clearPresetPacks().
std::vector<Preset>& views() { static std::vector<Preset> v; return v; }
std::vector<std::string>& paths() { static std::vector<std::string> p; return p; }   // absolute, kPresetFiles per entry

void rebuildViews()
{
    views().clear();
    paths().clear();
    for (const Pack& pk : packs())
        for (const PackEntry& e : pk.entries) {
            views().push_back(Preset{ e.name.c_str(), e.settings.c_str(),
                                      e.texture.empty() ? nullptr : e.texture.c_str(),
                                      e.wavetable.empty() ? nullptr : e.wavetable.c_str(),
                                      e.impulse.empty() ? nullptr : e.impulse.c_str(),
                                      e.mod.empty() ? nullptr : e.mod.c_str(),
                                      e.envs.empty() ? nullptr : e.envs.c_str() });
            const std::filesystem::path dir(pk.dir);
            auto resolve = [&dir](const std::string& rel) {
                return rel.empty() ? std::string() : (dir / rel).lexically_normal().string();
            };
            paths().push_back(resolve(e.texture));
            paths().push_back(resolve(e.wavetable));
            paths().push_back(resolve(e.impulse));
        }
}

std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i)
        if (i == s.size() || s[i] == sep) { out.push_back(s.substr(start, i - start)); start = i + 1; }
    return out;
}

} // namespace

bool loadPresetPack(const char* path)
{
    if (path == nullptr) return false;
    std::ifstream f(path);
    if (!f) return false;
    Pack pack;
    pack.dir = std::filesystem::path(path).parent_path().string();
    pack.name = std::filesystem::path(path).stem().string();
    std::string line;
    while (std::getline(f, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t.rfind("pack ", 0) == 0) { pack.name = trim(t.substr(5)); continue; }
        const std::vector<std::string> fields = split(t, '|');
        if (fields.size() < 2) return false;
        PackEntry e;
        e.name = trim(fields[0]);
        e.settings = trim(fields[1]);
        if (e.name.empty()) return false;
        if (fields.size() > 2) {   // x y bright motion width noisy bass density tagbits
            const std::vector<std::string> m = split(trim(fields[2]), ' ');
            float v[8] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
            size_t j = 0;
            for (const std::string& tok : m) {
                if (tok.empty()) continue;
                if (j < 8) v[j] = static_cast<float>(std::atof(tok.c_str()));
                else if (j == 8) e.meta.tags = static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 0));
                else if (j == 9) e.meta.loudDb = static_cast<float>(std::atof(tok.c_str()));   // optional: older packs have none
                ++j;
            }
            e.meta.x = v[0]; e.meta.y = v[1]; e.meta.bright = v[2]; e.meta.motion = v[3];
            e.meta.width = v[4]; e.meta.noisy = v[5]; e.meta.bass = v[6]; e.meta.density = v[7];
        }
        if (fields.size() > 3) e.texture = trim(fields[3]);
        if (fields.size() > 4) e.wavetable = trim(fields[4]);
        if (fields.size() > 5) e.impulse = trim(fields[5]);
        if (fields.size() > 6) e.mod = trim(fields[6]);
        if (fields.size() > 7) e.envs = trim(fields[7]);
        pack.entries.push_back(std::move(e));
    }
    if (pack.entries.empty()) return false;
    // Every preset of a pack belongs to that pack's family, appended after the built-in families.
    const int family = builtinPresetFamilyCount() + static_cast<int>(packs().size());
    for (PackEntry& e : pack.entries) e.meta.family = family;
    packs().push_back(std::move(pack));
    rebuildViews();
    return true;
}

int loadPresetPacksIn(const char* dir)
{
    if (dir == nullptr) return 0;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return 0;
    std::vector<std::string> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        if (entry.is_regular_file(ec) && entry.path().extension() == ".ambientpack") files.push_back(entry.path().string());
    std::sort(files.begin(), files.end());   // packs load in a stable order, so preset indices are reproducible
    int n = 0;
    for (const std::string& f : files) if (loadPresetPack(f.c_str())) ++n;
    return n;
}

int loadDefaultPresetPacks()
{
    // AMBIENT_PACKS wins (a folder, or several separated by ';'), otherwise the user's
    // Documents/AmbientSynth/Packs. Nothing there simply means no packs.
    if (const char* env = std::getenv("AMBIENT_PACKS")) {
        int n = 0;
        for (const std::string& dir : split(env, ';')) if (!dir.empty()) n += loadPresetPacksIn(dir.c_str());
        if (n > 0) return n;
    }
    const char* home = std::getenv("USERPROFILE");
    if (home == nullptr) home = std::getenv("HOME");
    if (home == nullptr) return 0;
    const std::string dir = (std::filesystem::path(home) / "Documents" / "AmbientSynth" / "Packs").string();
    return loadPresetPacksIn(dir.c_str());
}

void clearPresetPacks() { packs().clear(); rebuildViews(); }
int  numPresetPacks() { return static_cast<int>(packs().size()); }
const char* presetPackName(int pack) { return (pack >= 0 && pack < numPresetPacks()) ? packs()[static_cast<size_t>(pack)].name.c_str() : ""; }

int numPresets() { return builtinPresetCount() + static_cast<int>(views().size()); }

const Preset& preset(int index)
{
    const int b = builtinPresetCount();
    if (index < b) return builtinPreset(index);
    const size_t i = static_cast<size_t>(index - b);
    return i < views().size() ? views()[i] : builtinPreset(0);
}

const char* presetFilePath(int presetIndex, int which)
{
    const int b = builtinPresetCount();
    if (which < 0 || which >= kPresetFiles) return "";
    const size_t i = static_cast<size_t>(presetIndex - b) * kPresetFiles + static_cast<size_t>(which);
    return (presetIndex >= b && i < paths().size()) ? paths()[i].c_str() : "";
}

// ---------------------------------------------------------------- metadata

int numPresetMeta() { return builtinPresetMetaCount() > 0 ? numPresets() : 0; }

const PresetMeta& presetMeta(int index)
{
    static const PresetMeta none = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0, 0 };
    const int b = builtinPresetCount();
    if (index < b) return builtinPresetMeta(index);
    int i = index - b;
    for (const Pack& pk : packs()) {
        if (i < static_cast<int>(pk.entries.size())) return pk.entries[static_cast<size_t>(i)].meta;
        i -= static_cast<int>(pk.entries.size());
    }
    return none;
}

int numPresetFamilies() { return builtinPresetFamilyCount() + numPresetPacks(); }

const char* presetFamilyName(int family)
{
    const int b = builtinPresetFamilyCount();
    return family < b ? builtinPresetFamilyName(family) : presetPackName(family - b);
}

} // namespace ambient
