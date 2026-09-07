#include "EditorCommon.h"

using namespace ambient;

// The browse page (columns, list, map) and the perform page.

// ---------------------------------------------------------------- browse page

namespace {
const juce::Colour kFamilyColours[] = {
    juce::Colour(0xff7fb3d5), juce::Colour(0xff8ec9a8), juce::Colour(0xffe0c070), juce::Colour(0xffd08a8a),
    juce::Colour(0xffb094d8), juce::Colour(0xff70c8c8), juce::Colour(0xffe09a60), juce::Colour(0xffa0b8e0),
    juce::Colour(0xffc8d870), juce::Colour(0xffd880b8), juce::Colour(0xff90d0f0), juce::Colour(0xffd0d0d0),
};
// The twelve built-in families keep their colours; loaded packs get their own hues, spaced by
// the golden angle so neighbouring packs never look alike.
}
namespace edt {
juce::Colour familyColour(int f)
{
    constexpr int kBuiltIn = static_cast<int>(sizeof(kFamilyColours) / sizeof(kFamilyColours[0]));
    if (f < 0) f = 0;
    if (f < kBuiltIn) return kFamilyColours[static_cast<size_t>(f)];
    const float hue = std::fmod(0.08f + 0.6180339f * static_cast<float>(f - kBuiltIn + 1), 1.0f);
    return juce::Colour::fromHSV(hue, 0.42f, 0.82f, 1.0f);
}
}

AmbientSynthEditor::BrowseView::BrowseView(AmbientSynthProcessor& p) : proc(p), map(*this)
{
    search.setTextToShowWhenEmpty("search", kDim);
    search.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible(search);
    family.addItem("all families", 1);
    for (int f = 0; f < numPresetFamilies(); ++f) family.addItem(presetFamilyName(f), f + 2);
    family.setSelectedId(1, juce::dontSendNotification);
    family.onChange = [this] { applyFilter(); };
    addAndMakeVisible(family);
    sort.addItem("by number", 1); sort.addItem("by name", 2); sort.addItem("dark -> bright", 3); sort.addItem("calm -> moving", 4);
    sort.addItem("narrow -> wide", 5); sort.addItem("tonal -> noisy", 6); sort.addItem("sparse -> dense", 7);
    sort.setSelectedId(1, juce::dontSendNotification);
    sort.onChange = [this] { applyFilter(); };
    addAndMakeVisible(sort);
    for (int t = 0; t < kNumPresetTags; ++t) {
        auto b = std::make_unique<juce::ToggleButton>(presetTagName(t));
        b->onClick = [this] { applyFilter(); };
        addAndMakeVisible(*b);
        tagButtons.push_back(std::move(b));
    }
    list.setModel(this);
    list.setRowHeight(22);
    list.setColour(juce::ListBox::backgroundColourId, kBg);
    addAndMakeVisible(list);
    addAndMakeVisible(map);
    mapActive.setButtonText("Map blend (drag the cursor)");
    addAndMakeVisible(mapActive);
    mapActiveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, paramDesc(ParamId::MapActive).key, mapActive);
    radius.setSliderStyle(juce::Slider::LinearHorizontal);
    radius.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 18);
    addAndMakeVisible(radius);
    radiusAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, paramDesc(ParamId::MapRadius).key, radius);
    info.setFont(juce::FontOptions(12.0f)); info.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(info);
    load.onClick = [this] { if (selected >= 0) proc.setCurrentProgram(selected); };
    toA.onClick = [this] { if (selected >= 0) proc.setMorphSlotFromPreset(0, selected); };
    toB.onClick = [this] { if (selected >= 0) proc.setMorphSlotFromPreset(1, selected); };
    star.onClick = [this] { if (selected >= 0) { proc.setFavourite(selected, !proc.isFavourite(selected)); applyFilter(); } };
    onlyFavourites.onClick = [this] { applyFilter(); };
    hideDull.onClick = [this] { applyFilter(); };
    hideDull.setTooltip("Hides presets that measured as barely moving and barely wide -- the dull tail of a generated library");
    for (auto* b : { &load, &toA, &toB, &star }) addAndMakeVisible(*b);
    // The map already knows which presets are alike: its neighbours are the ones that measured
    // alike. This narrows the list to the selected preset's six nearest and zooms the map onto
    // them; off again, the list is what the other filters make it.
    similar.onClick = [this] {
        similarSet.clear();
        similarOf = -1;
        if (similar.getToggleState() && selected >= 0 && selected < numPresetMeta()) {
            const PresetMeta& m = presetMeta(selected);
            const PresetMap::Blend b = PresetMap::neighbours(m.x, m.y, 0.12f);
            similarSet.push_back(selected);
            for (int k = 0; k < b.count; ++k) if (b.index[k] != selected) similarSet.push_back(b.index[k]);
            similarOf = selected;
            map.zoomTo(m.x, m.y, 8.0f);
        } else if (!similar.getToggleState()) {
            map.zoomTo(0.5f, 0.5f, 1.0f);
        }
        applyFilter();
    };
    similar.setTooltip("Narrow the list to the presets that measured most like the selected one, and zoom the map onto them");
    addAndMakeVisible(similar);
    addAndMakeVisible(onlyFavourites);
    addAndMakeVisible(hideDull);

    // Classic columns: Family | Character | Motion & density | Features. A click narrows,
    // several rows in one column combine with OR, columns combine with AND, "All" clears.
    struct Def { const char* title; std::vector<int> tags; bool families; };
    const Def defs[4] = {
        { "Family", {}, true },
        { "Character", { 0, 1, 4, 5, 6, 7 }, false },            // Dark Bright Tonal Noisy Wide Bass
        { "Motion", { 2, 3, 8, 9 }, false },                      // Calm Moving Dense Sparse
        { "Features", { 10, 11, 12, 13, 14, 15, 16, 17, 18 }, false },   // Keys Generative Cosmos Feedback Sources JI Sub Stack Air
    };
    for (int c = 0; c < 4; ++c) {
        Column& col = columns[c];
        col.owner = this; col.title = defs[c].title;
        col.items.add("All"); col.tagBits.push_back(0); col.familyIdx.push_back(-1);
        if (defs[c].families) {
            for (int f = 0; f < numPresetFamilies(); ++f) { col.items.add(presetFamilyName(f)); col.tagBits.push_back(0); col.familyIdx.push_back(f); }
        } else {
            for (int t : defs[c].tags) { col.items.add(presetTagName(t)); col.tagBits.push_back(1u << t); col.familyIdx.push_back(-1); }
        }
        col.box.setModel(&col);
        col.box.setRowHeight(20);
        col.box.setColour(juce::ListBox::backgroundColourId, kBg);
        addAndMakeVisible(col.box);
    }
    // Route strip
    routeBox.setTextWhenNothingSelected("route preset");
    for (int r = 0; r < numRoutePresets(); ++r) routeBox.addItem(routePreset(r).name, r + 1);
    routeBox.onChange = [this] {
        const int r = routeBox.getSelectedId() - 1;
        if (r >= 0 && r < numRoutePresets()) { proc.setRouteText(routePreset(r).points); map.repaint(); }
    };
    addAndMakeVisible(routeBox);
    routePlay.setButtonText("Play route"); addAndMakeVisible(routePlay);
    routePlayAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, paramDesc(ParamId::RouteActive).key, routePlay);
    routeLoop.setButtonText("loop"); addAndMakeVisible(routeLoop);
    routeLoopAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, paramDesc(ParamId::RouteLoop).key, routeLoop);
    routeSpeed.setSliderStyle(juce::Slider::LinearHorizontal); routeSpeed.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18); routeSpeed.setTextValueSuffix("x");
    addAndMakeVisible(routeSpeed);
    routeSpeedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, paramDesc(ParamId::RouteSpeed).key, routeSpeed);
    routeAdd.setTooltip("Append the current cursor (position and radius) as a waypoint: 60 s travel, 60 s hold");
    routeAdd.onClick = [this] {
        Waypoint w; w.x = proc.engine().getParam(ParamId::MapX); w.y = proc.engine().getParam(ParamId::MapY); w.radius = proc.engine().getParam(ParamId::MapRadius);
        w.travel = 60.0f; w.hold = 60.0f;
        const int near = map.nearestPreset(map.toScreen(w.x, w.y), 8.0f);
        if (near >= 0) { w.preset = near; w.x = presetMeta(near).x; w.y = presetMeta(near).y; }
        proc.addRoutePoint(w); routeBox.setSelectedId(0, juce::dontSendNotification); map.repaint();
    };
    routeClear.onClick = [this] { proc.clearRoute(); routeBox.setSelectedId(0, juce::dontSendNotification); map.repaint(); };
    routeEdit.setTooltip("Edit the route as text: preset|travel|hold[|radius] or x,y|travel|hold[|radius], separated by ;");
    routeEdit.onClick = [this] { showRouteEditor(); };
    for (auto* b : { &routeAdd, &routeClear, &routeEdit }) addAndMakeVisible(*b);

    modeClassic.setClickingTogglesState(true); modeMap.setClickingTogglesState(true);
    modeClassic.setRadioGroupId(77); modeMap.setRadioGroupId(77);
    modeClassic.setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
    modeMap.setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
    modeClassic.onClick = [this] { setMode(0); };
    modeMap.onClick = [this] { setMode(1); };
    addAndMakeVisible(modeClassic); addAndMakeVisible(modeMap);
    setMode(0);
    applyFilter();
    startTimerHz(15);
}

