#include "EditorCommon.h"

using namespace ambient;

// The displays that live inside the grid, each drawn from the numbers the engine is using.



void AmbientSynthEditor::BrainView::timerCallback()
{
    if (!isShowing()) return;
    bool now[128] = {};
    proc.engine().soundingNotes(now);
    auto& col = hist[static_cast<size_t>(head)];
    for (int n = 0; n < 128; ++n) {
        col[static_cast<size_t>(n)] = now[n];
        if (now[n]) { lo = juce::jmin(lo, n - 2); hi = juce::jmax(hi, n + 2); }
    }
    head = (head + 1) % kCols;
    repaint();
}

void AmbientSynthEditor::BrainView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(6.0f);
    g.setColour(ui::card);
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(ui::condCol.withAlpha(0.85f));
    g.setFont(ui::title(10.0f));
    g.drawText("NOTES", r.reduced(8.0f, 4.0f), juce::Justification::topLeft);
    const auto plot = r.reduced(8.0f).withTrimmedTop(16.0f);
    const int span = juce::jmax(12, hi - lo);
    const float rowH = plot.getHeight() / static_cast<float>(span);
    const float colW = plot.getWidth() / static_cast<float>(kCols);
    // octave lines, faint, so the register can be read
    g.setColour(ui::faint.withAlpha(0.5f));
    for (int n = (lo / 12 + 1) * 12; n < lo + span; n += 12) {
        const float y = plot.getBottom() - (n - lo) * rowH;
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
    }
    // the roll: newest column at the right; a held note becomes a bar
    for (int k = 0; k < kCols; ++k) {
        const auto& col = hist[static_cast<size_t>((head + k) % kCols)];
        const float x = plot.getX() + k * colW;
        const float age = static_cast<float>(k) / static_cast<float>(kCols);
        g.setColour(ui::condCol.withAlpha(0.25f + 0.7f * age));
        for (int n = juce::jmax(0, lo); n < juce::jmin(128, lo + span); ++n)
            if (col[static_cast<size_t>(n)])
                g.fillRect(x, plot.getBottom() - (n - lo + 1) * rowH + rowH * 0.15f, colW + 0.6f, rowH * 0.7f);
    }
    g.setColour(ui::dim);
    g.setFont(ui::body(9.5f));
    g.drawText(juce::MidiMessage::getMidiNoteName(lo, true, true, 4), plot.getX(), plot.getBottom() - 12.0f, 40.0f, 12.0f, juce::Justification::left);
    g.drawText(juce::MidiMessage::getMidiNoteName(lo + span, true, true, 4), plot.getX(), plot.getY(), 40.0f, 12.0f, juce::Justification::left);
}

// ---------------------------------------------------------------- stage

void AmbientSynthEditor::StageView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds();
    displayFrame(g, r, "STAGE", ui::voiceCol);
    const auto plot = r.toFloat().reduced(12.0f, 8.0f).withTrimmedTop(14.0f);
    // The room: near plane at the bottom, far at the top, faint depth lines between.
    g.setColour(ui::track.withAlpha(0.5f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine(juce::roundToInt(plot.getY() + plot.getHeight() * i / 4.0f), plot.getX(), plot.getRight());
    g.drawVerticalLine(juce::roundToInt(plot.getCentreX()), plot.getY(), plot.getBottom());
    g.setColour(ui::faint); g.setFont(ui::body(9.0f));
    g.drawText("far", plot.withHeight(10.0f), juce::Justification::topLeft, false);
    g.drawText("near", plot.withTrimmedTop(plot.getHeight() - 10.0f), juce::Justification::bottomLeft, false);
    g.drawText("L", plot.withTrimmedTop(plot.getHeight() - 10.0f).withTrimmedLeft(24.0f), juce::Justification::bottomLeft, false);
    g.drawText("R", plot.withTrimmedTop(plot.getHeight() - 10.0f), juce::Justification::bottomRight, false);
    // the listener
    g.setColour(ui::text.withAlpha(0.5f));
    g.fillEllipse(plot.getCentreX() - 3.0f, plot.getBottom() - 4.0f, 6.0f, 6.0f);

    ambient::Engine::VoiceStage vs[ambient::Engine::kMaxVoices];
    const int n = proc.engine().voiceStage(vs, ambient::Engine::kMaxVoices);
    // Dots glide towards their voices (the picture is about the slow breathing of the planes,
    // not the control blocks); a voice that has gone fades out.
    for (auto& d : dots) d.on = false;
    for (int i = 0; i < n; ++i) {
        const auto& v = vs[i];
        Dot* d = nullptr;
        for (auto& c : dots) if (c.note == v.note && !c.on) { d = &c; break; }
        if (d == nullptr) for (auto& c : dots) if (c.note < 0) { d = &c; d->x = v.pan; d->y = v.distance; d->r = 0.0f; break; }
        if (d == nullptr) continue;
        d->on = true; d->note = v.note;
        d->x += (v.pan - d->x) * 0.2f;
        d->y += (v.distance - d->y) * 0.2f;
        d->r += (juce::jlimit(0.0f, 1.0f, v.level) - d->r) * 0.3f;
        const float x = plot.getCentreX() + d->x * plot.getWidth() * 0.46f;
        const float y = plot.getBottom() - 8.0f - d->y * (plot.getHeight() - 16.0f);
        const float rad = 3.0f + 9.0f * d->r;
        const juce::Colour col = v.owner == 1 ? ui::voiceCol : ui::live;
        g.setColour(col.withAlpha(0.18f + 0.25f * d->r));
        g.fillEllipse(x - rad * 1.8f, y - rad * 1.8f, rad * 3.6f, rad * 3.6f);
        g.setColour(col.withAlpha(0.55f + 0.45f * (1.0f - d->y)));
        g.fillEllipse(x - rad, y - rad, 2.0f * rad, 2.0f * rad);
        g.setColour(ui::text.withAlpha(0.75f)); g.setFont(ui::body(9.0f));
        g.drawText(juce::MidiMessage::getMidiNoteName(v.note, true, true, 4), juce::roundToInt(x) - 16, juce::roundToInt(y - rad) - 12, 32, 11, juce::Justification::centred, false);
    }
    for (auto& d : dots) if (!d.on) d.note = -1;
    g.setColour(ui::dim); g.setFont(ui::body(10.0f));
    g.drawText(juce::String(n) + (n == 1 ? " voice" : " voices") + "   brain " + juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) + " keys " + juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")),
               r.reduced(9, 5), juce::Justification::topRight, false);
}

