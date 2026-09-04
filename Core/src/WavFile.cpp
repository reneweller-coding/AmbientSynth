#include "ambient/WavFile.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace ambient {

bool readWavMono(const char* path, std::vector<float>& mono, int& sampleRate)
{
    mono.clear();
    FILE* f = std::fopen(path, "rb");
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
            mono.resize(frames);
            std::vector<unsigned char> buf(static_cast<size_t>(bytes * channels));
            const float inv = 1.0f / static_cast<float>(channels);
            for (uint32_t i = 0; i < frames; ++i) {
                if (std::fread(buf.data(), 1, buf.size(), f) != buf.size()) { mono.resize(i); break; }
                float sum = 0.0f;
                for (int c = 0; c < channels; ++c) {
                    const unsigned char* p = buf.data() + c * bytes;
                    float v = 0.0f;
                    if (format == 3 && bits == 32) { float fv; std::memcpy(&fv, p, 4); v = fv; }
                    else if (bits == 8)  v = (static_cast<int>(p[0]) - 128) / 128.0f;
                    else if (bits == 16) v = static_cast<int16_t>(p[0] | (p[1] << 8)) / 32768.0f;
                    else if (bits == 24) { int32_t s = (p[0] << 8) | (p[1] << 16) | (p[2] << 24); v = static_cast<float>(s >> 8) / 8388608.0f; }
                    else if (bits == 32) { int32_t s; std::memcpy(&s, p, 4); v = static_cast<float>(s) / 2147483648.0f; }
                    sum += v;
                }
                mono[i] = sum * inv;
            }
            sampleRate = static_cast<int>(rate);
            ok = !mono.empty();
            break;
        }
        if (std::fseek(f, next, SEEK_SET) != 0) break;
    }
    std::fclose(f);
    return ok;
}

} // namespace ambient