void AmbientSynthEditor::BrowseView::setMode(int m)
{
    mode = m;
    modeClassic.setToggleState(m == 0, juce::dontSendNotification);
    modeMap.setToggleState(m == 1, juce::dontSendNotification);
    for (auto& c : columns) c.box.setVisible(m == 0);
    for (auto& b : tagButtons) b->setVisible(m == 1);
    family.setVisible(m == 1);
    map.setVisible(m == 1); mapActive.setVisible(m == 1); radius.setVisible(m == 1);
    for (juce::Component* c : { static_cast<juce::Component*>(&routeBox), static_cast<juce::Component*>(&routePlay), static_cast<juce::Component*>(&routeLoop),
                                static_cast<juce::Component*>(&routeSpeed), static_cast<juce::Component*>(&routeAdd), static_cast<juce::Component*>(&routeClear), static_cast<juce::Component*>(&routeEdit) })
        c->setVisible(m == 1);
    resized();
    applyFilter();
}

void AmbientSynthEditor::BrowseView::Column::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool)
{
    const bool on = (row == 0) ? chosen.empty() : chosen.count(row) > 0;
    if (on) g.fillAll(kAccent.withAlpha(0.18f));
    g.setColour(on ? kText : kDim); g.setFont(juce::FontOptions(12.5f));
    g.drawText(items[row], 8, 0, w - 40, h, juce::Justification::centredLeft);
    // count of presets this row would leave -- cached, because counting five thousand presets
    // per painted row per repaint is not something a list box should be doing
    if (row > 0 && owner != nullptr) {
        updateCounts();
        if (static_cast<size_t>(row) < counts.size()) {
            g.setColour(kDim.withAlpha(0.7f)); g.setFont(juce::FontOptions(10.5f));
            g.drawText(juce::String(counts[static_cast<size_t>(row)]), w - 34, 0, 28, h, juce::Justification::centredRight);
        }
    }
}