// ---------------------------------------------------------------- cosmos spectrum

AmbientSynthEditor::CosmosView::CosmosView(AmbientSynthProcessor& p)
    : proc(p), re(kN), im(kN), window(kN), fft(std::make_unique<ambient::Fft>(kN))
{
    setInterceptsMouseClicks(false, false);
    for (int i = 0; i < kN; ++i) window[static_cast<size_t>(i)] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / kN);
    for (float& b : bins) b = -90.0f;
    startTimerHz(15);
}

void AmbientSynthEditor::CosmosView::timerCallback()
{
    if (!isShowing()) return;
    proc.engine().cosmosTap(re.data(), kN);
    float sq = 0.0f;
    for (int i = 0; i < kN; ++i) { sq += re[static_cast<size_t>(i)] * re[static_cast<size_t>(i)]; re[static_cast<size_t>(i)] *= window[static_cast<size_t>(i)]; im[static_cast<size_t>(i)] = 0.0f; }
    silent = sq < 1.0e-9f;
    fft->transform(re.data(), im.data(), false);
    const float sr = static_cast<float>(proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
    // Log-spaced bins 30 Hz .. 16 kHz, each the peak of the FFT bins it covers, in dB; fast up,
    // slow down, like a meter.
    for (int b = 0; b < kBins; ++b) {
        const float f0 = 30.0f * std::pow(16000.0f / 30.0f, static_cast<float>(b) / kBins);
        const float f1 = 30.0f * std::pow(16000.0f / 30.0f, static_cast<float>(b + 1) / kBins);
        int k0 = juce::jmax(1, static_cast<int>(f0 / sr * kN)), k1 = juce::jmax(k0 + 1, static_cast<int>(f1 / sr * kN));
        float peak = 0.0f;
        for (int k = k0; k < juce::jmin(k1, kN / 2); ++k) peak = juce::jmax(peak, re[static_cast<size_t>(k)] * re[static_cast<size_t>(k)] + im[static_cast<size_t>(k)] * im[static_cast<size_t>(k)]);
        const float db = 10.0f * std::log10(peak / (kN * kN * 0.0625f) + 1.0e-12f);   // 0 dB = full-scale sine
        bins[b] = db > bins[b] ? db : bins[b] + (db - bins[b]) * 0.15f;
    }
    repaint();
}

void AmbientSynthEditor::CosmosView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds();
    displayFrame(g, r, "COSMOS RETURN", ui::cosmosCol);
    const auto plot = r.toFloat().reduced(10.0f, 8.0f).withTrimmedTop(12.0f);
    g.setColour(ui::track.withAlpha(0.5f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f }) {
        const float t = std::log(hz / 30.0f) / std::log(16000.0f / 30.0f);
        g.drawVerticalLine(juce::roundToInt(plot.getX() + t * plot.getWidth()), plot.getY(), plot.getBottom());
    }
    if (silent) {
        g.setColour(ui::faint); g.setFont(ui::body(11.0f));
        g.drawText(rawParam(proc, "cosmos_send") > 0.001f ? "cosmos: nothing sounding" : "cosmos: send is off", plot, juce::Justification::centred, false);
        return;
    }
    juce::Path fill, line;
    const float bw = plot.getWidth() / kBins;
    fill.startNewSubPath(plot.getX(), plot.getBottom());
    for (int b = 0; b < kBins; ++b) {
        const float t = juce::jlimit(0.0f, 1.0f, (bins[b] + 72.0f) / 72.0f);   // -72 .. 0 dB
        const float x = plot.getX() + (b + 0.5f) * bw, y = plot.getBottom() - t * plot.getHeight();
        fill.lineTo(x, y);
        if (b == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
    }
    fill.lineTo(plot.getRight(), plot.getBottom()); fill.closeSubPath();
    g.setColour(ui::cosmosCol.withAlpha(0.18f)); g.fillPath(fill);
    g.setColour(ui::cosmosCol.withAlpha(0.9f)); g.strokePath(line, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::dim); g.setFont(ui::body(10.0f));
    juce::String legend;
    const float shift = rawParam(proc, "cosmos_shift");
    if (std::fabs(shift) > 0.01f) legend += "shift " + juce::String(shift, 1) + " Hz   ";
    if (rawParam(proc, "cosmos_res") > 0.001f) legend += "resonator   ";
    if (rawParam(proc, "cosmos_smear") > 0.001f && rawParam(proc, "cosmos_nebula") > 0.001f) legend += "nebula   ";
    if (rawParam(proc, "cosmos_shimmer") > 0.001f) legend += "shimmer";
    g.drawText(legend.trim(), r.reduced(9, 5), juce::Justification::topRight, false);
}

// ---------------------------------------------------------------- amplitude envelope

void AmbientSynthEditor::EnvView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds();
    displayFrame(g, r, "AMP ENVELOPE", ui::voiceCol);
    const auto plot = r.toFloat().reduced(12.0f, 8.0f).withTrimmedTop(14.0f);
    const float a = rawParam(proc, "attack"), d = rawParam(proc, "decay"), su = rawParam(proc, "sustain"), rl = rawParam(proc, "release");
    // Time on a square-root axis so a 60 s attack and a 0.1 s one both read; the hold is a
    // fixed slice, since how long a key is down is not the envelope's business.
    const float hold = juce::jmax(1.0f, 0.25f * (a + d + rl));
    const float total = a + d + hold + rl;
    auto xAt = [&](float t) { return plot.getX() + std::sqrt(juce::jlimit(0.0f, 1.0f, t / total)) * plot.getWidth(); };
    auto yAt = [&](float v) { return plot.getBottom() - juce::jlimit(0.0f, 1.0f, v) * (plot.getHeight() - 4.0f); };
    juce::Path p;
    p.startNewSubPath(xAt(0.0f), yAt(0.0f));
    const int seg = 24;
    for (int i = 1; i <= seg; ++i) { const float t = i / static_cast<float>(seg); p.lineTo(xAt(a * t), yAt(1.0f - std::pow(1.0f - t, 2.2f))); }
    for (int i = 1; i <= seg; ++i) { const float t = i / static_cast<float>(seg); p.lineTo(xAt(a + d * t), yAt(su + (1.0f - su) * std::pow(1.0f - t, 2.2f))); }
    p.lineTo(xAt(a + d + hold), yAt(su));
    for (int i = 1; i <= seg; ++i) { const float t = i / static_cast<float>(seg); p.lineTo(xAt(a + d + hold + rl * t), yAt(su * std::pow(1.0f - t, 2.2f))); }
    g.setColour(ui::track.withAlpha(0.6f));
    for (float t : { a, a + d, a + d + hold }) g.drawVerticalLine(juce::roundToInt(xAt(t)), plot.getY(), plot.getBottom());
    g.setColour(ui::voiceCol.withAlpha(0.25f)); g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::voiceCol); g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // the loudest voice's level, live
    const float level = 0.5f * (proc.engine().modSource(static_cast<int>(ambient::ModSource::Amp)) + 1.0f);
    if (level > 0.001f) {
        g.setColour(ui::live.withAlpha(0.8f));
        g.drawHorizontalLine(juce::roundToInt(yAt(level)), plot.getX(), plot.getRight());
        g.fillEllipse(plot.getRight() - 5.0f, yAt(level) - 3.0f, 6.0f, 6.0f);
    }
    g.setColour(ui::faint); g.setFont(ui::body(9.0f));
    g.drawText("A " + juce::String(a, 1) + " s", juce::roundToInt(xAt(0.0f)), juce::roundToInt(plot.getBottom()) - 11, 60, 10, juce::Justification::left, false);
    g.drawText("D " + juce::String(d, 1), juce::roundToInt(xAt(a)) + 2, juce::roundToInt(plot.getBottom()) - 11, 50, 10, juce::Justification::left, false);
    g.drawText("S " + juce::String(su, 2), juce::roundToInt(xAt(a + d)) + 2, juce::roundToInt(plot.getBottom()) - 11, 50, 10, juce::Justification::left, false);
    g.drawText("R " + juce::String(rl, 1) + " s", juce::roundToInt(xAt(a + d + hold)) + 2, juce::roundToInt(plot.getBottom()) - 11, 60, 10, juce::Justification::left, false);
}

