#include "ambient/Route.h"
#include "ambient/Presets.h"
#include "ambient/PresetMeta.h"
#include "ambient/Dsp.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

namespace ambient {

namespace {
// Twelve routes over the map, given by preset names so they follow the measured positions.
// travel | hold in seconds; a whole route is a 20-40 minute set at speed 1.
const RoutePreset kRoutes[] = {
    { "Night Descent",
      "Sleeping Descent|30|90;Somnus Bed|90|120;Glacial Interval|120|180;Cavern Hollow|150|240;Gravity Weight|180|300" },
    { "Glass to Storm",
      "Frost Span|30|120;Wineglass Field|90|120;Swarm Expanse|90|90;Storm Vigil|150|180;Drizzle Veil|120|120;Frost Span|240|120" },
    { "Breath and Choir",
      "Choral Morph|30|120;Choral Psalm|120|150;Vowel Voices|120|180;Organ Threshold|150|180;Vowel Basin|180|120" },
    { "Cosmos Crossing",
      "Stellar Horizon|30|120;Nebula Field|150|180;Turning Interval|150|180;Circling Expanse|120|180;Nebula Wind|200|240" },
    { "Slow Tide Loop",
      "Glacial Interval|30|150;Tidal Field|120|150;Season Arc|120|150;Hour Vigil|120|150" },
    { "Sub Journey",
      "Gravity Threshold|30|150;Cavern Hollow|150|180;Hollow Signal|150|180;Anvil Field|150|150;Abyss Descent|180|180" },
    { "Keys Interlude",
      "Touch Room|20|120;Wooden Reach|60|120;Near Signal|90|120;Voicing Interval|90|150;Hammer Hands|120|120" },
    { "Metal and Feedback",
      "Steel Feedback|30|150;Copper Bloom|120|150;Bar Signal|120|150;Foundry Veil|150|180;Foundry Expanse|180|150" },
    { "Ninety Minute Arc",
      "Glacial Interval|30|600;Epoch Hours|300|900;Epoch Reach|600|900" },
    { "Storm Front",
      "Wind Basin|30|150;Drizzle Veil|120|120;Echo Cloud|120|150;Fog Bed|150|180;Breath Span|180|240" },
    { "Between Two Worlds",
      "Somnus Vigil|30|60;Gust Threshold|300|60;Somnus Vigil|300|60" },
    { "Wide Wander",
      "0.15,0.85|30|60|0.14;0.85,0.85|240|60|0.14;0.85,0.15|240|60|0.14;0.15,0.15|240|60|0.14;0.5,0.5|240|120|0.2" },
};
}

int numRoutePresets() { return static_cast<int>(sizeof(kRoutes) / sizeof(kRoutes[0])); }
const RoutePreset& routePreset(int index) { return kRoutes[clampv(index, 0, numRoutePresets() - 1)]; }

bool Route::parse(const char* text)
{
    Waypoint pts[kMaxPoints];
    int n = 0;
    const char* s = text;
    if (s == nullptr) return false;
    while (*s) {
        while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') ++s;
        if (!*s) break;
        const char* e = s; while (*e && *e != ';') ++e;
        // split by '|'
        char buf[192];
        const int len = static_cast<int>(e - s);
        if (len <= 0 || len >= static_cast<int>(sizeof(buf))) return false;
        std::memcpy(buf, s, static_cast<size_t>(len)); buf[len] = 0;
        char* fields[4] = { buf, nullptr, nullptr, nullptr };
        int nf = 1;
        for (char* p = buf; *p && nf < 4; ++p) if (*p == '|') { *p = 0; fields[nf++] = p + 1; }
        if (nf < 3) return false;
        // trim the position field
        char* pos = fields[0]; while (*pos == ' ') ++pos;
        for (char* q = pos + std::strlen(pos); q > pos && q[-1] == ' '; --q) q[-1] = 0;
        Waypoint w;
        char* endp = nullptr;
        const float fx = std::strtof(pos, &endp);
        if (endp != pos && *endp == ',') {   // x,y
            w.x = clampv(fx, 0.0f, 1.0f);
            w.y = clampv(std::strtof(endp + 1, nullptr), 0.0f, 1.0f);
        } else {                              // preset name
            int found = -1;
            for (int p = 0; p < numPresets(); ++p) if (std::strcmp(preset(p).name, pos) == 0) { found = p; break; }
            if (found < 0) return false;
            const PresetMeta& m = presetMeta(found);
            w.x = m.x; w.y = m.y; w.preset = found;
        }
        w.travel = std::max(0.0f, std::strtof(fields[1], nullptr));
        w.hold   = std::max(0.0f, std::strtof(fields[2], nullptr));
        if (nf >= 4) w.radius = clampv(std::strtof(fields[3], nullptr), 0.02f, 0.4f);
        if (n >= kMaxPoints) return false;
        pts[n++] = w;
        s = (*e == ';') ? e + 1 : e;
    }
    if (n == 0) return false;
    for (int i = 0; i < n; ++i) points_[i] = pts[i];
    count_ = n;
    stop();
    return true;
}

int Route::write(char* buf, size_t cap) const
{
    size_t used = 0;
    for (int i = 0; i < count_; ++i) {
        const Waypoint& w = points_[i];
        char item[160];
        int len;
        if (w.preset >= 0 && w.preset < numPresets())
            len = std::snprintf(item, sizeof(item), "%s|%g|%g|%g", preset(w.preset).name, w.travel, w.hold, w.radius);
        else
            len = std::snprintf(item, sizeof(item), "%.3f,%.3f|%g|%g|%g", w.x, w.y, w.travel, w.hold, w.radius);
        if (len <= 0 || len >= static_cast<int>(sizeof(item))) return 0;   // a name too long for the item is a name the text cannot hold
        const size_t need = static_cast<size_t>(len) + (i > 0 ? 1 : 0);
        if (used + need + 1 > cap) return 0;
        if (i > 0) buf[used++] = ';';
        std::memcpy(buf + used, item, static_cast<size_t>(len)); used += static_cast<size_t>(len);
    }
    buf[used] = 0;
    return static_cast<int>(used);
}

void Route::start(float fromX, float fromY, float fromRadius, bool loop)
{
    if (count_ == 0) return;
    fromX_ = curX_ = fromX; fromY_ = curY_ = fromY; fromR_ = curR_ = fromRadius;
    loop_ = loop; running_ = true; holding_ = false; seg_ = 0; prog_ = 0.0f; elapsed_ = 0.0f;
}

bool Route::update(float dt, float& x, float& y, float& radius)
{
    if (!running_ || count_ == 0) { x = curX_; y = curY_; radius = curR_; return false; }
    elapsed_ += std::max(dt, 0.0f);
    const Waypoint& w = points_[seg_];
    if (!holding_) {
        const float t = w.travel > 1e-3f ? clampv(elapsed_ / w.travel, 0.0f, 1.0f) : 1.0f;
        const float c = t * t * (3.0f - 2.0f * t);
        curX_ = fromX_ + (w.x - fromX_) * c;
        curY_ = fromY_ + (w.y - fromY_) * c;
        curR_ = fromR_ + (w.radius - fromR_) * c;
        prog_ = t;
        if (t >= 1.0f) { holding_ = true; elapsed_ = 0.0f; prog_ = 0.0f; }
    } else {
        curX_ = w.x; curY_ = w.y; curR_ = w.radius;
        prog_ = w.hold > 1e-3f ? clampv(elapsed_ / w.hold, 0.0f, 1.0f) : 1.0f;
        if (elapsed_ >= w.hold) {
            fromX_ = w.x; fromY_ = w.y; fromR_ = w.radius;
            holding_ = false; elapsed_ = 0.0f; prog_ = 0.0f;
            if (seg_ + 1 < count_) ++seg_;
            else if (loop_) seg_ = 0;
            else { running_ = false; }
        }
    }
    x = curX_; y = curY_; radius = curR_;
    return running_;
}

} // namespace ambient