void AmbientSynthEditor::BrowseView::Column::updateCounts()
{
    if (countsFor == numPresets() && counts.size() == static_cast<size_t>(items.size())) return;
    countsFor = numPresets();
    counts.assign(static_cast<size_t>(items.size()), 0);
    for (int i = 0; i < numPresets(); ++i) {
        const PresetMeta& m = presetMeta(i);
        for (size_t r = 1; r < counts.size(); ++r)
            if ((familyIdx[r] >= 0 && m.family == familyIdx[r]) || (tagBits[r] && (m.tags & tagBits[r]))) ++counts[r];
    }
}

void AmbientSynthEditor::BrowseView::Column::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row == 0) chosen.clear();
    else if (e.mods.isCommandDown() || e.mods.isCtrlDown()) { if (chosen.count(row)) chosen.erase(row); else chosen.insert(row); }
    else if (chosen.size() == 1 && chosen.count(row)) chosen.clear();
    else { chosen.clear(); chosen.insert(row); }
    box.repaint();
    if (owner) owner->applyFilter();
}

bool AmbientSynthEditor::BrowseView::Column::passes(int preset) const
{
    if (chosen.empty()) return true;
    const PresetMeta& m = presetMeta(preset);
    for (int row : chosen) {
        const size_t r = static_cast<size_t>(row);
        if (familyIdx[r] >= 0 && m.family == familyIdx[r]) return true;
        if (tagBits[r] && (m.tags & tagBits[r])) return true;
    }
    return false;
}

void AmbientSynthEditor::BrowseView::applyFilter()
{
    filtered.clear();
    const juce::String needle = search.getText().trim().toLowerCase();
    const int fam = family.getSelectedId() - 2;
    uint32_t need = 0;
    for (int t = 0; t < kNumPresetTags; ++t) if (tagButtons[static_cast<size_t>(t)]->getToggleState()) need |= (1u << t);
    for (int i = 0; i < numPresets(); ++i) {
        const PresetMeta& m = presetMeta(i);
        if (needle.isNotEmpty() && !juce::String(preset(i).name).toLowerCase().contains(needle)) continue;
        if (onlyFavourites.getToggleState() && !proc.isFavourite(i)) continue;
        if (!similarSet.empty() && std::find(similarSet.begin(), similarSet.end(), i) == similarSet.end()) continue;
        // Dull, measured rather than judged: in the bottom fifth for movement and for width, and
        // not carrying the sparseness that would make that a deliberate character.
        if (hideDull.getToggleState() && m.motion < 0.2f && m.width < 0.2f && m.density < 0.5f) continue;
        if (mode == 1) {
            if (fam >= 0 && m.family != fam) continue;
            if ((m.tags & need) != need) continue;
        } else {
            bool ok = true;
            for (const auto& c : columns) if (!c.passes(i)) { ok = false; break; }
            if (!ok) continue;
        }
        filtered.push_back(i);
    }
    const int s = sort.getSelectedId();
    auto key = [s](int i) -> float {
        const PresetMeta& m = presetMeta(i);
        switch (s) { case 3: return m.bright; case 4: return m.motion; case 5: return m.width; case 6: return m.noisy; case 7: return m.density; default: return 0.0f; }
    };
    if (s == 2) std::sort(filtered.begin(), filtered.end(), [](int a, int b) { return juce::String(preset(a).name).compareIgnoreCase(preset(b).name) < 0; });
    else if (s >= 3) std::stable_sort(filtered.begin(), filtered.end(), [&](int a, int b) { return key(a) < key(b); });
    list.updateContent();
    info.setText(juce::String(static_cast<int>(filtered.size())) + " of " + juce::String(numPresets()) + " presets" +
                 (numPresetMeta() == 0 ? "   (map not measured yet: run Tools/preset_map.py)" : ""), juce::dontSendNotification);
    map.repaint();
}

void AmbientSynthEditor::BrowseView::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool sel)
{
    if (row < 0 || row >= static_cast<int>(filtered.size())) return;
    const int i = filtered[static_cast<size_t>(row)];
    const PresetMeta& m = presetMeta(i);
    if (sel) g.fillAll(kAccent.withAlpha(0.18f));
    else if (i == proc.getCurrentProgram()) g.fillAll(kAccent.withAlpha(0.08f));
    g.setColour(familyColour(m.family)); g.fillEllipse(6.0f, h * 0.5f - 4.0f, 8.0f, 8.0f);
    g.setColour(proc.isFavourite(i) ? juce::Colour(0xffe0c070) : kSectionFill); g.setFont(juce::FontOptions(13.0f));
    g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x98\x85")), 18, 0, 16, h, juce::Justification::centred);
    g.setColour(kText); g.setFont(juce::FontOptions(13.0f));
    g.drawText(preset(i).name, 38, 0, w - 320, h, juce::Justification::centredLeft);
    if (mode == 0) { g.setColour(kDim); g.setFont(juce::FontOptions(11.0f)); g.drawText(presetFamilyName(m.family), w - 320, 0, 140, h, juce::Justification::centredLeft); }
    // small bars: bright, motion, width, density
    const float vals[4] = { m.bright, m.motion, m.width, m.density };
    const juce::Colour cols[4] = { juce::Colour(0xffe0c070), juce::Colour(0xff8ec9a8), juce::Colour(0xff7fb3d5), juce::Colour(0xffd08a8a) };
    for (int k = 0; k < 4; ++k) {
        const int x = w - 176 + k * 42;
        g.setColour(kSectionFill); g.fillRect(x, h / 2 - 3, 36, 6);
        g.setColour(cols[k]); g.fillRect(x, h / 2 - 3, static_cast<int>(36 * vals[k]), 6);
    }
}

