#include "ambient/Osc.h"
#include "ambient/Presets.h"
#include "ambient/Dsp.h"
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using SocketHandle = SOCKET;
  static const SocketHandle kInvalidSocket = INVALID_SOCKET;
  static void closeSocket(SocketHandle s) { closesocket(s); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <sys/select.h>
  #include <sys/time.h>
  using SocketHandle = int;
  static const SocketHandle kInvalidSocket = -1;
  static void closeSocket(SocketHandle s) { ::close(s); }
#endif

namespace ambient {

namespace {

bool startsWith(const char* s, const char* prefix) { return std::strncmp(s, prefix, std::strlen(prefix)) == 0; }

int presetIndexFromArg(const OscMessage& m, bool cosmos)
{
    if (m.numArgs < 1) return -1;
    const int count = cosmos ? numCosmosPresets() : numPresets();
    if (m.types[0] == 's') {
        for (int i = 0; i < count; ++i) if (std::strcmp(m.strings[0], (cosmos ? cosmosPreset(i) : preset(i)).name) == 0) return i;
        return -1;
    }
    const int i = static_cast<int>(std::lround(m.floats[0]));
    return (i >= 0 && i < count) ? i : -1;
}

} // namespace

bool dispatchOsc(const OscMessage& m, OscSink& sink, GestureLayer& gestures)
{
    const char* a = m.address;
    if (!startsWith(a, "/ambient/")) return false;
    a += 9;
    if (startsWith(a, "param/") || startsWith(a, "paramn/")) {
        const bool normalised = a[5] == 'n';
        const char* key = a + (normalised ? 7 : 6);
        const ParamDesc* d = findParam(key);
        if (d == nullptr || m.numArgs < 1) return false;
        if (m.types[0] == 's') { sink.setParam(d->id, paramValueFromText(*d, m.strings[0])); return true; }
        if (normalised) sink.setParamNormalised(d->id, clampv(m.floats[0], 0.0f, 1.0f));
        else sink.setParam(d->id, clampv(m.floats[0], d->min, d->max));
        return true;
    }
    if (startsWith(a, "gesture/")) {
        GestureInput in;
        if (!gestureInputFromName(a + 8, in) || in == GestureInput::Count || m.numArgs < 1) return false;
        gestures.setInput(in, m.floats[0]);
        return true;
    }
    if (startsWith(a, "hand/")) {
        const char h = a[5];
        if ((h != 'L' && h != 'R') || m.numArgs < 3) return false;
        gestures.setHand(h == 'L' ? 0 : 1, m.floats[0], m.floats[1], m.floats[2],
                         m.numArgs > 3 ? m.floats[3] : 0.0f, m.numArgs > 4 ? m.floats[4] : 0.0f);
        return true;
    }
    if (std::strcmp(a, "head") == 0) {
        if (m.numArgs < 1) return false;
        gestures.setHead(m.floats[0], m.numArgs > 1 ? m.floats[1] : 0.0f, m.numArgs > 2 ? m.floats[2] : 0.0f);
        return true;
    }
    if (std::strcmp(a, "morph") == 0) {
        if (m.numArgs < 1) return false;
        sink.setParam(ParamId::MorphPos, clampv(m.floats[0], 0.0f, 1.0f));
        return true;
    }
    if (std::strcmp(a, "note") == 0) {
        if (m.numArgs < 1) return false;
        const int note = static_cast<int>(std::lround(m.floats[0]));
        const float vel = m.numArgs > 1 ? m.floats[1] : 1.0f;
        const float v = vel > 1.0f ? vel / 127.0f : vel;
        sink.event(ControlEvent{ v > 0.0f ? ControlEvent::Type::NoteOn : ControlEvent::Type::NoteOff, note, v });
        return true;
    }
    if (std::strcmp(a, "preset") == 0 || std::strcmp(a, "sound") == 0 || std::strcmp(a, "cosmos") == 0) {
        const bool cosmos = a[0] == 'c';
        const int idx = presetIndexFromArg(m, cosmos);
        if (idx < 0) return false;
        const ControlEvent::Type t = cosmos ? ControlEvent::Type::CosmosPreset : (a[0] == 's' ? ControlEvent::Type::SoundPreset : ControlEvent::Type::Preset);
        sink.event(ControlEvent{ t, idx, 0.0f });
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- server

bool OscServer::start(int port, OscSink& sink, GestureLayer& gestures)
{
    stop();
    sink_ = &sink;
    gestures_ = &gestures;
    port_ = port;
    error_[0] = 0;
#if defined(_WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { std::snprintf(error_, sizeof(error_), "WSAStartup failed"); return false; }
#endif
    SocketHandle s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kInvalidSocket) { std::snprintf(error_, sizeof(error_), "socket() failed"); return false; }
    int reuse = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<unsigned short>(port));
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::snprintf(error_, sizeof(error_), "port %d is in use", port);
        closeSocket(s);
        return false;
    }
    socket_ = static_cast<long long>(s);
    stopRequested_.store(false);
    running_.store(true);
    thread_ = std::thread([this] { run(); });
    return true;
}

void OscServer::stop()
{
    if (!running_.load()) return;
    stopRequested_.store(true);
    if (thread_.joinable()) thread_.join();
    if (socket_ >= 0) { closeSocket(static_cast<SocketHandle>(socket_)); socket_ = -1; }
    running_.store(false);
#if defined(_WIN32)
    WSACleanup();
#endif
}

void OscServer::run()
{
    char buffer[4096];
    const SocketHandle s = static_cast<SocketHandle>(socket_);
    while (!stopRequested_.load(std::memory_order_relaxed)) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s, &set);
        timeval tv{ 0, 100000 };   // 100 ms so stop() is prompt
        const int ready = ::select(static_cast<int>(s) + 1, &set, nullptr, nullptr, &tv);
        if (ready <= 0) continue;
        sockaddr_in from{};
#if defined(_WIN32)
        int fromLen = sizeof(from);
#else
        socklen_t fromLen = sizeof(from);
#endif
        const int n = static_cast<int>(::recvfrom(s, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &fromLen));
        if (n <= 0) continue;
        const int count = parseOscPacket(buffer, static_cast<size_t>(n), [this](const OscMessage& m) {
            if (dispatchOsc(m, *sink_, *gestures_)) received_.fetch_add(1, std::memory_order_relaxed);
        });
        (void)count;
    }
}

} // namespace ambient
