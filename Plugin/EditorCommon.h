// AmbientSynth -- what the editor's four translation units share: the geometry of the grid, the
// palette shorthands, and the small drawing helpers the displays are built from. Internal to the
// plugin; nothing outside Plugin/ includes it.
#pragma once
#include "PluginEditor.h"
#include "AmbientLookAndFeel.h"
#include "ambient/Params.h"
#include "ambient/Help.h"
#include "ambient/Presets.h"
#include "ambient/PresetMeta.h"
#include "ambient/PresetMap.h"
#include "ambient/Route.h"
#include "ambient/Filter.h"
#include <cstdlib>

namespace edt {

// One cell is a knob and its name; the value is drawn inside the knob (see AmbientLookAndFeel),
// which is what buys the room for a knob this size in the same wall of controls.
constexpr int kCellW = 68, kCellH = 76, kPad = 11, kTitleH = 22, kGroupTitleH = 26, kHeaderH = 118;
// The editor is laid out once, in this design space, and the whole thing is then scaled to
// whatever size the window has. Dragging the corner is the zoom; the aspect ratio is fixed, so
// nothing ever reflows into a different arrangement -- it only gets bigger or smaller.
constexpr int kMinDesignW = 1400, kMinDesignH = 820;
// A tab bar above a row that pages, and the modulation strip along the bottom; the page height
// follows from these plus the body, so nothing on the main page ever has to scroll.
constexpr int kTabH = 26, kStripH = 300, kDisplayMinW = 300;
const juce::Colour kBg = ui::bg0, kGroupFill = ui::group, kSectionFill = ui::card, kAccent = ui::accent,
                   kText = ui::text, kDim = ui::dim;
const juce::Colour kVoice = ui::voiceCol, kFore = ui::foreCol, kBack = ui::backCol, kCosmos = ui::cosmosCol,
                   kConductor = ui::condCol, kMorph = ui::morphCol, kMaster = ui::masterCol;

// A parameter's raw value by key, for the displays (they read the host's atomics, never the engine's).
inline float rawParam(AmbientSynthProcessor& proc, const char* key)
{
    auto* v = proc.apvts.getRawParameterValue(key);
    return v != nullptr ? v->load() : 0.0f;
}

inline void displayFrame(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour c)
{
    g.setColour(ui::bg0.withAlpha(0.55f));
    g.fillRoundedRectangle(r.toFloat(), 6.0f);
    g.setColour(ui::cardEdge);
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 6.0f, 1.0f);
    g.setColour(c.withAlpha(0.8f));
    g.setFont(ui::title(10.5f));
    g.drawText(title, r.reduced(9, 5), juce::Justification::topLeft, false);
}

// Log-frequency axis with a few labelled lines, 20 Hz .. 20 kHz.
inline float xForHz(juce::Rectangle<float> plot, float hz)
{
    const float t = (std::log(juce::jmax(20.0f, hz)) - std::log(20.0f)) / (std::log(20000.0f) - std::log(20.0f));
    return plot.getX() + juce::jlimit(0.0f, 1.0f, t) * plot.getWidth();
}
inline float yForDb(juce::Rectangle<float> plot, float db)
{
    const float t = (db + 36.0f) / 48.0f;   // -36 .. +12 dB
    return plot.getBottom() - juce::jlimit(0.0f, 1.0f, t) * plot.getHeight();
}
inline void drawAxes(juce::Graphics& g, juce::Rectangle<float> plot)
{
    g.setColour(ui::track.withAlpha(0.55f));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        g.drawVerticalLine(juce::roundToInt(xForHz(plot, hz)), plot.getY(), plot.getBottom());
    g.setColour(ui::track.withAlpha(0.9f));
    g.drawHorizontalLine(juce::roundToInt(yForDb(plot, 0.0f)), plot.getX(), plot.getRight());
    g.setColour(ui::faint);
    g.setFont(ui::body(9.0f));
    for (float hz : { 100.0f, 1000.0f, 10000.0f })
        g.drawText(hz >= 1000.0f ? juce::String(hz / 1000.0f, 0) + "k" : juce::String(hz, 0),
                   juce::roundToInt(xForHz(plot, hz)) - 14, juce::roundToInt(plot.getBottom()) - 11, 28, 10,
                   juce::Justification::centred, false);
}

// The colour a modulation source is drawn in, everywhere it appears.
juce::Colour sourceColour(ambient::ModSource s);
// The colour a preset family is drawn in on the map and in the list.
juce::Colour familyColour(int f);

} // namespace edt

using namespace edt;