void AmbientSynthEditor::BrowseView::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    if (row < 0 || row >= static_cast<int>(filtered.size())) return;
    selected = filtered[static_cast<size_t>(row)];
    if (e.x >= 16 && e.x < 36) { proc.setFavourite(selected, !proc.isFavourite(selected)); list.repaint(); return; }   // the star
    if (e.getNumberOfClicks() >= 2 || e.mods.isLeftButtonDown()) proc.setCurrentProgram(selected);
    if (mapActive.getToggleState()) {   // the cursor jumps to the preset's point
        const PresetMeta& m = presetMeta(selected);
        if (auto* px = proc.apvts.getParameter(paramDesc(ParamId::MapX).key)) px->setValueNotifyingHost(m.x);
        if (auto* py = proc.apvts.getParameter(paramDesc(ParamId::MapY).key)) py->setValueNotifyingHost(m.y);
    }
    map.repaint();
}

void AmbientSynthEditor::BrowseView::timerCallback()
{
    map.repaint(); list.repaint();
    if (routeEditOpen) {   // same pattern as the gesture editor: mirror while open, apply when gone
        if (routeEditor != nullptr) routeEditText = routeEditor->getText();
        else {
            routeEditOpen = false;
            if (routeEditText.trim().isEmpty()) proc.clearRoute();
            else if (!proc.setRouteText(routeEditText))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Route", "A point could not be parsed (unknown preset name or malformed); the previous route is kept.");
            routeBox.setSelectedId(0, juce::dontSendNotification);
        }
    }
}

void AmbientSynthEditor::BrowseView::showRouteEditor()
{
    auto* editor = new juce::TextEditor();
    editor->setMultiLine(true, true);
    editor->setReturnKeyStartsNewLine(true);
    editor->setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    editor->setText(proc.routeText().replace(";", ";\n"), false);
    editor->setSize(560, 320);
    auto* content = new juce::Component();
    content->setSize(560, 360);
    content->addAndMakeVisible(editor);
    editor->setTopLeftPosition(0, 0);
    auto* hint = new juce::Label(juce::String(), "one point per line:  Preset Name|travel s|hold s[|radius]   or   x,y|travel|hold[|radius]");
    hint->setFont(juce::FontOptions(11.0f)); hint->setColour(juce::Label::textColourId, kDim);
    hint->setBounds(0, 324, 560, 30); content->addAndMakeVisible(hint);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned(content);
    o.dialogTitle = "Route over the map";
    o.componentToCentreAround = this;
    o.dialogBackgroundColour = kGroupFill;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
    routeEditor = editor; routeEditText = editor->getText(); routeEditOpen = true;
}

void AmbientSynthEditor::BrowseView::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    g.setColour(kGroupFill);
    g.fillRoundedRectangle(getLocalBounds().reduced(8).toFloat(), 8.0f);
    g.setColour(kDim); g.setFont(juce::FontOptions(11.0f));
    g.drawText("bright  motion  width  density", list.getRight() - 176, list.getY() - 16, 176, 14, juce::Justification::centredLeft);
    if (mode == 0) {
        g.drawText("preset", list.getX() + 38, list.getY() - 16, 200, 14, juce::Justification::centredLeft);
        g.drawText("family", list.getRight() - 320, list.getY() - 16, 140, 14, juce::Justification::centredLeft);
        for (auto& c : columns) {
            g.setColour(kText); g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(c.title.toUpperCase(), c.box.getX(), c.box.getY() - 18, c.box.getWidth(), 16, juce::Justification::centredLeft);
        }
    }
}

