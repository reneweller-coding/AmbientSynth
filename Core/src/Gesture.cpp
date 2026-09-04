#include "ambient/Gesture.h"
#include "ambient/Dsp.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace ambient {

namespace {
const char* const kInputNames[kNumGestureInputs] = {
    "HandDistance", "LeftHeight", "RightHeight", "LeftForward", "RightForward",
    "LeftTilt", "RightTilt", "LeftPinch", "RightPinch", "HeadYaw", "HeadPitch", "HeadRoll",
    "Custom0", "Custom1", "Custom2", "Custom3", "Custom4", "Custom5", "Custom6", "Custom7",
};
float norm01(float v, float lo, float hi) { return clampv((v - lo) / (hi - lo), 0.0f, 1.0f); }
}

const char* gestureInputName(GestureInput in)
{
    const int i = static_cast<int>(in);
    return (i >= 0 && i < kNumGestureInputs) ? kInputNames[i] : "none";
}

bool gestureInputFromName(const char* name, GestureInput& out)
{
    if (name == nullptr) return false;
    for (int i = 0; i < kNumGestureInputs; ++i)
        if (std::strcmp(name, kInputNames[i]) == 0) { out = static_cast<GestureInput>(i); return true; }
    if (std::strcmp(name, "none") == 0 || std::strcmp(name, "-") == 0) { out = GestureInput::Count; return true; }
    return false;
}

GestureLayer::GestureLayer()
{
    for (auto& i : inputs_) i.store(0.0f, std::memory_order_relaxed);
    setDefaultMappings();
}

void GestureLayer::setInput(GestureInput in, float normalised)
{
    const int i = static_cast<int>(in);
    if (i < 0 || i >= kNumGestureInputs) return;
    inputs_[i].store(clampv(normalised, 0.0f, 1.0f), std::memory_order_relaxed);
    updates_.fetch_add(1, std::memory_order_relaxed);
}

void GestureLayer::setCalibration(float heightLow, float heightHigh, float reachNear, float reachFar, float distNear, float distFar)
{
    hLow_ = heightLow; hHigh_ = heightHigh; rNear_ = reachNear; rFar_ = reachFar; dNear_ = distNear; dFar_ = distFar;
}

void GestureLayer::setHand(int hand, float x, float y, float z, float pinch, float tilt)
{
    const int h = hand & 1;
    handX_[h] = x; handY_[h] = y; handZ_[h] = z;
    setInput(h == 0 ? GestureInput::LeftHeight : GestureInput::RightHeight, norm01(y, hLow_, hHigh_));
    setInput(h == 0 ? GestureInput::LeftForward : GestureInput::RightForward, norm01(-z, rNear_, rFar_));
    setInput(h == 0 ? GestureInput::LeftPinch : GestureInput::RightPinch, pinch);
    setInput(h == 0 ? GestureInput::LeftTilt : GestureInput::RightTilt, 0.5f + 0.5f * clampv(tilt, -1.0f, 1.0f));
    const float dx = handX_[0] - handX_[1], dy = handY_[0] - handY_[1], dz = handZ_[0] - handZ_[1];
    setInput(GestureInput::HandDistance, norm01(std::sqrt(dx * dx + dy * dy + dz * dz), dNear_, dFar_));
}

void GestureLayer::setHead(float yawDeg, float pitchDeg, float rollDeg)
{
    setInput(GestureInput::HeadYaw,   0.5f + 0.5f * clampv(yawDeg / 90.0f, -1.0f, 1.0f));
    setInput(GestureInput::HeadPitch, 0.5f + 0.5f * clampv(pitchDeg / 90.0f, -1.0f, 1.0f));
    setInput(GestureInput::HeadRoll,  0.5f + 0.5f * clampv(rollDeg / 90.0f, -1.0f, 1.0f));
}

bool GestureLayer::addMapping(const GestureMapping& m)
{
    if (numMappings_ >= kMaxMappings) return false;
    maps_[numMappings_] = m;
    state_[numMappings_] = State{};
    ++numMappings_;
    return true;
}

