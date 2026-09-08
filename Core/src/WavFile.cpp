#include "ambient/WavFile.h"
// One translation unit defines dr_flac; everything else here is our own.
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG            // no Ogg-FLAC in this library, and it halves the object
#if defined(_MSC_VER)
  #pragma warning(push, 0)        // somebody else's file: our /W4 is not its business
#endif
#include "dr_flac.h"
#if defined(_MSC_VER)
  #pragma warning(pop)
#endif
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <io.h>
  #include <fcntl.h>
#endif

namespace ambient {

namespace {
// A sample is read once and then never again, but the file cache has no way of knowing that: a
// batch that renders five thousand presets reads eleven gigabytes of clips and impulses, and
// every byte of it stays resident afterwards. Windows filled its standby list to 38 GB that way
// and started trimming the working sets of the applications on screen instead.
// FILE_FLAG_SEQUENTIAL_SCAN tells the cache manager to age these pages out immediately.
FILE* openRead(const char* path)
{
#if defined(_WIN32)
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE) return std::fopen(path, "rb");   // fall back rather than fail
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(h), _O_RDONLY | _O_BINARY);
    if (fd < 0) { CloseHandle(h); return std::fopen(path, "rb"); }
    FILE* f = _fdopen(fd, "rb");
    if (f == nullptr) { _close(fd); return std::fopen(path, "rb"); }
    return f;
#else
    return std::fopen(path, "rb");
#endif
}
} // namespace

bool readWavMono(const char* path, std::vector<float>& mono, int& sampleRate)
{
    std::vector<std::vector<float>> ch;
    if (!readWavChannels(path, ch, sampleRate) || ch.empty()) { mono.clear(); return false; }
    mono.assign(ch[0].size(), 0.0f);
    const float inv = 1.0f / static_cast<float>(ch.size());
    for (const auto& c : ch) for (size_t i = 0; i < mono.size() && i < c.size(); ++i) mono[i] += c[i] * inv;
    return true;
}

namespace {

// FLAC, for the sample library. The clips ship as 24-bit FLAC rather than 24-bit WAV: the same
// samples to the bit, in half the bytes (measured on this library, 39-44 % of the float originals
// against 75 % for 24-bit PCM). It costs a decode when a preset loads -- milliseconds for a
// twelve-second clip, and never on the audio thread -- and nothing at all in memory afterwards,
// because what comes out is the same block of floats either way.
bool readFlacChannels(const char* path, std::vector<std::vector<float>>& channelsOut, int& sampleRate)
{
    unsigned int channels = 0, rate = 0;
    drflac_uint64 frames = 0;
    float* pcm = drflac_open_file_and_read_pcm_frames_f32(path, &channels, &rate, &frames, nullptr);
    if (pcm == nullptr) return false;
    if (channels == 0 || frames == 0) { drflac_free(pcm, nullptr); return false; }
    sampleRate = static_cast<int>(rate);
    channelsOut.assign(channels, std::vector<float>(static_cast<size_t>(frames), 0.0f));
    for (drflac_uint64 i = 0; i < frames; ++i)
        for (unsigned int c = 0; c < channels; ++c)
            channelsOut[c][static_cast<size_t>(i)] = pcm[i * channels + c];
    drflac_free(pcm, nullptr);
    return true;
}

// What a file is, from its first four bytes rather than from its name: a name can be wrong, and a
// reader that trusts the extension hands back silence without a word.
bool looksLikeFlac(const char* path)
{
    FILE* f = openRead(path);
    if (f == nullptr) return false;
    char tag[4] = { 0, 0, 0, 0 };
    const bool got = std::fread(tag, 1, 4, f) == 4;
    std::fclose(f);
    return got && std::memcmp(tag, "fLaC", 4) == 0;
}

// A preset names its clip as it lies in the source library -- "../Textures/a_bell.wav" -- while
// what ships is the same audio as FLAC, at half the download. Rather than rewrite every reference
// in every pack (and break every pack anybody else has written), the named file is looked for
// first and the FLAC beside it second. One rule, in the one place that opens audio at all.
std::string withFlacExtension(const char* path)
{
    std::string s(path == nullptr ? "" : path);
    const size_t dot = s.find_last_of('.');
    const size_t slash = s.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return s + ".flac";
    return s.substr(0, dot) + ".flac";
}

bool exists(const std::string& path)
{
    FILE* f = openRead(path.c_str());
    if (f == nullptr) return false;
    std::fclose(f);
    return true;
}

} // namespace