void AmbientSynthEditor::BrowseView::resized()
{
    auto area = getLocalBounds().reduced(16);
    auto top = area.removeFromTop(26);
    modeClassic.setBounds(top.removeFromLeft(80)); modeMap.setBounds(top.removeFromLeft(60)); top.removeFromLeft(12);
    search.setBounds(top.removeFromLeft(180)); top.removeFromLeft(6);
    if (mode == 1) { family.setBounds(top.removeFromLeft(150)); top.removeFromLeft(6); }
    sort.setBounds(top.removeFromLeft(150)); top.removeFromLeft(12);
    onlyFavourites.setBounds(top.removeFromLeft(130));
    similar.setBounds(top.removeFromLeft(150));
    hideDull.setBounds(top.removeFromLeft(160));
    area.removeFromTop(8);
    auto buttons = area.removeFromBottom(26);
    load.setBounds(buttons.removeFromLeft(80)); buttons.removeFromLeft(6);
    toA.setBounds(buttons.removeFromLeft(60)); buttons.removeFromLeft(6);
    toB.setBounds(buttons.removeFromLeft(60)); buttons.removeFromLeft(6);
    star.setBounds(buttons.removeFromLeft(90)); buttons.removeFromLeft(12);
    if (mode == 1) {
        auto mapControls = buttons.removeFromRight(juce::jmax(300, buttons.getWidth() * 3 / 5));
        mapActive.setBounds(mapControls.removeFromLeft(220)); mapControls.removeFromLeft(12);
        radius.setBounds(mapControls);
    }
    info.setBounds(buttons);
    area.removeFromBottom(6);
    if (mode == 1) {   // route strip above the bottom row, on the map side
        auto strip = area.removeFromBottom(26);
        strip.removeFromLeft(juce::jmax(420, (getLocalBounds().reduced(16).getWidth()) * 2 / 5) + 12);
        routeBox.setBounds(strip.removeFromLeft(170)); strip.removeFromLeft(6);
        routePlay.setBounds(strip.removeFromLeft(96)); strip.removeFromLeft(2);
        routeLoop.setBounds(strip.removeFromLeft(56)); strip.removeFromLeft(6);
        routeSpeed.setBounds(strip.removeFromLeft(juce::jmax(120, strip.getWidth() - 250))); strip.removeFromLeft(6);
        routeAdd.setBounds(strip.removeFromLeft(70)); strip.removeFromLeft(4);
        routeClear.setBounds(strip.removeFromLeft(56)); strip.removeFromLeft(4);
        routeEdit.setBounds(strip.removeFromLeft(70));
        area.removeFromBottom(6);
    }

    if (mode == 0) {
        // Omnisphere-style: four columns on top, the results below.
        auto cols = area.removeFromTop(juce::jmax(160, area.getHeight() * 2 / 5));
        const int colW = cols.getWidth() / 4;
        for (int c = 0; c < 4; ++c) {
            auto r = cols.removeFromLeft(colW).reduced(0, 0);
            r.removeFromTop(18);   // title painted above
            columns[c].box.setBounds(r.reduced(c == 0 ? 0 : 4, 0).withTrimmedRight(4));
        }
        area.removeFromTop(24);    // column header of the result list
        list.setBounds(area);
    } else {
        auto left = area.removeFromLeft(juce::jmax(420, area.getWidth() * 2 / 5));
        area.removeFromLeft(12);
        const int perRow = 5, tagH = 22;
        const int rows = (kNumPresetTags + perRow - 1) / perRow;
        auto tags = left.removeFromTop(rows * tagH);
        for (int t = 0; t < kNumPresetTags; ++t) {
            const int r = t / perRow, c = t % perRow;
            tagButtons[static_cast<size_t>(t)]->setBounds(tags.getX() + c * (tags.getWidth() / perRow), tags.getY() + r * tagH, tags.getWidth() / perRow, tagH);
        }
        left.removeFromTop(18);
        list.setBounds(left);
        map.setBounds(area);
    }
}

// The window onto the plane: at zoom 1 and centre (0.5, 0.5) this is exactly the old fixed view.
juce::Point<float> AmbientSynthEditor::BrowseView::MapView::toScreen(float x, float y) const
{
    const auto r = getLocalBounds().toFloat().reduced(18.0f);
    return { r.getCentreX() + (x - centre.x) * zoom * r.getWidth(),
             r.getCentreY() - (y - centre.y) * zoom * r.getHeight() };
}

juce::Point<float> AmbientSynthEditor::BrowseView::MapView::toMapRaw(juce::Point<float> s) const
{
    const auto r = getLocalBounds().toFloat().reduced(18.0f);
    return { centre.x + (s.x - r.getCentreX()) / juce::jmax(1.0f, zoom * r.getWidth()),
             centre.y + (r.getCentreY() - s.y) / juce::jmax(1.0f, zoom * r.getHeight()) };
}

juce::Point<float> AmbientSynthEditor::BrowseView::MapView::toMap(juce::Point<float> s) const
{
    const auto m = toMapRaw(s);
    return { juce::jlimit(0.0f, 1.0f, m.x), juce::jlimit(0.0f, 1.0f, m.y) };
}

void AmbientSynthEditor::BrowseView::MapView::zoomTo(float x, float y, float z)
{
    zoom = juce::jlimit(1.0f, 40.0f, z);
    centre = { x, y };
    if (zoom <= 1.0f) centre = { 0.5f, 0.5f };
    repaint();
}

void AmbientSynthEditor::BrowseView::MapView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // Zoom about the mouse: the point of the plane under the cursor stays under the cursor, so
    // the eye can follow a cluster in while it opens up.
    const auto pivot = toMapRaw(e.position);
    const float factor = std::pow(1.25f, w.deltaY * 4.0f);
    const float newZoom = juce::jlimit(1.0f, 40.0f, zoom * factor);
    if (newZoom == zoom) return;
    zoom = newZoom;
    const auto r = getLocalBounds().toFloat().reduced(18.0f);
    centre = { pivot.x - (e.position.x - r.getCentreX()) / (zoom * r.getWidth()),
               pivot.y - (r.getCentreY() - e.position.y) / (zoom * r.getHeight()) };
    if (zoom <= 1.0f) centre = { 0.5f, 0.5f };
    repaint();
}

void AmbientSynthEditor::BrowseView::MapView::mouseDoubleClick(const juce::MouseEvent&)
{
    zoomTo(0.5f, 0.5f, 1.0f);
}