void GestureLayer::setDefaultMappings()
{
    // A first vocabulary for a 30-minute set. The right pinch is the clutch:
    // nothing moves unless the right hand is holding the instrument.
    clearMappings();
    const GestureInput clutch = GestureInput::RightPinch;
    addMapping({ GestureInput::HandDistance, ParamId::MorphPos,    0.0f, 1.0f, 0.5f,  0.01f, clutch, false });
    addMapping({ GestureInput::LeftHeight,   ParamId::Depth,       0.0f, 1.0f, 0.5f,  0.01f, clutch, false });
    addMapping({ GestureInput::RightHeight,  ParamId::Brightness,  0.2f, 1.0f, 0.5f,  0.01f, clutch, false });
    addMapping({ GestureInput::LeftTilt,     ParamId::CosmosSend,  0.0f, 1.0f, 0.5f,  0.02f, clutch, false });
    addMapping({ GestureInput::RightTilt,    ParamId::FarLevel,    0.2f, 1.0f, 0.5f,  0.02f, clutch, false });
    addMapping({ GestureInput::LeftForward,  ParamId::CloudSend,   0.0f, 1.0f, 0.5f,  0.02f, clutch, false });
    addMapping({ GestureInput::RightForward, ParamId::DelayMix,    0.0f, 0.6f, 0.5f,  0.02f, clutch, false });
    addMapping({ GestureInput::HeadYaw,      ParamId::Width,       0.6f, 1.8f, 1.0f,  0.02f, GestureInput::Count, false });
}

bool GestureLayer::parseMappings(const char* text)
{
    if (text == nullptr) return false;
    GestureMapping parsed[kMaxMappings];
    int n = 0;
    const char* p = text;
    while (*p) {
        const char* end = p;
        while (*end && *end != '\n') ++end;
        char line[256];
        size_t len = static_cast<size_t>(end - p);
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        std::memcpy(line, p, len);
        line[len] = 0;
        p = (*end) ? end + 1 : end;
        // tokens
        char* tok[8] = {};
        int nt = 0;
        char* cur = line;
        while (nt < 8) {
            while (*cur == ' ' || *cur == '\t' || *cur == '\r') ++cur;
            if (*cur == 0 || *cur == '#') break;
            tok[nt++] = cur;
            while (*cur && *cur != ' ' && *cur != '\t' && *cur != '\r') ++cur;
            if (*cur) { *cur = 0; ++cur; }
        }
        if (nt == 0) continue;
        if (nt < 4) return false;
        GestureMapping m;
        if (!gestureInputFromName(tok[0], m.input) || m.input == GestureInput::Count) return false;
        const ParamDesc* d = findParam(tok[1]);
        if (d == nullptr) return false;
        m.param = d->id;
        m.min = static_cast<float>(std::atof(tok[2]));
        m.max = static_cast<float>(std::atof(tok[3]));
        if (nt > 4) m.smoothSeconds = static_cast<float>(std::atof(tok[4]));
        if (nt > 5) m.deadzone = static_cast<float>(std::atof(tok[5]));
        if (nt > 6 && !gestureInputFromName(tok[6], m.clutch)) return false;
        if (nt > 7) m.invert = std::atoi(tok[7]) != 0;
        if (n >= kMaxMappings) return false;
        parsed[n++] = m;
    }
    clearMappings();
    for (int i = 0; i < n; ++i) addMapping(parsed[i]);
    return true;
}

int GestureLayer::writeMappings(char* out, int capacity) const
{
    int pos = 0;
    for (int i = 0; i < numMappings_ && pos < capacity; ++i) {
        const GestureMapping& m = maps_[i];
        pos += std::snprintf(out + pos, static_cast<size_t>(capacity - pos), "%s %s %g %g %g %g %s %d\n",
                             gestureInputName(m.input), paramDesc(m.param).key, m.min, m.max, m.smoothSeconds, m.deadzone,
                             m.clutch == GestureInput::Count ? "none" : gestureInputName(m.clutch), m.invert ? 1 : 0);
    }
    return pos;
}

} // namespace ambient