void AmbientSynthEditor::FilterView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds();
    displayFrame(g, r, "FILTER RESPONSE", ui::voiceCol);
    const auto plot = r.toFloat().reduced(10.0f, 8.0f).withTrimmedTop(12.0f);
    drawAxes(g, plot);

    const float sr = static_cast<float>(juce::jmax(8000.0, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0));
    FilterCurve fc;
    fc.capture(proc, sr);

    juce::Path svf, zp, both;
    const int steps = juce::jmax(64, static_cast<int>(plot.getWidth()));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float hz = 20.0f * std::pow(1000.0f, t);
        // Each branch on its own, and the two combined the way the voice combines the signals.
        // The parallel sum ignores the phase between them, which is the one thing this picture
        // cannot show.
        const float hs = fc.branchFilter(hz);
        const float hz_ = fc.branchZ(hz);
        const float combined = fc.magnitude(hz);
        const float x = plot.getX() + t * plot.getWidth();
        auto db = [](float m) { return 20.0f * std::log10(juce::jmax(m, 1.0e-6f)); };
        if (i == 0) { svf.startNewSubPath(x, yForDb(plot, db(hs))); zp.startNewSubPath(x, yForDb(plot, db(hz_))); both.startNewSubPath(x, yForDb(plot, db(combined))); }
        else        { svf.lineTo(x, yForDb(plot, db(hs)));           zp.lineTo(x, yForDb(plot, db(hz_)));           both.lineTo(x, yForDb(plot, db(combined))); }
    }
    if (fc.fOn) { g.setColour(ui::voiceCol.withAlpha(0.55f)); g.strokePath(svf, juce::PathStrokeType(1.2f)); }
    if (fc.zUsed > 0)  { g.setColour(ui::accent.withAlpha(0.55f));   g.strokePath(zp,  juce::PathStrokeType(1.2f)); }
    g.setColour(ui::text.withAlpha(0.25f));
    g.strokePath(both, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::text);
    g.strokePath(both, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(ui::dim);
    g.setFont(ui::body(10.0f));
    g.drawText(fc.legend(), r.reduced(9, 5), juce::Justification::topRight, false);
}