int AmbientSynthEditor::BrowseView::MapView::nearestPreset(juce::Point<float> p, float maxDist) const
{
    int best = -1; float bd = maxDist * maxDist;
    for (int i = 0; i < std::min(numPresetMeta(), numPresets()); ++i) {
        const auto s = toScreen(presetMeta(i).x, presetMeta(i).y);
        const float d = (s.x - p.x) * (s.x - p.x) + (s.y - p.y) * (s.y - p.y);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void AmbientSynthEditor::BrowseView::MapView::paint(juce::Graphics& g)
{
    g.setColour(kBg.brighter(0.03f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    g.setColour(kSectionFill);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 6.0f, 1.0f);
    if (numPresetMeta() == 0) {
        g.setColour(kDim); g.setFont(juce::FontOptions(13.0f));
        g.drawText("The map is not measured yet: run Tools/preset_map.py and rebuild.", getLocalBounds(), juce::Justification::centred);
        return;
    }
    const bool blend = owner.mapActive.getToggleState();
    const float x = owner.proc.engine().getParam(ParamId::MapX), y = owner.proc.engine().getParam(ParamId::MapY);
    const float rad = owner.proc.engine().getParam(ParamId::MapRadius);
    const auto cursor = toScreen(x, y);
    const auto r = getLocalBounds().toFloat().reduced(18.0f);
    if (blend) {   // the blend radius as a soft disc
        const float rr = rad * r.getWidth() * zoom;
        g.setColour(kAccent.withAlpha(0.10f)); g.fillEllipse(cursor.x - rr, cursor.y - rr, 2 * rr, 2 * rr);
        g.setColour(kAccent.withAlpha(0.25f)); g.drawEllipse(cursor.x - rr, cursor.y - rr, 2 * rr, 2 * rr, 1.0f);
    }
    // dimmed points first, then the filtered ones, then the current program
    const int current = owner.proc.getCurrentProgram();
    std::vector<bool> inFilter(static_cast<size_t>(numPresets()), false);
    for (int i : owner.filtered) inFilter[static_cast<size_t>(i)] = true;
    // With a preset library loaded there can be thousands of points: shrink the dots so the
    // plane stays readable, and draw the dimmed ones as squares, which is much cheaper to fill.
    const int shown = std::min(numPresetMeta(), numPresets());
    // The dots grow a little with the zoom -- not with it, or a cluster opened up would be
    // blobs -- and everything outside the window is skipped, which at forty times is most of it.
    const float dotScale = juce::jlimit(0.34f, 1.0f, std::sqrt(200.0f / juce::jmax(1, shown))) * std::pow(zoom, 0.35f);
    const bool many = shown > 800;
    const auto screen = getLocalBounds().toFloat().expanded(12.0f);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < shown; ++i) {
            if (inFilter[static_cast<size_t>(i)] != (pass == 1)) continue;
            const PresetMeta& m = presetMeta(i);
            const auto s = toScreen(m.x, m.y);
            if (!screen.contains(s)) continue;
            const float size = juce::jmax(2.0f, (6.0f + 6.0f * m.density) * dotScale);
            juce::Colour c = familyColour(m.family);
            if (pass == 0) c = c.withAlpha(0.18f);
            g.setColour(c);
            if (pass == 0 && many) g.fillRect(s.x - size / 2, s.y - size / 2, size, size);
            else                   g.fillEllipse(s.x - size / 2, s.y - size / 2, size, size);
            if (i == owner.selected || i == current) { g.setColour(kText); g.drawEllipse(s.x - size / 2 - 3, s.y - size / 2 - 3, size + 6, size + 6, 1.5f); }
        }
    }
    // Names, once there is room for them. Zoomed in past four, every preset in the filter whose
    // label would not sit on another's gets its name; the check is a coarse grid of the label's
    // own size, so a cluster shows the few names that fit and not a smear of all of them.
    if (zoom >= 4.0f) {
        g.setFont(juce::FontOptions(10.5f));
        std::set<std::pair<int, int>> taken;
        const int cw = 96, ch = 13;
        for (int i : owner.filtered) {
            if (i >= shown) continue;
            const PresetMeta& m = presetMeta(i);
            const auto s = toScreen(m.x, m.y);
            if (!screen.contains(s)) continue;
            const std::pair<int, int> cell { static_cast<int>(s.x) / cw, static_cast<int>(s.y) / ch };
            if (!taken.insert(cell).second) continue;
            g.setColour((i == owner.selected || i == current) ? kText : kDim);
            g.drawText(preset(i).name, static_cast<int>(s.x) + 7, static_cast<int>(s.y) - 7, cw + 40, ch, juce::Justification::centredLeft);
        }
    }
    if (zoom > 1.0f) {   // say where we are, and how to get back
        g.setColour(kDim); g.setFont(juce::FontOptions(10.5f));
        g.drawText(juce::String("zoom x") + juce::String(zoom, 1) + "   drag to pan, double-click to reset",
                   getLocalBounds().reduced(10).removeFromTop(14), juce::Justification::topRight);
    }
    // the route: numbered points joined by a line, the segment being walked highlighted
    {
        const Route& rt = owner.proc.engine().route();
        if (rt.count() > 0) {
            const bool running = rt.running();
            const int seg = rt.segment();
            for (int i = 0; i < rt.count(); ++i) {
                const Waypoint& w = rt.point(i);
                const auto s = toScreen(w.x, w.y);
                if (i > 0) {
                    const Waypoint& pw = rt.point(i - 1);
                    const auto ps = toScreen(pw.x, pw.y);
                    g.setColour(juce::Colour(0xffe0c070).withAlpha(running && i == seg ? 0.9f : 0.35f));
                    g.drawLine(juce::Line<float>(ps, s), running && i == seg ? 2.0f : 1.0f);
                }
                g.setColour(juce::Colour(0xffe0c070).withAlpha(0.8f));
                g.drawEllipse(s.x - 9, s.y - 9, 18, 18, 1.2f);
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText(juce::String(i + 1), static_cast<int>(s.x) - 9, static_cast<int>(s.y) - 9, 18, 18, juce::Justification::centred);
            }
            if (owner.proc.engine().getParam(ParamId::RouteLoop) >= 0.5f && rt.count() > 1) {
                g.setColour(juce::Colour(0xffe0c070).withAlpha(0.2f));
                g.drawLine(juce::Line<float>(toScreen(rt.point(rt.count() - 1).x, rt.point(rt.count() - 1).y), toScreen(rt.point(0).x, rt.point(0).y)), 1.0f);
            }
        }
    }
    // neighbour lines while blending
    if (blend) {
        const PresetMap::Blend b = PresetMap::neighbours(x, y, rad);
        for (int k = 0; k < b.count; ++k) {
            if (b.weight[k] < 0.02f) continue;
            const PresetMeta& m = presetMeta(b.index[k]);
            const auto s = toScreen(m.x, m.y);
            g.setColour(kAccent.withAlpha(0.2f + 0.7f * b.weight[k]));
            g.drawLine(juce::Line<float>(cursor, s), 1.0f + 2.0f * b.weight[k]);
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(juce::String(preset(b.index[k]).name) + " " + juce::String(static_cast<int>(100 * b.weight[k])) + "%", static_cast<int>(s.x) + 8, static_cast<int>(s.y) - 7, 220, 14, juce::Justification::centredLeft);
        }
        g.setColour(kText);
        g.drawLine(cursor.x - 8, cursor.y, cursor.x + 8, cursor.y, 1.2f);
        g.drawLine(cursor.x, cursor.y - 8, cursor.x, cursor.y + 8, 1.2f);
    }
    if (hover >= 0 && hover < numPresets()) {
        const PresetMeta& m = presetMeta(hover);
        const auto s = toScreen(m.x, m.y);
        juce::String tags;
        for (int t = 0; t < kNumPresetTags; ++t) if (m.tags & (1u << t)) tags += juce::String(presetTagName(t)) + " ";
        g.setColour(kText); g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(preset(hover).name, static_cast<int>(s.x) + 10, static_cast<int>(s.y) - 20, 260, 14, juce::Justification::centredLeft);
        g.setColour(kDim); g.setFont(juce::FontOptions(10.5f));
        g.drawText(juce::String(presetFamilyName(m.family)) + "  -  " + tags.trim(), static_cast<int>(s.x) + 10, static_cast<int>(s.y) - 6, 360, 14, juce::Justification::centredLeft);
    }
    // legend
    g.setFont(juce::FontOptions(10.5f));
    int lx = 10, ly = getHeight() - 16;
    for (int f = 0; f < numPresetFamilies(); ++f) {
        g.setColour(familyColour(f)); g.fillEllipse(static_cast<float>(lx), static_cast<float>(ly) + 3.0f, 7.0f, 7.0f);
        g.setColour(kDim); g.drawText(presetFamilyName(f), lx + 10, ly, 120, 13, juce::Justification::centredLeft);
        lx += 14 + 7 * juce::jmin(16, static_cast<int>(std::strlen(presetFamilyName(f))));
        if (lx > getWidth() - 120) break;
    }
}

void AmbientSynthEditor::BrowseView::MapView::mouseMove(const juce::MouseEvent& e)
{
    const int h = nearestPreset(e.position, 10.0f);
    if (h != hover) { hover = h; repaint(); }
}

void AmbientSynthEditor::BrowseView::MapView::mouseDown(const juce::MouseEvent& e)
{
    dragging = false;
    panning = false;
    // The right button, the middle button, or a drag on empty plane while the blend cursor is
    // off: all of those pan. With the blend on, the left button is the cursor, as it always was.
    if (e.mods.isRightButtonDown() || e.mods.isMiddleButtonDown()) {
        panning = true; panFrom = e.position; centreFrom = centre; return;
    }
    const int h = nearestPreset(e.position, 10.0f);
    if (h >= 0 && !owner.mapActive.getToggleState()) {   // plain click on a point loads it
        owner.selected = h; owner.proc.setCurrentProgram(h); owner.list.repaint(); repaint(); return;
    }
    if (!owner.mapActive.getToggleState()) { panning = true; panFrom = e.position; centreFrom = centre; return; }
    mouseDrag(e);
}

void AmbientSynthEditor::BrowseView::MapView::mouseDrag(const juce::MouseEvent& e)
{
    if (panning) {
        const auto r = getLocalBounds().toFloat().reduced(18.0f);
        centre = { centreFrom.x - (e.position.x - panFrom.x) / (zoom * juce::jmax(1.0f, r.getWidth())),
                   centreFrom.y + (e.position.y - panFrom.y) / (zoom * juce::jmax(1.0f, r.getHeight())) };
        // Keep the plane on screen: the centre may not leave the unit square by more than the
        // half-window, so the last row of presets stays reachable and nothing is lost off the edge.
        const float half = 0.5f / zoom;
        centre = { juce::jlimit(half, 1.0f - half, centre.x), juce::jlimit(half, 1.0f - half, centre.y) };
        repaint();
        return;
    }
    dragging = true;
    if (!owner.mapActive.getToggleState()) return;
    const auto m = toMap(e.position);
    if (auto* px = owner.proc.apvts.getParameter(paramDesc(ParamId::MapX).key)) px->setValueNotifyingHost(m.x);
    if (auto* py = owner.proc.apvts.getParameter(paramDesc(ParamId::MapY).key)) py->setValueNotifyingHost(m.y);
    repaint();
}

void AmbientSynthEditor::BrowseView::MapView::mouseUp(const juce::MouseEvent&) { dragging = false; panning = false; }

AmbientSynthEditor::PerformView::PerformView(AmbientSynthProcessor& p) : proc(p)
{
    for (int m = 0; m < 8; ++m) {
        const ParamDesc& d = paramDesc(static_cast<ParamId>(static_cast<int>(ParamId::MacroA) + m));
        auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
        s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);
        addAndMakeVisible(*s);
        auto l = std::make_unique<juce::Label>(juce::String(), d.name);
        l->setJustificationType(juce::Justification::centred);
        l->setFont(juce::FontOptions(20.0f, juce::Font::bold));
        l->setColour(juce::Label::textColourId, kText);
        addAndMakeVisible(*l);
        attachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, d.key, *s));
        knobs.push_back(std::move(s));
        labels.push_back(std::move(l));
    }
    morph.setSliderStyle(juce::Slider::LinearHorizontal);
    morph.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    addAndMakeVisible(morph);
    morphAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, paramDesc(ParamId::MorphPos).key, morph);
    morphActive.setButtonText("Morph");
    addAndMakeVisible(morphActive);
    morphActiveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, paramDesc(ParamId::MorphActive).key, morphActive);
    for (auto* l : { &morphLabel, &aLabel, &bLabel }) {
        l->setFont(juce::FontOptions(16.0f));
        l->setColour(juce::Label::textColourId, kDim);
        addAndMakeVisible(*l);
    }
    morphLabel.setText("A  <  position  >  B", juce::dontSendNotification);
    morphLabel.setJustificationType(juce::Justification::centred);
    aLabel.setJustificationType(juce::Justification::centredRight);
    bLabel.setJustificationType(juce::Justification::centredLeft);

    // Set timeline
    setRec.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb03030));
    setRec.onClick = [this] {
        if (proc.isRecordingSet()) {
            chooser = std::make_unique<juce::FileChooser>("Save the set", juce::File(), "*.ambientset");
            chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [this](const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File() && !file.hasFileExtension("ambientset")) file = file.withFileExtension("ambientset");
                    if (!proc.stopSetRecording(file) && file != juce::File())
                        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Set", "Could not write the set file.");
                    updateSetInfo();
                });
        } else { proc.startSetRecording(); updateSetInfo(); }
    };
    setPlay.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Play a set", juce::File(), "*.ambientset");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                const auto file = fc.getResult();
                if (!file.existsAsFile()) return;
                if (!proc.playSetFile(file)) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Set", "This is not a readable set file.");
                updateSetInfo();
            });
    };
    setStop.onClick = [this] { proc.stopSetPlayback(); if (proc.isRecordingSet()) proc.stopSetRecording(juce::File()); updateSetInfo(); };
    for (auto* b : { &setRec, &setPlay, &setStop }) addAndMakeVisible(*b);
    setInfo.setFont(juce::FontOptions(12.0f)); setInfo.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(setInfo);
    updateSetInfo();
}