bool readWavChannels(const char* path, std::vector<std::vector<float>>& channelsOut, int& sampleRate)
{
    channelsOut.clear();
    if (path == nullptr) return false;
    if (!exists(path)) {
        const std::string alt = withFlacExtension(path);
        if (exists(alt)) return readFlacChannels(alt.c_str(), channelsOut, sampleRate);
        return false;
    }
    if (looksLikeFlac(path)) return readFlacChannels(path, channelsOut, sampleRate);
    FILE* f = openRead(path);
    if (!f) return false;
    auto rd32 = [&](uint32_t& v) { return std::fread(&v, 4, 1, f) == 1; };
    auto rd16 = [&](uint16_t& v) { return std::fread(&v, 2, 1, f) == 1; };
    char tag[4]; uint32_t size = 0;
    if (std::fread(tag, 1, 4, f) != 4 || std::memcmp(tag, "RIFF", 4) != 0 || !rd32(size) ||
        std::fread(tag, 1, 4, f) != 4 || std::memcmp(tag, "WAVE", 4) != 0) { std::fclose(f); return false; }
    uint16_t format = 0, channels = 0, bits = 0; uint32_t rate = 0;
    bool haveFmt = false, ok = false;
    while (std::fread(tag, 1, 4, f) == 4 && rd32(size)) {
        const long next = std::ftell(f) + static_cast<long>(size + (size & 1u));
        if (std::memcmp(tag, "fmt ", 4) == 0) {
            uint16_t blockAlign = 0; uint32_t byteRate = 0;
            if (!rd16(format) || !rd16(channels) || !rd32(rate) || !rd32(byteRate) || !rd16(blockAlign) || !rd16(bits)) break;
            if (format == 0xFFFE && size >= 26) {   // WAVE_FORMAT_EXTENSIBLE: the sub-format GUID starts with the real tag
                uint16_t cbSize = 0, validBits = 0; uint32_t mask = 0; uint16_t sub = 0;
                if (rd16(cbSize) && rd16(validBits) && rd32(mask) && rd16(sub)) format = sub;
            }
            haveFmt = channels > 0 && rate > 0 && (bits == 8 || bits == 16 || bits == 24 || bits == 32);
        } else if (std::memcmp(tag, "data", 4) == 0 && haveFmt) {
            const int bytes = bits / 8;
            const uint32_t frames = size / static_cast<uint32_t>(bytes * channels);
            channelsOut.assign(channels, std::vector<float>(frames, 0.0f));
            std::vector<unsigned char> buf(static_cast<size_t>(bytes * channels));
            uint32_t got = frames;
            for (uint32_t i = 0; i < frames; ++i) {
                if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) { got = i; break; }
                for (int c = 0; c < channels; ++c) {
                    const unsigned char* p = buf.data() + c * bytes;
                    float v = 0.0f;
                    if (format == 3 && bits == 32) { float fv; std::memcpy(&fv, p, 4); v = fv; }
                    else if (bits == 8)  v = (static_cast<int>(p[0]) - 128) / 128.0f;
                    else if (bits == 16) v = static_cast<int16_t>(p[0] | (p[1] << 8)) / 32768.0f;
                    else if (bits == 24) { int32_t s = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = static_cast<float>(s >> 8) / 8388608.0f; }
                    else if (bits == 32) { int32_t s; std::memcpy(&s, p, 4); v = static_cast<float>(s) / 2147483648.0f; }
                    channelsOut[static_cast<size_t>(c)][i] = v;
                }
            }
            for (auto& c : channelsOut) c.resize(got);
            sampleRate = static_cast<int>(rate);
            ok = got > 0;
            break;
        }
        if (std::fseek(f, next, SEEK_SET) != 0) break;
    }
    std::fclose(f);
    return ok;
}

} // namespace ambient