void AmbientSynthEditor::SourceView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds();
    const juce::String pre = "src" + juce::String(slot) + "_";
    const int type = static_cast<int>(std::lround(rawParam(proc, (pre + "type").toRawUTF8())));
    static const char* const kTitles[] = { "SOURCE OFF", "WAVETABLE", "FM PAIR", "TEXTURE GRAINS", "NOISE COLOUR", "ADDITIVE BANK" };
    displayFrame(g, r, kTitles[juce::jlimit(0, 5, type)], ui::voiceCol);
    const auto plot = r.toFloat().reduced(10.0f, 8.0f).withTrimmedTop(12.0f);
    const float cy = plot.getCentreY(), hh = plot.getHeight() * 0.42f;
    g.setColour(ui::track.withAlpha(0.6f));
    g.drawHorizontalLine(juce::roundToInt(cy), plot.getX(), plot.getRight());
    if (type == 0) {
        g.setColour(ui::faint); g.setFont(ui::body(11.0f));
        g.drawText("choose a type to the left", plot, juce::Justification::centred, false);
        return;
    }

    juce::Path p;
    const int steps = juce::jmax(64, static_cast<int>(plot.getWidth()));
    auto plotWave = [&](auto valueAt) {
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float y = cy - juce::jlimit(-1.0f, 1.0f, valueAt(t)) * hh;
            const float x = plot.getX() + t * plot.getWidth();
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
    };

    if (type == 1) {   // wavetable: one cycle of the frame at Position, resynthesised from its spectrum
        const int table = static_cast<int>(std::lround(rawParam(proc, (pre + "table").toRawUTF8())));
        const float pos = rawParam(proc, (pre + "pos").toRawUTF8());
        const ambient::Wavetable* wt = table >= ambient::kNumTables - 1 ? proc.engine().userWavetable()
                                                                        : &ambient::builtinTable(table);
        float spec[ambient::kTablePartials] = {};
        if (wt != nullptr && wt->frames > 0) wt->spectrumAt(pos, spec);
        float norm = 0.0f; for (float a : spec) norm += a;
        plotWave([&](float t) {
            float v = 0.0f;
            for (int h = 0; h < ambient::kTablePartials; ++h)
                if (spec[h] > 1.0e-5f) v += spec[h] * std::sin(juce::MathConstants<float>::twoPi * (h + 1) * t);
            return norm > 1.0e-6f ? v / norm * 1.4f : 0.0f;
        });
        g.setColour(ui::dim); g.setFont(ui::body(10.0f));
        g.drawText(juce::String(ambient::kTableNames[juce::jlimit(0, ambient::kNumTables - 1, table)]) + "  pos " + juce::String(pos, 2),
                   r.reduced(9, 5), juce::Justification::topRight, false);
    } else if (type == 2) {   // FM: carrier phase-modulated by the modulator at the ratio and index
        const float ratio = rawParam(proc, (pre + "fm_ratio").toRawUTF8()), idx = rawParam(proc, (pre + "fm_index").toRawUTF8());
        plotWave([&](float t) {
            const float ph = juce::MathConstants<float>::twoPi * t;
            return std::sin(ph + idx * std::sin(ph * ratio));
        });
        g.setColour(ui::dim); g.setFont(ui::body(10.0f));
        g.drawText("ratio " + juce::String(ratio, 2) + "   index " + juce::String(idx, 2), r.reduced(9, 5), juce::Justification::topRight, false);
    } else if (type == 3) {   // texture: the clip's envelope, and the window the grains are drawn from
        const ambient::Texture* tex = proc.engine().displayTexture();
        if (tex == nullptr || tex->empty()) {
            g.setColour(ui::faint); g.setFont(ui::body(11.0f));
            g.drawText("no clip loaded -- Texture... below, or a pack preset", plot, juce::Justification::centred, false);
            return;
        }
        const int n = static_cast<int>(tex->mono.size());
        const int cols = juce::jmax(32, static_cast<int>(plot.getWidth()));
        g.setColour(ui::voiceCol.withAlpha(0.55f));
        for (int c = 0; c < cols; ++c) {
            const int a = static_cast<int>(static_cast<long long>(c) * n / cols), b = juce::jmax(a + 1, static_cast<int>(static_cast<long long>(c + 1) * n / cols));
            float peak = 0.0f;
            const int stride = juce::jmax(1, (b - a) / 64);
            for (int i = a; i < b; i += stride) peak = juce::jmax(peak, std::fabs(tex->mono[static_cast<size_t>(i)]));
            const float x = plot.getX() + c * plot.getWidth() / cols;
            g.drawVerticalLine(juce::roundToInt(x), cy - peak * hh * 2.0f, cy + peak * hh * 2.0f);
        }
        const float pos = rawParam(proc, (pre + "pos").toRawUTF8()), spread = rawParam(proc, (pre + "spread").toRawUTF8());
        const float x0 = plot.getX() + juce::jlimit(0.0f, 1.0f, pos - spread) * plot.getWidth();
        const float x1 = plot.getX() + juce::jlimit(0.0f, 1.0f, pos + spread) * plot.getWidth();
        g.setColour(ui::live.withAlpha(0.18f));
        g.fillRect(x0, plot.getY(), juce::jmax(2.0f, x1 - x0), plot.getHeight());
        g.setColour(ui::live);
        g.drawVerticalLine(juce::roundToInt(plot.getX() + pos * plot.getWidth()), plot.getY(), plot.getBottom());
        // The grains themselves: each a window over the clip where it is reading right now, wide
        // as its length, fading as it ages -- the loudest voice's slot, live.
        ambient::SourceSlot::GrainInfo gi[ambient::kSlotGrains];
        const int gn = proc.engine().displayGrains(slot - 1, gi, ambient::kSlotGrains);
        const float grainMs = rawParam(proc, (pre + "grain").toRawUTF8());
        const float clipSec = static_cast<float>(tex->mono.size() / juce::jmax(1.0, tex->sampleRate));
        const float gw = juce::jmax(3.0f, grainMs * 0.001f / juce::jmax(0.05f, clipSec) * plot.getWidth());
        for (int i = 0; i < gn; ++i) {
            const float gx = plot.getX() + juce::jlimit(0.0f, 1.0f, gi[i].pos) * plot.getWidth();
            const float w = std::sin(juce::MathConstants<float>::pi * juce::jlimit(0.0f, 1.0f, gi[i].age));   // the Hann window's height now
            const float gh = (0.25f + 0.75f * w) * plot.getHeight() * 0.5f;
            const float gy = cy - gh * 0.5f + gi[i].pan * plot.getHeight() * 0.18f;
            g.setColour(ui::accent.withAlpha(0.10f + 0.45f * w));
            g.fillRoundedRectangle(gx - gw * 0.5f, gy, gw, gh, 2.0f);
        }
        g.setColour(ui::dim); g.setFont(ui::body(10.0f));
        g.drawText(juce::String(gn) + " grains", r.reduced(9, 5).withTrimmedTop(12), juce::Justification::topRight, false);
        g.setColour(ui::dim); g.setFont(ui::body(10.0f));
        g.drawText(juce::String(tex->mono.size() / juce::jmax(1.0, tex->sampleRate), 1) + " s   base " + juce::String(tex->baseHz, 1) + " Hz",
                   r.reduced(9, 5), juce::Justification::topRight, false);
        return;
    } else if (type == 5) {   // additive: the bank's partials as they are being summed, and the cycle they make
        float live[ambient::kTablePartials] = {};
        const int n = slot == 1 ? proc.engine().displayPartials(live, ambient::kTablePartials)
                                : proc.engine().displaySlotPartials(slot - 1, live, ambient::kTablePartials);
        float peak = 1.0e-6f;
        for (int i = 0; i < ambient::kTablePartials; ++i) {
            amp[i] += ((i < n ? std::abs(live[i]) : 0.0f) - amp[i]) * 0.25f;
            peak = juce::jmax(peak, amp[i]);
        }
        if (peak < 1.0e-4f) {
            g.setColour(ui::faint); g.setFont(ui::body(11.0f));
            g.drawText("additive bank -- nothing sounding", plot, juce::Justification::centred, false);
            return;
        }
        const float bw = juce::jmin(9.0f, plot.getWidth() / static_cast<float>(ambient::kTablePartials));
        for (int i = 0; i < ambient::kTablePartials; ++i) {
            const float a = amp[i] / peak;
            if (a < 1.0e-4f) continue;
            const float db = juce::jlimit(0.0f, 1.0f, 1.0f + std::log10(a) / 2.5f);
            const float h = db * plot.getHeight() * 0.9f;
            g.setColour(ui::accent.withAlpha(0.10f + 0.13f * db));
            g.fillRect(plot.getX() + i * bw + 0.5f, plot.getBottom() - h, juce::jmax(1.0f, bw - 1.2f), h);
        }
        plotWave([&](float t) {
            float v = 0.0f;
            for (int i = 0; i < ambient::kTablePartials; ++i)
                if (amp[i] > 1.0e-5f) v += amp[i] * std::sin(juce::MathConstants<float>::twoPi * ((i + 1) * t + 0.381966f * i));
            return v / (peak * 2.2f);
        });
        g.setColour(ui::dim); g.setFont(ui::body(10.0f));
        g.drawText(juce::String(n) + " partials" + (slot == 1 ? "   " + juce::String(rawParam(proc, "strands"), 0) + " strands" : juce::String()),
                   r.reduced(9, 5), juce::Justification::topRight, false);
    } else {   // noise: the colour as a spectral slope, plus the band centre for Band and Wind
        const int kind = static_cast<int>(std::lround(rawParam(proc, (pre + "noise").toRawUTF8())));
        static const float kSlope[] = { 0.0f, -3.0f, -6.0f, 3.0f, 6.0f, 0.0f, 0.0f, 0.0f, -2.0f, -6.0f };
        const float slope = kSlope[juce::jlimit(0, 9, kind)];
        const auto sp = plot;
        g.setColour(ui::track.withAlpha(0.5f));
        for (float hz : { 100.0f, 1000.0f, 10000.0f }) g.drawVerticalLine(juce::roundToInt(xForHz(sp, hz)), sp.getY(), sp.getBottom());
        juce::Path line;
        const bool banded = (kind == 6 || kind == 7);
        const float posN = rawParam(proc, (pre + "pos").toRawUTF8());
        const float centre = 40.0f * std::pow(300.0f, posN);
        const float q = rawParam(proc, (pre + "noise_q").toRawUTF8());
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float hz = 20.0f * std::pow(1000.0f, t);
            float db = slope * std::log2(hz / 1000.0f);
            if (kind == 5) db -= 6.0f * std::exp(-std::pow(std::log2(hz / 3000.0f), 2.0f) / 0.9f);   // grey: the dip at 3 kHz
            if (banded) {
                const float width = 0.15f + 2.2f * (1.0f - q);
                db = -30.0f + 30.0f * std::exp(-std::pow(std::log2(hz / centre) / width, 2.0f));
            }
            const float x = sp.getX() + t * sp.getWidth();
            const float y = yForDb(sp, juce::jlimit(-36.0f, 12.0f, db - 6.0f));
            if (i == 0) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
        g.setColour(ui::voiceCol.withAlpha(0.25f)); g.strokePath(line, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(ui::voiceCol);                  g.strokePath(line, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(ui::dim); g.setFont(ui::body(10.0f));
        g.drawText(juce::String(ambient::kNoiseKindNames[juce::jlimit(0, ambient::kNumNoiseKinds - 1, kind)])
                       + (banded ? "   " + juce::String(centre, 0) + " Hz" : juce::String()),
                   r.reduced(9, 5), juce::Justification::topRight, false);
        return;
    }
    g.setColour(ui::voiceCol.withAlpha(0.25f)); g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::voiceCol);                  g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ---------------------------------------------------------------- the output's spectrum

// ---------------------------------------------------------------- the Vector's square

juce::Rectangle<float> AmbientSynthEditor::VectorView::square() const
{
    const auto plot = getLocalBounds().toFloat().reduced(12.0f, 9.0f).withTrimmedTop(15.0f);
    const float side = juce::jmin(plot.getWidth(), plot.getHeight());
    return juce::Rectangle<float>(plot.getCentreX() - side * 0.5f, plot.getCentreY() - side * 0.5f, side, side);
}

void AmbientSynthEditor::VectorView::drag(const juce::MouseEvent& e)
{
    const auto sq = square();
    if (sq.getWidth() < 4.0f) return;
    const float x = juce::jlimit(0.0f, 1.0f, (e.position.x - sq.getX()) / sq.getWidth());
    const float y = juce::jlimit(0.0f, 1.0f, 1.0f - (e.position.y - sq.getY()) / sq.getHeight());
    if (auto* px = proc.apvts.getParameter(paramDesc(ambient::ParamId::VecX).key)) px->setValueNotifyingHost(x);
    if (auto* py = proc.apvts.getParameter(paramDesc(ambient::ParamId::VecY).key)) py->setValueNotifyingHost(y);
}

void AmbientSynthEditor::VectorView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds();
    displayFrame(g, r, "VECTOR", ui::voiceCol);
    const auto sq = square();
    if (sq.getWidth() < 20.0f) return;

    const float amount = rawParam(proc, "vec_amount");
    const float vx = rawParam(proc, "vec_x"), vy = rawParam(proc, "vec_y");
    auto toXY = [&](float x, float y) {
        return juce::Point<float>(sq.getX() + x * sq.getWidth(), sq.getBottom() - y * sq.getHeight());
    };

    g.setColour(ui::card);
    g.fillRoundedRectangle(sq, 4.0f);
    g.setColour(ui::track.withAlpha(0.5f));
    for (int i = 1; i < 4; ++i) {
        g.drawHorizontalLine(juce::roundToInt(sq.getY() + sq.getHeight() * i / 4.0f), sq.getX(), sq.getRight());
        g.drawVerticalLine(juce::roundToInt(sq.getX() + sq.getWidth() * i / 4.0f), sq.getY(), sq.getBottom());
    }
    g.setColour(ui::faint);
    g.drawRoundedRectangle(sq, 4.0f, 1.0f);

    // The corners, named for what is at them.
    g.setFont(ui::body(9.0f));
    g.setColour(ui::dim);
    g.drawText("SRC 1", sq.getX() + 3.0f, sq.getBottom() - 12.0f, 44.0f, 11.0f, juce::Justification::left, false);
    g.drawText("SRC 2", sq.getRight() - 47.0f, sq.getBottom() - 12.0f, 44.0f, 11.0f, juce::Justification::right, false);
    g.drawText("SRC 3", sq.getX() + 3.0f, sq.getY() + 2.0f, 44.0f, 11.0f, juce::Justification::left, false);
    g.drawText("ALL", sq.getRight() - 47.0f, sq.getY() + 2.0f, 44.0f, 11.0f, juce::Justification::right, false);

    // Where the wander has actually been. The point on the panel is where the knobs are; the trail
    // is where the engine's own drift has taken it, which is the part a knob cannot show.
    const bool live = amount > 0.0005f;
    const float wander = rawParam(proc, "vec_wander");
    if (live && wander > 0.0005f) {
        tx[head] = vx; ty[head] = vy;                 // the drift itself is inside the engine; this
        head = (head + 1) % kTrail;                    // records where the setting has been
        if (filled < kTrail) ++filled;
        juce::Path trail;
        for (int k = 0; k < filled; ++k) {
            const auto p = toXY(tx[(head - filled + k + kTrail) % kTrail], ty[(head - filled + k + kTrail) % kTrail]);
            if (k == 0) trail.startNewSubPath(p); else trail.lineTo(p);
        }
        g.setColour(ui::accent.withAlpha(0.25f));
        g.strokePath(trail, juce::PathStrokeType(1.0f));
    }

    // The three slot weights, as a bar in each corner: what the point is actually doing.
    const float w00 = (1.0f - vx) * (1.0f - vy), w10 = vx * (1.0f - vy);
    const float w01 = (1.0f - vx) * vy, w11 = vx * vy;
    const float f[3] = { w00 + w11 / 3.0f, w10 + w11 / 3.0f, w01 + w11 / 3.0f };
    const juce::Point<float> corner[3] = { toXY(0.0f, 0.0f), toXY(1.0f, 0.0f), toXY(0.0f, 1.0f) };
    for (int k = 0; k < 3; ++k) {
        const float t = juce::jlimit(0.0f, 1.0f, 3.0f * f[k] / 3.0f);
        g.setColour(ui::accent.withAlpha(0.15f + 0.5f * t));
        g.fillEllipse(corner[k].x - 4.0f - 10.0f * t, corner[k].y - 4.0f - 10.0f * t,
                      8.0f + 20.0f * t, 8.0f + 20.0f * t);
    }

    const auto p = toXY(vx, vy);
    g.setColour(live ? ui::live : ui::dim.withAlpha(0.5f));
    g.fillEllipse(p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
    g.setColour(ui::text.withAlpha(live ? 0.9f : 0.4f));
    g.drawEllipse(p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f, 1.2f);

    // The three weights as bars beside the square: the picture says where the point is, these say
    // what that does to each slot, which is the question the ear is actually asking.
    const float barX = sq.getRight() + 18.0f;
    const float barW = r.getRight() - 14.0f - barX;
    if (barW > 90.0f) {
        static const char* const kNames[3] = { "SOURCE 1", "SOURCE 2", "SOURCE 3" };
        const float rowH = 20.0f;
        float by = sq.getCentreY() - 1.5f * rowH;
        g.setFont(ui::body(9.5f));
        for (int k = 0; k < 3; ++k, by += rowH) {
            g.setColour(ui::dim);
            g.drawText(kNames[k], barX, by, 58.0f, 12.0f, juce::Justification::left, false);
            const float x0 = barX + 62.0f, w = barW - 62.0f - 34.0f;
            g.setColour(ui::track.withAlpha(0.5f));
            g.fillRoundedRectangle(x0, by + 2.0f, w, 8.0f, 3.0f);
            // The factor the engine applies, which is what the level is multiplied by.
            const float factor = live ? 1.0f + amount * (3.0f * f[k] - 1.0f) : 1.0f;
            g.setColour(ui::accent.withAlpha(0.35f + 0.5f * juce::jlimit(0.0f, 1.0f, factor / 3.0f)));
            g.fillRoundedRectangle(x0, by + 2.0f, juce::jmax(2.0f, w * juce::jlimit(0.0f, 1.0f, factor / 3.0f)), 8.0f, 3.0f);
            g.setColour(ui::text.withAlpha(0.75f));
            g.drawText(juce::String(factor, 2) + "x", x0 + w + 4.0f, by, 30.0f, 12.0f, juce::Justification::left, false);
        }
        g.setColour(ui::faint);
        g.drawText("the factor on each slot's own level", barX, sq.getCentreY() + 1.9f * rowH, barW, 12.0f,
                   juce::Justification::left, false);
    }

    g.setColour(ui::dim);
    g.setFont(ui::body(9.5f));
    g.drawText(live ? juce::String("drag the point") : juce::String("Amount is 0: the slots play at their own levels"),
               r.reduced(9, 5), juce::Justification::topRight, false);
}

// ---------------------------------------------------------------- loudness, in the header

void AmbientSynthEditor::LoudnessView::mouseDown(const juce::MouseEvent&)
{
    proc.engine().resetLoudness();
    repaint();
}

void AmbientSynthEditor::LoudnessView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(ui::bg0.withAlpha(0.5f));
    g.fillRoundedRectangle(r, 4.0f);
    const ambient::LoudnessReading ld = proc.engine().loudness();
    const auto plot = r.reduced(6.0f, 3.0f);

    // A bar for the short-term value, over the window an ambient master lives in: -30 to -6 LUFS,
    // with the -18 to -24 band the literature asks for marked out. The number matters more than
    // the bar, so the bar is thin and the type is not.
    const float lo = -30.0f, hi = -6.0f;
    auto xOf = [&](float lufs) { return plot.getX() + plot.getWidth() * 0.42f * juce::jlimit(0.0f, 1.0f, (lufs - lo) / (hi - lo)); };
    const float y = plot.getCentreY();
    g.setColour(ui::track.withAlpha(0.5f));
    g.fillRoundedRectangle(plot.getX(), y - 3.0f, plot.getWidth() * 0.42f, 6.0f, 2.0f);
    g.setColour(ui::accent.withAlpha(0.20f));
    g.fillRoundedRectangle(xOf(-24.0f), y - 3.0f, xOf(-18.0f) - xOf(-24.0f), 6.0f, 2.0f);
    if (ld.shortTerm > -119.0f) {
        g.setColour(ld.shortTerm > -14.0f ? juce::Colour(0xffe0a060) : ui::live);
        g.fillRoundedRectangle(plot.getX(), y - 3.0f, juce::jmax(2.0f, xOf(ld.shortTerm) - plot.getX()), 6.0f, 2.0f);
    }
    if (ld.integrated > -119.0f) {   // the integrated value as a mark on the same scale
        g.setColour(ui::text.withAlpha(0.8f));
        g.drawVerticalLine(juce::roundToInt(xOf(ld.integrated)), y - 6.0f, y + 6.0f);
    }

    g.setFont(ui::body(9.5f));
    auto num = [](float v) { return v > -119.0f ? juce::String(v, 1) : juce::String("--"); };
    const juce::String text = "I " + num(ld.integrated) + "   S " + num(ld.shortTerm)
                            + "   LRA " + juce::String(ld.range, 1)
                            + "   TP " + num(ld.truePeak)
                            + "   crest " + juce::String(ld.crest, 1);
    // Red when the true peak is over the -1 dBTP a lossy codec needs as headroom, amber when the
    // crest factor has fallen under the 14 dB that says the dynamics are still there.
    const bool tpHot = ld.truePeak > -1.0f;
    g.setColour(tpHot ? juce::Colour(0xffe06060)
                      : (ld.crest > 0.0f && ld.crest < 14.0f ? juce::Colour(0xffe0a060) : ui::dim));
    g.drawText(text, plot.withTrimmedLeft(plot.getWidth() * 0.44f), juce::Justification::centredLeft, false);
}
