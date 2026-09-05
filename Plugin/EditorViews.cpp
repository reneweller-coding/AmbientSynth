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
    const float cutoff = rawParam(proc, "cutoff"), res = rawParam(proc, "resonance");
    const int model = juce::jlimit(0, ambient::kNumFilterModels - 1, static_cast<int>(std::lround(rawParam(proc, "filter_model"))));
    const int zMode = static_cast<int>(std::lround(rawParam(proc, "z_mode")));
    const int zShape = juce::jlimit(0, ambient::kZShapes - 1, static_cast<int>(std::lround(rawParam(proc, "z_shape"))));
    const float zMix = rawParam(proc, "z_mix");
    const bool fOn = rawParam(proc, "filter_on") >= 0.5f && zMode != 2;
    const bool zOn = zMode != 0;
    const bool parallel = std::lround(rawParam(proc, "z_route")) == 1;

    // The z-plane cascade, from the frame the engine would build at this point.
    ambient::ZBiquad zb[ambient::kZSections];
    float zNorm = 1.0f; int zUsed = 0;
    if (zMode != 0) {
        const ambient::ZFrame frame = ambient::zInterpolate(zShape, rawParam(proc, "z_x"), rawParam(proc, "z_y"));
        zNorm = ambient::zBuildCascade(frame, zb, sr);
        zUsed = frame.used;
    }

    juce::Path svf, zp, both;
    const int steps = juce::jmax(64, static_cast<int>(plot.getWidth()));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float hz = 20.0f * std::pow(1000.0f, t);
        const float w = juce::MathConstants<float>::twoPi * hz / sr;
        // the chosen model's own response, from the same maths the voice uses
        const float hs = fOn ? ambient::VoiceFilter::magnitude(static_cast<ambient::FilterModel>(model), cutoff, res, hz, sr) : 1.0f;
        float hz_ = 1.0f;
        if (zUsed > 0) { hz_ = zNorm; for (int s = 0; s < zUsed; ++s) hz_ *= zb[s].magnitudeAt(w); }
        // Magnitudes combine as the voice combines the signals; the parallel sum ignores the phase
        // between the two branches, which is the one thing this picture cannot show.
        float combined = hs;
        if (fOn && zOn) combined = parallel ? (1.0f - zMix) * hs + zMix * hz_ : hs * ((1.0f - zMix) + zMix * hz_);
        else if (zOn) combined = (1.0f - zMix) + zMix * hz_;
        const float x = plot.getX() + t * plot.getWidth();
        auto db = [](float m) { return 20.0f * std::log10(juce::jmax(m, 1.0e-6f)); };
        if (i == 0) { svf.startNewSubPath(x, yForDb(plot, db(hs))); zp.startNewSubPath(x, yForDb(plot, db(hz_))); both.startNewSubPath(x, yForDb(plot, db(combined))); }
        else        { svf.lineTo(x, yForDb(plot, db(hs)));           zp.lineTo(x, yForDb(plot, db(hz_)));           both.lineTo(x, yForDb(plot, db(combined))); }
    }
    if (fOn) { g.setColour(ui::voiceCol.withAlpha(0.55f)); g.strokePath(svf, juce::PathStrokeType(1.2f)); }
    if (zUsed > 0)  { g.setColour(ui::accent.withAlpha(0.55f));   g.strokePath(zp,  juce::PathStrokeType(1.2f)); }
    g.setColour(ui::text.withAlpha(0.25f));
    g.strokePath(both, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::text);
    g.strokePath(both, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(ui::dim);
    g.setFont(ui::body(10.0f));
    juce::String legend = fOn ? juce::String(ambient::kFilterModelNames[model]) + "   " + juce::String(cutoff, 0) + " Hz" : juce::String("filter off");
    if (zOn) legend += "   z: " + juce::String(ambient::kZShapeNames[zShape]) + (!fOn ? "  (alone)" : parallel ? "  (parallel)" : "  (series)");
    g.drawText(legend, r.reduced(9, 5), juce::Justification::topRight, false);
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
