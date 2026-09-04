// AmbientSynth -- minimal WAV reader (PCM 8/16/24/32 and 32-bit float, any channel
// count mixed to mono). Framework-free so the render tool and the Quest app can load
// textures and wavetables; the plugin uses JUCE's readers for other formats.
#pragma once
#include <vector>

namespace ambient {

// Returns false if the file is missing or not a WAV the reader understands.
bool readWavMono(const char* path, std::vector<float>& mono, int& sampleRate);

} // namespace ambient