void AmbientSynthEditor::PerformView::updateSetInfo()
{
    setRec.setToggleState(proc.isRecordingSet(), juce::dontSendNotification);
    const int t = static_cast<int>(proc.setTime());
    juce::String text;
    if (proc.isRecordingSet()) text = juce::String::formatted("recording set  %d:%02d", t / 60, t % 60);
    else if (proc.isPlayingSet()) text = juce::String::formatted("playing set  %d:%02d", t / 60, t % 60);
    else text = "a set = every knob, gesture and note with its time; Record, play a while, stop to save; render it again with ambient_render --set-file";
    setInfo.setText(text, juce::dontSendNotification);
}

void AmbientSynthEditor::PerformView::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
    g.setColour(kGroupFill);
    g.fillRoundedRectangle(getLocalBounds().reduced(12).toFloat(), 10.0f);
    g.setColour(kDim);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("PERFORM  -  eight macros, one morph. Each knob moves several parameters; a knob acts once it has been moved.",
               getLocalBounds().reduced(24).removeFromTop(24), juce::Justification::centredLeft);
    aLabel.setText("A: " + proc.morphSlotName(0), juce::dontSendNotification);
    bLabel.setText("B: " + proc.morphSlotName(1), juce::dontSendNotification);
    updateSetInfo();
}

