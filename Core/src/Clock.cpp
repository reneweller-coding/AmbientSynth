#include "ambient/Clock.h"

namespace ambient {

const char* const kClockSourceNames[kNumClockSources] = { "Internal", "Host", "MIDI" };

const char* const kSyncDivNames[kNumSyncDivs] = {
    "Free", "64 bars", "32 bars", "16 bars", "8 bars", "4 bars", "2 bars", "1 bar",
    "1/2", "1/2 T", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16", "1/16 T", "1/32",
};

const double kSyncBeats[kNumSyncDivs] = {
    0.0, 256.0, 128.0, 64.0, 32.0, 16.0, 8.0, 4.0,
    2.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125,
};

} // namespace ambient
