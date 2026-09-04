// AmbientSynth -- gesture layer: a few continuous inputs (hands, head, or anything
// a controller sends) mapped onto parameters with range, smoothing, jitter
// dead-zone and a clutch. Framework-free: the same code runs behind an OpenXR
// hand tracker on the Quest and behind the OSC receiver on the desktop.
//
// Inputs are normalised to 0..1 (tilts and head angles are mapped from -1..1).
// The layer never writes parameters itself; update() hands (ParamId, value) to a
// sink so each host can route it into its own parameter system.
#pragma once
#include "Params.h"
#include <atomic>
#include <cstdint>
#include <cmath>

namespace ambient {

enum class GestureInput : int {
    HandDistance,     // distance between the hands, 0.1 .. 0.8 m
    LeftHeight, RightHeight,     // hand height, 0.9 .. 1.7 m
    LeftForward, RightForward,   // hand reach in front of the head, 0.2 .. 0.7 m
    LeftTilt, RightTilt,         // palm roll, -1 .. 1
    LeftPinch, RightPinch,       // pinch strength 0 .. 1
    HeadYaw, HeadPitch, HeadRoll,   // -1 .. 1 (= -90 .. 90 degrees)
    Custom0, Custom1, Custom2, Custom3, Custom4, Custom5, Custom6, Custom7,
    Count
};
constexpr int kNumGestureInputs = static_cast<int>(GestureInput::Count);
const char* gestureInputName(GestureInput);
bool gestureInputFromName(const char* name, GestureInput& out);

struct GestureMapping {
    GestureInput input = GestureInput::HandDistance;
    ParamId      param = ParamId::MorphPos;
    float        min = 0.0f, max = 1.0f;      // parameter value at input 0 and 1
    float        smoothSeconds = 0.3f;        // one-pole glide of the parameter
    float        deadzone = 0.01f;            // ignore input changes smaller than this
    GestureInput clutch = GestureInput::Count; // Count = always engaged; else engaged while clutch > 0.5
    bool         invert = false;
};

class GestureLayer {
public:
    static constexpr int kMaxMappings = 32;

    GestureLayer();

    // Inputs, any thread.
    void setInput(GestureInput in, float normalised);
    float input(GestureInput in) const { return inputs_[static_cast<int>(in)].load(std::memory_order_relaxed); }
    // Raw poses in metres (OpenXR convention: x right, y up, z back), relative to the stage.
    // Derives distance, heights, reach; tilt and pinch are passed through.
    void setHand(int hand, float x, float y, float z, float pinch, float tilt);
    void setHead(float yawDeg, float pitchDeg, float rollDeg);
    void setCalibration(float heightLow, float heightHigh, float reachNear, float reachFar, float distNear, float distFar);

    // Mappings, message thread (the audio thread reads them; keep changes rare).
    int  numMappings() const { return numMappings_; }
    const GestureMapping& mapping(int i) const { return maps_[i]; }
    void clearMappings() { numMappings_ = 0; }
    bool addMapping(const GestureMapping& m);
    void setDefaultMappings();
    // Text form, one mapping per line:
    //   input param min max smooth deadzone clutch invert
    // e.g. "HandDistance morph 0 1 0.5 0.01 RightPinch 0"
    bool parseMappings(const char* text);
    int  writeMappings(char* out, int capacity) const;

    // Audio thread, once per block. Calls sink(ParamId, float) for every mapping
    // whose target changed. Returns the number of parameters written.
    template <class Sink>
    int update(double dt, Sink&& sink)
    {
        int written = 0;
        for (int i = 0; i < numMappings_; ++i) {
            const GestureMapping& m = maps_[i];
            State& s = state_[i];
            float x = input(m.input);
            const bool engaged = (m.clutch == GestureInput::Count) || input(m.clutch) > 0.5f;
            if (engaged) {
                if (!s.hasTarget || std::fabs(x - s.lastInput) >= m.deadzone) { s.lastInput = x; s.hasTarget = true; }
            }
            if (!s.hasTarget) continue;
            float u = s.lastInput;
            if (m.invert) u = 1.0f - u;
            const float target = m.min + (m.max - m.min) * u;
            if (!s.primed) { s.value = target; s.primed = true; }
            const float coef = m.smoothSeconds <= 1e-4 ? 1.0f : 1.0f - static_cast<float>(std::exp(-dt / m.smoothSeconds));
            const float next = s.value + (target - s.value) * coef;
            if (std::fabs(next - s.value) > 1e-6f || !s.sent) {
                s.value = next; s.sent = true;
                sink(m.param, next);
                ++written;
            }
        }
        return written;
    }

    uint64_t inputUpdates() const { return updates_.load(std::memory_order_relaxed); }

private:
    struct State { float lastInput = 0.0f, value = 0.0f; bool hasTarget = false, primed = false, sent = false; };
    std::atomic<float> inputs_[kNumGestureInputs];
    std::atomic<uint64_t> updates_{ 0 };
    GestureMapping maps_[kMaxMappings];
    State state_[kMaxMappings];
    int numMappings_ = 0;
    float handX_[2] = {}, handY_[2] = {}, handZ_[2] = {};
    float hLow_ = 0.9f, hHigh_ = 1.7f, rNear_ = 0.2f, rFar_ = 0.7f, dNear_ = 0.1f, dFar_ = 0.8f;
};

} // namespace ambient
