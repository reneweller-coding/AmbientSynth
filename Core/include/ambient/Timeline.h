// AmbientSynth -- a set as a timeline: every parameter change and every note with its time,
// recorded while playing (hands, knobs, OSC, MIDI, routes -- everything ends up as parameter
// changes) and played back later, live or offline. A good set becomes reproducible, and
// `ambient_render --set-file` renders it again at any length or sample rate.
//
// Text form (.ambientset), one event per line, time in seconds from the start:
//   12.500 param far_decay 42.0
//   13.020 on 57 0.80
//   40.000 off 57
// Lines starting with '#' are comments. Events are kept sorted by time.
#pragma once
#include "Params.h"
#include <cstddef>
#include <vector>

namespace ambient {

struct TimelineEvent {
    enum class Type : int { Param = 0, NoteOn = 1, NoteOff = 2 };
    double t = 0.0;
    Type   type = Type::Param;
    int    a = 0;       // ParamId or note
    float  v = 0.0f;    // value or velocity
};

class SetTimeline {
public:
    void clear() { events_.clear(); cursor_ = 0; }
    size_t size() const { return events_.size(); }
    double length() const { return events_.empty() ? 0.0 : events_.back().t; }
    const TimelineEvent& event(size_t i) const { return events_[i]; }

    // Recording (message or audio thread of the host; appends, keeps order when t is monotonic).
    void add(const TimelineEvent& e);

    // Playback: seek, then step() emits every event with from <= t < to, in order.
    void seek(double t);
    template <class Sink> void step(double from, double to, Sink&& sink)
    {
        while (cursor_ < events_.size() && events_[cursor_].t < to) {
            if (events_[cursor_].t >= from) sink(events_[cursor_]);
            ++cursor_;
        }
    }
    bool finished() const { return cursor_ >= events_.size(); }

    // Text form. save/load allocate (message thread); parse/write work on memory.
    bool parse(const char* text);
    std::vector<char> write() const;   // NUL-terminated
    bool save(const char* path) const;
    bool load(const char* path);

private:
    std::vector<TimelineEvent> events_;
    size_t cursor_ = 0;
};

} // namespace ambient