void AmbientSynthEditor::PerformView::resized()
{
    auto area = getLocalBounds().reduced(24);
    auto top = area.removeFromTop(32);
    {   // set controls on the right of the title line
        auto s = top.removeFromRight(juce::jmin(720, top.getWidth() - 620));
        setStop.setBounds(s.removeFromRight(90).reduced(0, 4)); s.removeFromRight(6);
        setPlay.setBounds(s.removeFromRight(100).reduced(0, 4)); s.removeFromRight(6);
        setRec.setBounds(s.removeFromRight(100).reduced(0, 4)); s.removeFromRight(10);
        setInfo.setBounds(s);
    }
    auto morphArea = area.removeFromBottom(90);
    const int cols = 4, rows = 2;
    const int cellW = area.getWidth() / cols, cellH = area.getHeight() / rows;
    for (int m = 0; m < 8; ++m) {
        juce::Rectangle<int> cell(area.getX() + (m % cols) * cellW, area.getY() + (m / cols) * cellH, cellW, cellH);
        cell = cell.reduced(16);
        labels[static_cast<size_t>(m)]->setBounds(cell.removeFromBottom(30));
        const int side = std::min(cell.getWidth(), cell.getHeight());
        knobs[static_cast<size_t>(m)]->setBounds(cell.withSizeKeepingCentre(side, side));
    }
    morphActive.setBounds(morphArea.removeFromLeft(90).withSizeKeepingCentre(90, 28));
    morphLabel.setBounds(morphArea.removeFromTop(22));
    aLabel.setBounds(morphArea.removeFromLeft(220));
    bLabel.setBounds(morphArea.removeFromRight(220));
    morph.setBounds(morphArea.withSizeKeepingCentre(morphArea.getWidth(), 40));
}
