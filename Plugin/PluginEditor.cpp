#include "PluginEditor.h"
#include "AmbientLookAndFeel.h"
#include <cstdlib>
#include "ambient/Params.h"
#include "ambient/Presets.h"
#include "ambient/PresetMeta.h"
#include "ambient/PresetMap.h"
#include "ambient/Route.h"

using namespace ambient;

namespace {
// The library runs to thousands of presets, so a flat list is unreadable: group the box by
// family (the built-in families first, then one heading per loaded pack).
void fillPresetBox(juce::ComboBox& box)
{
    const bool grouped = numPresetMeta() >= numPresets() && numPresetFamilies() > 1;
    int lastFamily = -1;
    for (int i = 0; i < numPresets(); ++i) {
        if (grouped) {
            const int fam = presetMeta(i).family;
            if (fam != lastFamily) { box.addSectionHeading(presetFamilyName(fam)); lastFamily = fam; }
        }
        box.addItem(preset(i).name, i + 1);
    }
}
} // namespace


namespace {
// One cell is a knob and its name; the value is drawn inside the knob (see AmbientLookAndFeel),
// which is what buys the room for a knob this size in the same wall of controls.
constexpr int kCellW = 68, kCellH = 76, kPad = 11, kTitleH = 22, kGroupTitleH = 26, kHeaderH = 118;
// The editor is laid out once, in this design space, and the whole thing is then scaled to
// whatever size the window has. Dragging the corner is the zoom; the aspect ratio is fixed, so
// nothing ever reflows into a different arrangement -- it only gets bigger or smaller.
constexpr int kMinDesignW = 1400, kMinDesignH = 820;
const juce::Colour kBg = ui::bg0, kGroupFill = ui::group, kSectionFill = ui::card, kAccent = ui::accent,
                   kText = ui::text, kDim = ui::dim;
const juce::Colour kVoice = ui::voiceCol, kFore = ui::foreCol, kBack = ui::backCol, kCosmos = ui::cosmosCol,
                   kConductor = ui::condCol, kMorph = ui::morphCol, kMaster = ui::masterCol;
}

AmbientSynthEditor::AmbientSynthEditor(AmbientSynthProcessor& p)
    : AudioProcessorEditor(p), proc_(p)
{
    setLookAndFeel(&laf_);

    groups_ = {
        { "VOICE",      kVoice,     { { "Oscillator", "Envelope" }, { "Source 2" }, { "Source 3" }, { "Air", "Filter" }, { "Space" }, { "Z-Plane" }, { "Foundation" } }, {}, 0 },
        { "FOREGROUND", kFore,      { { "Ensemble", "Delay" }, { "Delay 2", "Near Reverb" } }, {}, 0 },
        { "BACKGROUND", kBack,      { { "Cloud", "Far Reverb" }, { "Feedback", "Room" } }, {}, 0 },
        { "CONDUCTOR",  kConductor, { { "Cluster Brain" }, { "Tuning" }, { "Coherence" } }, {}, 1 },
        { "COSMOS",     kCosmos,    { { "Cosmos" } }, {}, 1 },
        { "MORPH",      kMorph,     { { "Morph" }, { "Macros" } }, {}, 1 },
    };

    content_.onPaint = [this](juce::Graphics& g) { paintContent(g); };
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);
    buildCells();
    colourCellsByGroup();
    // The Master section lives in the header, so its cells belong to the editor, not the content.
    if (Section* ms = findSection("Master"))
        for (int ci : ms->cells) { addAndMakeVisible(*cells_[static_cast<size_t>(ci)].comp); addAndMakeVisible(*cells_[static_cast<size_t>(ci)].label); }

    recButton_ = std::make_unique<juce::TextButton>("Rec");
    recButton_->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffb03030));
    recButton_->setClickingTogglesState(false);
    recButton_->onClick = [this] {
        if (proc_.isRecording()) { proc_.stopRecording(); recButton_->setToggleState(false, juce::dontSendNotification); return; }
        chooser_ = std::make_unique<juce::FileChooser>("Record to WAV", juce::File(), "*.wav");
        chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file == juce::File()) return;
                if (!file.hasFileExtension("wav")) file = file.withFileExtension("wav");
                if (proc_.startRecording(file)) recButton_->setToggleState(true, juce::dontSendNotification);
                else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Record", "Could not open the file for writing.");
            });
    };
    addAndMakeVisible(*recButton_);
    calibButton_ = std::make_unique<juce::TextButton>("Calibrate");
    calibButton_->setTooltip("Hands: together and apart, low and high, near and far, for 6 s");
    calibButton_->onClick = [this] { proc_.gestures().startCalibration(6.0f); };
    addAndMakeVisible(*calibButton_);
    mapButton_ = std::make_unique<juce::TextButton>("Gestures...");
    mapButton_->setTooltip("Edit the gesture/macro mapping table");
    mapButton_->onClick = [this] { showMappingEditor(); };
    addAndMakeVisible(*mapButton_);
    performButton_ = std::make_unique<juce::TextButton>("Perform");
    performButton_->setTooltip("Only the eight macros and the morph, large: for playing a set");
    performButton_->setClickingTogglesState(true);
    performButton_->setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
    performButton_->onClick = [this] { setPage(performButton_->getToggleState() ? 1 : 0); };
    addAndMakeVisible(*performButton_);
    perform_ = std::make_unique<PerformView>(proc_);
    addChildComponent(*perform_);
    browseButton_ = std::make_unique<juce::TextButton>("Browse");
    browseButton_->setTooltip("Preset browser: filters, tags, and the map of all presets (drag the cursor to blend)");
    browseButton_->setClickingTogglesState(true);
    browseButton_->setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
    browseButton_->onClick = [this] { setPage(browseButton_->getToggleState() ? 2 : 0); };
    addAndMakeVisible(*browseButton_);
    browse_ = std::make_unique<BrowseView>(proc_);
    addChildComponent(*browse_);

    // Header controls
    soundBox_ = std::make_unique<juce::ComboBox>();
    soundBox_->setTextWhenNothingSelected("Sound preset");
    fillPresetBox(*soundBox_);
    soundBox_->setSelectedId(proc_.soundPresetIndex() + 1, juce::dontSendNotification);
    soundBox_->onChange = [this] {
        const int idx = soundBox_->getSelectedId() - 1;
        if (idx >= 0 && idx != proc_.soundPresetIndex()) proc_.applySoundPreset(idx);
    };
    addAndMakeVisible(*soundBox_);
    cosmosBox_ = std::make_unique<juce::ComboBox>();
    cosmosBox_->setTextWhenNothingSelected("Cosmos preset");
    for (int i = 0; i < numCosmosPresets(); ++i) cosmosBox_->addItem(cosmosPreset(i).name, i + 1);
    cosmosBox_->setSelectedId(proc_.cosmosPresetIndex() + 1, juce::dontSendNotification);
    cosmosBox_->onChange = [this] {
        const int idx = cosmosBox_->getSelectedId() - 1;
        if (idx >= 0 && idx != proc_.cosmosPresetIndex()) proc_.applyCosmosPreset(idx);
    };
    addAndMakeVisible(*cosmosBox_);

    saveButton_ = std::make_unique<juce::TextButton>("Save...");
    saveButton_->onClick = [this] {
        chooser_ = std::make_unique<juce::FileChooser>("Save preset", juce::File(), "*.ambientsynth");
        chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [this](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file == juce::File()) return;
                if (!file.hasFileExtension("ambientsynth")) file = file.withFileExtension("ambientsynth");
                if (!proc_.savePresetFile(file))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Preset", "Could not write the preset file.");
            });
    };
    addAndMakeVisible(*saveButton_);
    loadButton_ = std::make_unique<juce::TextButton>("Load...");
    loadButton_->onClick = [this] {
        chooser_ = std::make_unique<juce::FileChooser>("Load preset", juce::File(), "*.ambientsynth");
        chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc) {
                const auto file = fc.getResult();
                if (!file.existsAsFile()) return;
                if (!proc_.loadPresetFile(file))
                    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Preset", "This is not an AmbientSynth preset file.");
                repaint();
            });
    };
    addAndMakeVisible(*loadButton_);

    // Master gain knob lives in the header next to the Master section.
    master_ = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    master_->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    addAndMakeVisible(*master_);
    masterAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "master_gain", *master_);

    modButton_ = std::make_unique<juce::TextButton>("Mod");
    modButton_->setClickingTogglesState(true);
    modButton_->onClick = [this] { setPage(modButton_->getToggleState() ? 3 : 0); };
    addAndMakeVisible(*modButton_);
    mod_ = std::make_unique<ModView>(proc_);
    modPort_.setViewedComponent(mod_.get(), false);
    modPort_.setScrollBarsShown(true, false);
    addChildComponent(modPort_);

    scope_ = std::make_unique<ScopeView>(proc_);
    addAndMakeVisible(*scope_);

    // Free scaling: the corner is the zoom. The ratio is fixed so the arrangement never changes,
    // only its size, and the window opens at whatever fraction of the screen actually fits.
    setResizable(true, true);
    layoutBody();                       // measure once: the design size is what the body needs
    // Width: exactly what the body needs, so nothing ever scrolls sideways. Height: a landscape
    // window; the body is taller than any screen and scrolls vertically, as it always has.
    designW_ = std::max(kMinDesignW, bodyW_);
    designH_ = std::max(kMinDesignH, kHeaderH + std::min(bodyH_, juce::roundToInt(designW_ * 0.50f)));
    if (auto* con = getConstrainer()) {
        con->setFixedAspectRatio(static_cast<double>(designW_) / static_cast<double>(designH_));
        con->setSizeLimits(designW_ / 3, designH_ / 3, designW_ * 2, designH_ * 2);
    }
    {
        float fit = 1.0f;
        if (auto* screen = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
            const auto area = screen->userArea;
            fit = juce::jlimit(0.34f, 1.0f, juce::jmin(area.getWidth() * 0.94f / designW_,
                                                      area.getHeight() * 0.90f / designH_));
        }
        setSize(juce::roundToInt(designW_ * fit), juce::roundToInt(designH_ * fit));
    }
    if (juce::SystemStats::getEnvironmentVariable("AMBIENT_PERFORM", "").isNotEmpty()) setPage(1);   // open on the perform page
    {   // dev aids: AMBIENT_BROWSE=1|map opens the browser (map view with "map"), AMBIENT_ROUTE=<route preset> preloads a route
        if (juce::SystemStats::getEnvironmentVariable("AMBIENT_MOD", "").isNotEmpty()) setPage(3);
        const juce::String br = juce::SystemStats::getEnvironmentVariable("AMBIENT_BROWSE", "");
        if (br.isNotEmpty()) { setPage(2); if (br == "map") browse_->setMode(1); }
        const juce::String rt = juce::SystemStats::getEnvironmentVariable("AMBIENT_ROUTE", "");
        for (int r = 0; rt.isNotEmpty() && r < numRoutePresets(); ++r) if (rt == routePreset(r).name) proc_.setRouteText(routePreset(r).points);
    }
    startTimerHz(12);
}

AmbientSynthEditor::~AmbientSynthEditor()
{
    setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------- cells

void AmbientSynthEditor::buildCells()
{
    for (const ParamDesc& d : paramTable()) {
        if (d.id == ParamId::MasterGain) continue;   // header knob
        Section* sec = findSection(d.section);
        if (sec == nullptr) {
            Section s; s.name = d.section;
            if (s.name == "Tuning" || s.name == "Far Reverb" || s.name == "Cosmos") s.maxUnits = 8;
            if (s.name == "Tuning") s.maxUnits = 9;
            if (s.name == "Morph" || s.name == "Foundation") s.maxUnits = 9;
            if (s.name == "Source 2" || s.name == "Source 3") s.maxUnits = 12;
            if (s.name == "Z-Plane") s.maxUnits = 11;
            for (int gi = 0; gi < static_cast<int>(groups_.size()); ++gi)
                for (auto& row : groups_[static_cast<size_t>(gi)].rows)
                    for (auto& n : row) if (n == s.name) s.group = gi;
            sections_.push_back(std::move(s));
            sec = &sections_.back();
        }
        Cell c;
        c.param = static_cast<int>(d.id);
        c.baseLabel = d.name;
        c.label = std::make_unique<juce::Label>(juce::String(), d.name);
        c.label->setJustificationType(juce::Justification::centred);
        c.label->setFont(juce::FontOptions(11.0f));
        c.label->setColour(juce::Label::textColourId, kDim);
        content_.addAndMakeVisible(*c.label);
        switch (d.kind) {
        case ParamKind::Float:
        case ParamKind::Int: {
            auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            if (d.unit[0] != 0) s->setTextValueSuffix(juce::String(" ") + d.unit);
            // A parameter that spans zero reads much better as an arc growing out of the centre.
            if (d.min < -1.0e-6f && d.max > 1.0e-6f) s->getProperties().set("bipolar", true);
            content_.addAndMakeVisible(*s);
            c.slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, d.key, *s);
            c.comp = std::move(s);
            break;
        }
        case ParamKind::Bool: {
            auto b = std::make_unique<juce::ToggleButton>();
            content_.addAndMakeVisible(*b);
            c.button = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.apvts, d.key, *b);
            c.comp = std::move(b);
            break;
        }
        case ParamKind::Choice: {
            auto cb = std::make_unique<juce::ComboBox>();
            for (int i = 0; i < d.numChoices; ++i) cb->addItem(d.choices[i], i + 1);
            content_.addAndMakeVisible(*cb);
            c.combo = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc_.apvts, d.key, *cb);
            c.comp = std::move(cb);
            c.units = 2;
            break;
        }
        }
        c.comp->addMouseListener(this, false);
        cellOf_[c.comp.get()] = static_cast<int>(cells_.size());
        sec->cells.push_back(static_cast<int>(cells_.size()));
        cells_.push_back(std::move(c));
    }

    // Extra cells: Scala loader, source files, morph slot pickers and capture buttons.
    auto scala = std::make_unique<juce::TextButton>("Load Scala...");
    scala->onClick = [this] { chooseScalaFile(); };
    addExtraCell("Tuning", std::move(scala), "User scale", 2);
    auto table = std::make_unique<juce::TextButton>("Wavetable...");
    table->onClick = [this] { chooseSourceFile(true); };
    tableCell_ = addExtraCell("Source 2", std::move(table), "User table", 2);
    auto texture = std::make_unique<juce::TextButton>("Texture...");
    texture->onClick = [this] { chooseSourceFile(false); };
    textureCell_ = addExtraCell("Source 3", std::move(texture), "Texture file", 2);
    auto impulse = std::make_unique<juce::TextButton>("Impulse...");
    impulse->onClick = [this] { chooseImpulseFile(); };
    impulseCell_ = addExtraCell("Room", std::move(impulse), "Dark Hall (built in)", 2);

    auto boxA = std::make_unique<juce::ComboBox>();
    boxA->setTextWhenNothingSelected("A: preset");
    fillPresetBox(*boxA);
    morphABox_ = boxA.get();
    boxA->onChange = [this] { if (morphABox_->getSelectedId() > 0) proc_.setMorphSlotFromPreset(0, morphABox_->getSelectedId() - 1); };
    addExtraCell("Morph", std::move(boxA), "A", 2);
    auto boxB = std::make_unique<juce::ComboBox>();
    boxB->setTextWhenNothingSelected("B: preset");
    fillPresetBox(*boxB);
    morphBBox_ = boxB.get();
    boxB->onChange = [this] { if (morphBBox_->getSelectedId() > 0) proc_.setMorphSlotFromPreset(1, morphBBox_->getSelectedId() - 1); };
    addExtraCell("Morph", std::move(boxB), "B", 2);
    auto setA = std::make_unique<juce::TextButton>("A <- now");
    setA->onClick = [this] { proc_.setMorphSlotFromCurrent(0); morphABox_->setSelectedId(0, juce::dontSendNotification); };
    addExtraCell("Morph", std::move(setA), "capture", 1);
    auto setB = std::make_unique<juce::TextButton>("B <- now");
    setB->onClick = [this] { proc_.setMorphSlotFromCurrent(1); morphBBox_->setSelectedId(0, juce::dontSendNotification); };
    addExtraCell("Morph", std::move(setB), "capture", 1);
}

void AmbientSynthEditor::colourCellsByGroup()
{
    for (const auto& sec : sections_) {
        const juce::Colour col = sec.group >= 0 ? groups_[static_cast<size_t>(sec.group)].colour : kMaster;
        for (int ci : sec.cells) {
            Cell& c = cells_[static_cast<size_t>(ci)];
            if (auto* sl = dynamic_cast<juce::Slider*>(c.comp.get())) sl->setColour(juce::Slider::rotarySliderFillColourId, col);
            else if (auto* tb = dynamic_cast<juce::ToggleButton*>(c.comp.get())) tb->setColour(juce::ToggleButton::tickColourId, col);
        }
    }
}

int AmbientSynthEditor::addExtraCell(const juce::String& section, std::unique_ptr<juce::Component> comp, const juce::String& label, int units)
{
    Section* sec = findSection(section);
    if (sec == nullptr) return -1;
    Cell c;
    c.units = units;
    c.baseLabel = label;
    c.label = std::make_unique<juce::Label>(juce::String(), label);
    c.label->setJustificationType(juce::Justification::centred);
    c.label->setFont(juce::FontOptions(11.0f));
    c.label->setColour(juce::Label::textColourId, kDim);
    content_.addAndMakeVisible(*c.label);
    content_.addAndMakeVisible(*comp);
    c.comp = std::move(comp);
    const int idx = static_cast<int>(cells_.size());
    sec->cells.push_back(idx);
    cells_.push_back(std::move(c));
    return idx;
}

AmbientSynthEditor::Section* AmbientSynthEditor::findSection(const juce::String& name)
{
    for (auto& s : sections_) if (s.name == name) return &s;
    return nullptr;
}

// ---------------------------------------------------------------- layout

int AmbientSynthEditor::sectionWidth(const Section& s) const
{
    int widest = 0, row = 0;
    for (int ci : s.cells) {
        const int u = cells_[static_cast<size_t>(ci)].units;
        if (row + u > s.maxUnits) row = 0;
        row += u; widest = std::max(widest, row);
    }
    return widest * kCellW + 2 * kPad;
}

int AmbientSynthEditor::sectionHeight(const Section& s) const
{
    int rows = 1, row = 0;
    for (int ci : s.cells) {
        const int u = cells_[static_cast<size_t>(ci)].units;
        if (row + u > s.maxUnits) { ++rows; row = 0; }
        row += u;
    }
    return rows * kCellH + kTitleH + kPad;
}

void AmbientSynthEditor::layoutSection(Section& s, int x, int y)
{
    s.bounds = { x, y, sectionWidth(s), sectionHeight(s) };
    int cx = x + kPad, cy = y + kTitleH, row = 0;
    for (int ci : s.cells) {
        Cell& c = cells_[static_cast<size_t>(ci)];
        if (row + c.units > s.maxUnits) { row = 0; cx = x + kPad; cy += kCellH; }
        juce::Rectangle<int> cell(cx, cy, c.units * kCellW, kCellH);
        c.label->setBounds(cell.removeFromBottom(15));
        if (dynamic_cast<juce::ComboBox*>(c.comp.get()) != nullptr) c.comp->setBounds(cell.withSizeKeepingCentre(cell.getWidth() - 6, 24));
        else if (dynamic_cast<juce::ToggleButton*>(c.comp.get()) != nullptr) c.comp->setBounds(cell.withSizeKeepingCentre(24, 24));
        else if (dynamic_cast<juce::TextButton*>(c.comp.get()) != nullptr) c.comp->setBounds(cell.withSizeKeepingCentre(cell.getWidth() - 6, 24));
        else c.comp->setBounds(cell.reduced(2, 0));
        cx += c.units * kCellW; row += c.units;
    }
}

void AmbientSynthEditor::resized()
{
    // Measure the body first: the design width is whatever it needs, so the right-hand column can
    // never fall off the edge, and the scale follows from that.
    layoutBody();
    designW_ = std::max(kMinDesignW, bodyW_);
    designH_ = std::max(kMinDesignH, kHeaderH + std::min(bodyH_, juce::roundToInt(designW_ * 0.50f)));
    scale_ = juce::jmax(0.05f, static_cast<float>(getWidth()) / static_cast<float>(designW_));
    const auto tf = juce::AffineTransform::scale(scale_);
    for (auto* child : getChildren()) child->setTransform(tf);

    header_ = juce::Rectangle<int>(0, 0, designW_, kHeaderH);
    if (soundBox_) soundBox_->setBounds(200, 8, 180, 24);
    if (cosmosBox_) cosmosBox_->setBounds(388, 8, 160, 24);
    if (saveButton_) saveButton_->setBounds(556, 8, 64, 24);
    if (loadButton_) loadButton_->setBounds(626, 8, 64, 24);
    if (recButton_) recButton_->setBounds(696, 8, 56, 24);
    if (calibButton_) calibButton_->setBounds(880, 8, 76, 24);
    if (mapButton_) mapButton_->setBounds(962, 8, 84, 24);
    if (performButton_) performButton_->setBounds(1052, 8, 70, 24);
    if (browseButton_) browseButton_->setBounds(1128, 8, 66, 24);
    if (modButton_) modButton_->setBounds(1200, 8, 52, 24);
    const int W = designW_, H = juce::roundToInt(getHeight() / scale_);
    if (perform_) perform_->setBounds(0, kHeaderH, W, H - kHeaderH);
    if (browse_) browse_->setBounds(0, kHeaderH, W, H - kHeaderH);
    modPort_.setBounds(0, kHeaderH, W, H - kHeaderH);
    if (mod_) {
        // Eight LFO rows, six envelope rows and the matrix: a fixed content height, so the rows
        // keep a size at which the curves can be read.
        const int need = 14 + ambient::kNumLfos * 100 + 14;
        mod_->setBounds(0, 0, juce::jmax(600, modPort_.getMaximumVisibleWidth()),
                        juce::jmax(need, modPort_.getMaximumVisibleHeight()));
    }
    routing_ = { 12, 40, 900, kHeaderH - 46 };
    const int scopeX = 930, scopeR = W - 300;
    if (scope_) scope_->setBounds(scopeX, 38, juce::jmax(160, scopeR - scopeX), kHeaderH - 46);
    keys_ = { scopeX, kHeaderH - 12, juce::jmax(160, scopeR - scopeX), 7 };
    if (master_) master_->setBounds(W - 74, 6, 66, 52);

    // The Master section (mid/side) sits in the header, left of the master knob.
    if (Section* ms = findSection("Master")) {
        ms->maxUnits = 8;
        layoutSection(*ms, W - 74 - sectionWidth(*ms) - 6, 4);
        ms->bounds = ms->bounds.withTrimmedTop(-2);
    }

    // Everything else scrolls below the header.
    viewport_.setBounds(0, kHeaderH, W, H - kHeaderH);

    content_.setSize(std::max(bodyW_, viewport_.getMaximumVisibleWidth()),
                     std::max(bodyH_, viewport_.getMaximumVisibleHeight()));
}

// Places every group and section, and records how much room the whole body needs. The design
// size is measured from this once, in the constructor -- guessing it meant the right-hand column
// kept falling off the edge.
void AmbientSynthEditor::layoutBody()
{
    int colWidth[2] = { 0, 0 };
    for (auto& g : groups_) {
        for (auto& row : g.rows) {
            int w = kPad;
            for (auto& n : row) if (Section* s = findSection(n)) w += sectionWidth(*s) + kPad;
            colWidth[g.column] = std::max(colWidth[g.column], w);
        }
    }
    int colX[2] = { kPad, kPad + colWidth[0] + kPad };
    int colY[2] = { kPad, kPad };
    for (auto& g : groups_) {
        const int x0 = colX[g.column];
        int y = colY[g.column] + kGroupTitleH;
        for (auto& row : g.rows) {
            int x = x0 + kPad, rowH = 0;
            for (auto& n : row) {
                Section* s = findSection(n);
                if (s == nullptr) continue;
                layoutSection(*s, x, y);
                x += s->bounds.getWidth() + kPad;
                rowH = std::max(rowH, s->bounds.getHeight());
            }
            y += rowH + kPad;
        }
        g.bounds = { x0, colY[g.column], colWidth[g.column], y - colY[g.column] };
        colY[g.column] = y + kPad;
    }
    bodyW_ = colX[1] + colWidth[1] + kPad;
    bodyH_ = std::max(colY[0], colY[1]);
}

// ---------------------------------------------------------------- perform page

void AmbientSynthEditor::setPerforming(bool on) { setPage(on ? 1 : 0); }

void AmbientSynthEditor::setPage(int page)
{
    performing_ = page == 1;
    viewport_.setVisible(page == 0);
    perform_->setVisible(page == 1);
    browse_->setVisible(page == 2);
    modPort_.setVisible(page == 3);
    if (performButton_->getToggleState() != (page == 1)) performButton_->setToggleState(page == 1, juce::dontSendNotification);
    if (browseButton_->getToggleState() != (page == 2)) browseButton_->setToggleState(page == 2, juce::dontSendNotification);
    if (modButton_ && modButton_->getToggleState() != (page == 3)) modButton_->setToggleState(page == 3, juce::dontSendNotification);
    if (page == 2) browse_->applyFilter();
    if (page == 3 && mod_) mod_->pullMatrix();
}

// ---------------------------------------------------------------- modulation page

AmbientSynthEditor::ModView::ModView(AmbientSynthProcessor& p) : proc(p)
{
    auto addKnob = [this](Row& row, const char* key, const juce::String& name) {
        const ParamDesc* d = findParam(key);
        if (d == nullptr) return;
        if (d->kind == ParamKind::Choice) {
            auto cb = std::make_unique<juce::ComboBox>();
            for (int i = 0; i < d->numChoices; ++i) cb->addItem(d->choices[i], i + 1);
            addAndMakeVisible(*cb);
            row.combos.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, key, *cb));
            row.controls.push_back(std::move(cb));
        } else {
            auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            if (d->unit[0] != 0) s->setTextValueSuffix(juce::String(" ") + d->unit);
            s->setColour(juce::Slider::rotarySliderFillColourId, ui::accent);
            addAndMakeVisible(*s);
            row.sliders.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, key, *s));
            row.controls.push_back(std::move(s));
        }
        auto l = std::make_unique<juce::Label>(juce::String(), name);
        l->setJustificationType(juce::Justification::centred);
        l->setFont(ui::body(10.5f));
        l->setColour(juce::Label::textColourId, ui::dim);
        addAndMakeVisible(*l);
        row.labels.push_back(std::move(l));
    };

    for (int i = 0; i < ambient::kNumLfos; ++i) {
        Row& r = lfos[static_cast<size_t>(i)];
        r.title = "LFO " + juce::String(i + 1);
        const juce::String n(i + 1);
        addKnob(r, ("lfo" + n + "_shape").toRawUTF8(), "Shape");
        addKnob(r, ("lfo" + n + "_rate").toRawUTF8(), "Rate");
        addKnob(r, ("lfo" + n + "_phase").toRawUTF8(), "Phase");
        addKnob(r, ("lfo" + n + "_depth").toRawUTF8(), "Depth");
        addKnob(r, ("lfo" + n + "_mode").toRawUTF8(), "Mode");
        addKnob(r, ("lfo" + n + "_table").toRawUTF8(), "Table");
    }
    for (int i = 0; i < ambient::kNumModEnvs; ++i) {
        Row& r = envs[static_cast<size_t>(i)];
        r.title = "ENV " + juce::String(i + 1);
        const juce::String n(i + 1);
        addKnob(r, ("env" + n + "_mode").toRawUTF8(), "Mode");
        addKnob(r, ("env" + n + "_time").toRawUTF8(), "Time");
        addKnob(r, ("env" + n + "_depth").toRawUTF8(), "Depth");
    }

    matrixText.setMultiLine(true, false);
    matrixText.setReturnKeyStartsNewLine(true);
    matrixText.setFont(ui::body(12.0f));
    addAndMakeVisible(matrixText);
    applyMatrix.onClick = [this] { pushMatrix(); };
    clearMatrix.onClick = [this] { matrixText.setText({}, false); pushMatrix(); };
    addAndMakeVisible(applyMatrix);
    addAndMakeVisible(clearMatrix);
    matrixInfo.setFont(ui::body(11.5f));
    matrixInfo.setColour(juce::Label::textColourId, ui::dim);
    addAndMakeVisible(matrixInfo);
    hint.setText("source > target : depth [: via] [: u]     one per line, e.g.  lfo1 > cutoff : 0.25",
                 juce::dontSendNotification);
    hint.setFont(ui::body(11.5f));
    hint.setColour(juce::Label::textColourId, ui::faint);
    addAndMakeVisible(hint);
    pullMatrix();
    startTimerHz(30);
}

void AmbientSynthEditor::ModView::pullMatrix()
{
    char buf[4096];
    const int n = proc.engine().writeModMatrix(buf, sizeof(buf));
    juce::String t(juce::CharPointer_UTF8(buf), static_cast<size_t>(juce::jmax(0, n)));
    matrixText.setText(t.replace(";", "\n"), false);
    matrixInfo.setText(juce::String(proc.engine().modMatrix().count()) + " of 32 routes",
                       juce::dontSendNotification);
}

void AmbientSynthEditor::ModView::pushMatrix()
{
    const juce::String t = matrixText.getText().replaceCharacters("\n", ";").removeCharacters(" ");
    if (proc.engine().setModMatrixText(t.toRawUTF8())) {
        matrixInfo.setColour(juce::Label::textColourId, ui::dim);
        matrixInfo.setText(juce::String(proc.engine().modMatrix().count()) + " of 32 routes",
                           juce::dontSendNotification);
    } else {
        matrixInfo.setColour(juce::Label::textColourId, juce::Colour(0xffd08a8a));
        matrixInfo.setText("a line could not be read; the previous matrix is kept", juce::dontSendNotification);
    }
}

void AmbientSynthEditor::ModView::timerCallback()
{
    if (isShowing()) repaint();
}

ambient::LfoSpec AmbientSynthEditor::ModView::specOf(int i) const
{
    const juce::String n(i + 1);
    auto get = [this](const juce::String& key) {
        auto* v = proc.apvts.getRawParameterValue(key);
        return v != nullptr ? v->load() : 0.0f;
    };
    ambient::LfoSpec sp;
    sp.shape  = static_cast<ambient::LfoShape>(juce::jlimit(0, ambient::kNumLfoShapes - 1,
                    static_cast<int>(std::lround(get("lfo" + n + "_shape")))));
    sp.rateHz = get("lfo" + n + "_rate");
    sp.phase  = get("lfo" + n + "_phase");
    sp.depth  = get("lfo" + n + "_depth");
    sp.table  = static_cast<int>(std::lround(get("lfo" + n + "_table")));
    return sp;
}

namespace {
void curveFrame(juce::Graphics& g, juce::Rectangle<int> r, bool lit)
{
    g.setColour(ui::bg0.withAlpha(0.6f));
    g.fillRoundedRectangle(r.toFloat(), 5.0f);
    g.setColour(lit ? ui::accent.withAlpha(0.35f) : ui::cardEdge);
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 5.0f, 1.0f);
    g.setColour(ui::track.withAlpha(0.7f));
    g.drawLine(static_cast<float>(r.getX() + 4), r.toFloat().getCentreY(),
               static_cast<float>(r.getRight() - 4), r.toFloat().getCentreY(), 1.0f);
}
}

void AmbientSynthEditor::ModView::paintLfo(juce::Graphics& g, int i)
{
    const Row& row = lfos[static_cast<size_t>(i)];
    const auto r = row.curve;
    if (r.isEmpty()) return;
    const ambient::LfoSpec sp = specOf(i);
    const bool lit = sp.depth > 0.001f;
    curveFrame(g, r, lit);

    // One cycle of the shape. Random and Steps need the live oscillator, or the picture would be
    // a shape the instrument is not actually playing.
    const ambient::Lfo& live = proc.engine().lfo(i);
    juce::Path p;
    const float x0 = static_cast<float>(r.getX() + 5), w = static_cast<float>(r.getWidth() - 10);
    const float cy = r.toFloat().getCentreY(), h = r.getHeight() * 0.38f;
    const int steps = juce::jlimit(64, 400, r.getWidth());
    for (int s = 0; s <= steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps);
        const float v = ambient::Lfo::shapeAt(sp, t + sp.phase, nullptr, &live) * sp.depth;
        const float y = cy - juce::jlimit(-1.0f, 1.0f, v) * h;
        if (s == 0) p.startNewSubPath(x0 + t * w, y); else p.lineTo(x0 + t * w, y);
    }
    g.setColour(ui::accent.withAlpha(lit ? 0.22f : 0.08f));
    g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(lit ? ui::accent : ui::faint);
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // The running dot: where the oscillator is right now.
    const float ph = proc.engine().lfoPhase(i);
    const float v = proc.engine().modSource(static_cast<int>(ambient::ModSource::Lfo1) + i);
    const float dx = x0 + ph * w, dy = cy - juce::jlimit(-1.0f, 1.0f, v) * h;
    g.setColour(ui::live.withAlpha(0.30f));
    g.fillEllipse(dx - 7.0f, dy - 7.0f, 14.0f, 14.0f);
    g.setColour(ui::live);
    g.fillEllipse(dx - 3.0f, dy - 3.0f, 6.0f, 6.0f);

    // Period, because "0.004 Hz" tells nobody anything.
    const float hz = juce::jmax(1.0e-5f, sp.rateHz);
    const float period = 1.0f / hz;
    const juce::String txt = period >= 90.0f ? juce::String(period / 60.0f, 1) + " min"
                                             : juce::String(period, period < 10.0f ? 2 : 1) + " s";
    g.setColour(ui::dim);
    g.setFont(ui::body(10.5f));
    g.drawText(txt, r.reduced(8, 4), juce::Justification::topRight, false);
    g.setColour(lit ? ui::text : ui::faint);
    g.setFont(ui::title(11.0f));
    g.drawText(row.title, r.reduced(8, 4), juce::Justification::topLeft, false);
}

void AmbientSynthEditor::ModView::paintEnv(juce::Graphics& g, int i)
{
    const Row& row = envs[static_cast<size_t>(i)];
    const auto r = row.curve;
    if (r.isEmpty()) return;
    const ambient::ModEnv& e = proc.engine().envShape(i);
    const juce::String n(i + 1);
    auto get = [this](const juce::String& key) {
        auto* v = proc.apvts.getRawParameterValue(key);
        return v != nullptr ? v->load() : 0.0f;
    };
    const float depth = get("env" + n + "_depth");
    const float scale = juce::jmax(0.01f, get("env" + n + "_time"));
    const auto mode = static_cast<ambient::EnvMode>(juce::jlimit(0, ambient::kNumEnvModes - 1,
                          static_cast<int>(std::lround(get("env" + n + "_mode")))));
    const bool lit = depth > 0.001f && e.count() > 1;
    curveFrame(g, r, lit);

    const float x0 = static_cast<float>(r.getX() + 5), w = static_cast<float>(r.getWidth() - 10);
    const float cy = r.toFloat().getCentreY(), h = r.getHeight() * 0.38f;
    const float len = juce::jmax(0.001f, e.length());

    // The loop region as a lighter band, so a looping shape reads as one at a glance.
    if (e.loopFrom() >= 0 && e.loopTo() > e.loopFrom() && e.loopTo() < e.count()) {
        const float a = e.point(e.loopFrom()).time / len, b = e.point(e.loopTo()).time / len;
        g.setColour(ui::accent.withAlpha(0.07f));
        g.fillRect(x0 + a * w, static_cast<float>(r.getY() + 4), (b - a) * w, static_cast<float>(r.getHeight() - 8));
    }

    juce::Path p;
    const int steps = juce::jlimit(64, 400, r.getWidth());
    for (int s = 0; s <= steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps) * len;
        const float v = e.at(t, ambient::EnvMode::OneShot, true) * depth;
        const float y = cy - juce::jlimit(-1.0f, 1.0f, v) * h;
        const float x = x0 + (t / len) * w;
        if (s == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    g.setColour(ui::foreCol.withAlpha(lit ? 0.22f : 0.08f));
    g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(lit ? ui::foreCol : ui::faint);
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Breakpoints, and the sustain point as a ring.
    for (int k = 0; k < e.count(); ++k) {
        const ambient::EnvPoint& pt = e.point(k);
        const float x = x0 + (pt.time / len) * w;
        const float y = cy - juce::jlimit(-1.0f, 1.0f, pt.value * depth) * h;
        g.setColour(lit ? ui::foreCol.withAlpha(0.8f) : ui::faint);
        g.fillEllipse(x - 2.5f, y - 2.5f, 5.0f, 5.0f);
        if (k == e.sustain()) { g.setColour(ui::live); g.drawEllipse(x - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.4f); }
    }

    // Where the envelope is now, on its phrase clock.
    const float t = proc.engine().envTime() / scale;
    if (lit && t <= len * 1.02f) {
        const float v = proc.engine().modSource(static_cast<int>(ambient::ModSource::Env1) + i);
        const float dx = x0 + juce::jlimit(0.0f, 1.0f, t / len) * w;
        const float dy = cy - juce::jlimit(-1.0f, 1.0f, v) * h;
        g.setColour(ui::live.withAlpha(0.30f));
        g.fillEllipse(dx - 7.0f, dy - 7.0f, 14.0f, 14.0f);
        g.setColour(ui::live);
        g.fillEllipse(dx - 3.0f, dy - 3.0f, 6.0f, 6.0f);
    }

    g.setColour(ui::dim);
    g.setFont(ui::body(10.5f));
    g.drawText(juce::String(len * scale, len * scale < 10.0f ? 1 : 0) + " s   "
                   + juce::String(ambient::kEnvModeNames[static_cast<int>(mode)]),
               r.reduced(8, 4), juce::Justification::topRight, false);
    g.setColour(lit ? ui::text : ui::faint);
    g.setFont(ui::title(11.0f));
    g.drawText(row.title, r.reduced(8, 4), juce::Justification::topLeft, false);
}

void AmbientSynthEditor::ModView::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg0);
    for (int i = 0; i < ambient::kNumLfos; ++i) paintLfo(g, i);
    for (int i = 0; i < ambient::kNumModEnvs; ++i) paintEnv(g, i);
    g.setColour(ui::text);
    g.setFont(ui::title(12.0f));
    g.drawText("MATRIX", matrixText.getX(), matrixText.getY() - 20, 200, 16, juce::Justification::centredLeft);
}

void AmbientSynthEditor::ModView::resized()
{
    auto area = getLocalBounds().reduced(14);
    auto left = area.removeFromLeft(area.getWidth() * 55 / 100);
    area.removeFromLeft(14);

    auto layoutRow = [](Row& row, juce::Rectangle<int> r, int curveWidth) {
        row.curve = r.removeFromLeft(curveWidth);
        r.removeFromLeft(10);
        const int n = juce::jmax(1, static_cast<int>(row.controls.size()));
        const int cw = r.getWidth() / n;
        for (int k = 0; k < n; ++k) {
            auto cell = r.removeFromLeft(cw);
            row.labels[static_cast<size_t>(k)]->setBounds(cell.removeFromBottom(13));
            if (dynamic_cast<juce::ComboBox*>(row.controls[static_cast<size_t>(k)].get()) != nullptr)
                row.controls[static_cast<size_t>(k)]->setBounds(cell.withSizeKeepingCentre(cell.getWidth() - 4, 22));
            else
                row.controls[static_cast<size_t>(k)]->setBounds(cell.reduced(3, 1));
        }
    };

    const int lfoH = 94;
    for (int i = 0; i < ambient::kNumLfos; ++i) {
        auto r = left.removeFromTop(lfoH);
        left.removeFromTop(6);
        layoutRow(lfos[static_cast<size_t>(i)], r, juce::jmax(120, r.getWidth() * 40 / 100));
    }

    auto matrix = area.removeFromBottom(juce::jmax(170, area.getHeight() - 6 * 100));
    const int envH = 94;
    for (int i = 0; i < ambient::kNumModEnvs; ++i) {
        auto r = area.removeFromTop(envH);
        area.removeFromTop(6);
        layoutRow(envs[static_cast<size_t>(i)], r, juce::jmax(120, r.getWidth() * 52 / 100));
    }

    matrix.removeFromTop(24);
    auto bottom = matrix.removeFromBottom(26);
    hint.setBounds(matrix.removeFromBottom(18));
    matrixText.setBounds(matrix);
    applyMatrix.setBounds(bottom.removeFromLeft(74));
    bottom.removeFromLeft(8);
    clearMatrix.setBounds(bottom.removeFromLeft(74));
    bottom.removeFromLeft(12);
    matrixInfo.setBounds(bottom);
}

// ---------------------------------------------------------------- browse page

namespace {
const juce::Colour kFamilyColours[] = {
    juce::Colour(0xff7fb3d5), juce::Colour(0xff8ec9a8), juce::Colour(0xffe0c070), juce::Colour(0xffd08a8a),
    juce::Colour(0xffb094d8), juce::Colour(0xff70c8c8), juce::Colour(0xffe09a60), juce::Colour(0xffa0b8e0),
    juce::Colour(0xffc8d870), juce::Colour(0xffd880b8), juce::Colour(0xff90d0f0), juce::Colour(0xffd0d0d0),
};
// The twelve built-in families keep their colours; loaded packs get their own hues, spaced by
// the golden angle so neighbouring packs never look alike.
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
    for (auto* b : { &load, &toA, &toB, &star }) addAndMakeVisible(*b);
    addAndMakeVisible(onlyFavourites);

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
    onlyFavourites.setBounds(top.removeFromLeft(140));
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

juce::Point<float> AmbientSynthEditor::BrowseView::MapView::toScreen(float x, float y) const
{
    const auto r = getLocalBounds().toFloat().reduced(18.0f);
    return { r.getX() + r.getWidth() * x, r.getBottom() - r.getHeight() * y };
}

juce::Point<float> AmbientSynthEditor::BrowseView::MapView::toMap(juce::Point<float> s) const
{
    const auto r = getLocalBounds().toFloat().reduced(18.0f);
    return { juce::jlimit(0.0f, 1.0f, (s.x - r.getX()) / juce::jmax(1.0f, r.getWidth())),
             juce::jlimit(0.0f, 1.0f, (r.getBottom() - s.y) / juce::jmax(1.0f, r.getHeight())) };
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
        const float rr = rad * r.getWidth();
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
    const float dotScale = juce::jlimit(0.34f, 1.0f, std::sqrt(200.0f / juce::jmax(1, shown)));
    const bool many = shown > 800;
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < shown; ++i) {
            if (inFilter[static_cast<size_t>(i)] != (pass == 1)) continue;
            const PresetMeta& m = presetMeta(i);
            const auto s = toScreen(m.x, m.y);
            const float size = juce::jmax(2.0f, (6.0f + 6.0f * m.density) * dotScale);
            juce::Colour c = familyColour(m.family);
            if (pass == 0) c = c.withAlpha(0.18f);
            g.setColour(c);
            if (pass == 0 && many) g.fillRect(s.x - size / 2, s.y - size / 2, size, size);
            else                   g.fillEllipse(s.x - size / 2, s.y - size / 2, size, size);
            if (i == owner.selected || i == current) { g.setColour(kText); g.drawEllipse(s.x - size / 2 - 3, s.y - size / 2 - 3, size + 6, size + 6, 1.5f); }
        }
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
        g.drawText(juce::String(presetFamilyName(m.family)) + "  ·  " + tags.trim(), static_cast<int>(s.x) + 10, static_cast<int>(s.y) - 6, 360, 14, juce::Justification::centredLeft);
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
    const int h = nearestPreset(e.position, 10.0f);
    if (h >= 0 && !owner.mapActive.getToggleState()) {   // plain click on a point loads it
        owner.selected = h; owner.proc.setCurrentProgram(h); owner.list.repaint(); repaint(); return;
    }
    mouseDrag(e);
}

void AmbientSynthEditor::BrowseView::MapView::mouseDrag(const juce::MouseEvent& e)
{
    dragging = true;
    if (!owner.mapActive.getToggleState()) return;
    const auto m = toMap(e.position);
    if (auto* px = owner.proc.apvts.getParameter(paramDesc(ParamId::MapX).key)) px->setValueNotifyingHost(m.x);
    if (auto* py = owner.proc.apvts.getParameter(paramDesc(ParamId::MapY).key)) py->setValueNotifyingHost(m.y);
    repaint();
}

void AmbientSynthEditor::BrowseView::MapView::mouseUp(const juce::MouseEvent&) { dragging = false; }

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

// ---------------------------------------------------------------- interaction

void AmbientSynthEditor::mouseDown(const juce::MouseEvent& e)
{
    if (!e.mods.isPopupMenu()) return;
    auto it = cellOf_.find(e.eventComponent);
    if (it == cellOf_.end()) return;
    const int idx = it->second;
    const int param = cells_[static_cast<size_t>(idx)].param;
    if (param < 0) return;
    const ParamId id = static_cast<ParamId>(param);
    const int cc = proc_.midiCcFor(id);
    juce::PopupMenu menu;
    menu.addItem(1, proc_.learnTarget() == param ? "Learning... move a controller" : "MIDI Learn", proc_.learnTarget() != param);
    if (cc >= 0) menu.addItem(2, "Clear MIDI (CC " + juce::String(cc) + ")");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(e.eventComponent), [this, id](int result) {
        if (result == 1) proc_.armMidiLearn(id);
        else if (result == 2) proc_.clearMidiLearn(id);
    });
}

void AmbientSynthEditor::timerCallback()
{
    proc_.engine().soundingNotes(sounding_);
    if (mapOpen_) {
        if (mapEditor_ != nullptr) mapText_ = mapEditor_->getText();
        else {
            mapOpen_ = false;
            if (!proc_.setGestureMappings(mapText_))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Mappings", "A line could not be parsed; the previous table is kept.");
        }
    }
    if (soundBox_ && soundBox_->getSelectedId() != proc_.soundPresetIndex() + 1)
        soundBox_->setSelectedId(proc_.soundPresetIndex() + 1, juce::dontSendNotification);
    if (cosmosBox_ && cosmosBox_->getSelectedId() != proc_.cosmosPresetIndex() + 1)
        cosmosBox_->setSelectedId(proc_.cosmosPresetIndex() + 1, juce::dontSendNotification);
    const int learn = proc_.learnTarget();
    for (auto& c : cells_) {
        if (c.param < 0) continue;
        const int cc = proc_.midiCcFor(static_cast<ParamId>(c.param));
        juce::String text = c.baseLabel;
        if (learn == c.param) text += " · learn";
        else if (cc >= 0) text += " · CC" + juce::String(cc);
        if (c.label->getText() != text) {
            c.label->setText(text, juce::dontSendNotification);
            c.label->setColour(juce::Label::textColourId, (cc >= 0 || learn == c.param) ? kAccent : kDim);
        }
    }
    updateSourceCells();
    repaint(header_);
    if (Section* m = findSection("Morph")) content_.repaint(m->bounds.withTrimmedTop(-kGroupTitleH));
}

int AmbientSynthEditor::cellForParam(ParamId id) const
{
    for (int i = 0; i < static_cast<int>(cells_.size()); ++i) if (cells_[static_cast<size_t>(i)].param == static_cast<int>(id)) return i;
    return -1;
}

void AmbientSynthEditor::updateSourceCells()
{
    // The 17 slot parameters are laid out identically for Source 2 and Source 3 (see Params.h):
    // 0 type 1 level 2 octave 3 ratio 4 pan 5 table 6 position 7 pos drift 8 fm ratio 9 fm index
    // 10 grain 11 density 12 follow 13 grains 14 spread 15 noise 16 noise q.
    // Grey out what the chosen type ignores.
    enum { Off = 0, Table = 1, Fm = 2, Texture = 3, Noise = 4 };
    const ParamId first[2] = { ParamId::Src2Type, ParamId::Src3Type };
    for (int k = 0; k < 2; ++k) {
        const int type = static_cast<int>(std::lround(proc_.engine().getParam(first[k])));
        for (int off = 1; off <= 16; ++off) {
            bool on = type != Off;
            switch (off) {
            case 5:  on = type == Table; break;                                  // wavetable choice
            case 6:  on = type == Table || type == Texture || type == Noise; break;   // position / centre
            case 7:  on = type != Off;  break;                                   // pos drift moves all of them
            case 8:
            case 9:  on = type == Fm; break;
            case 10:
            case 13:
            case 14: on = type == Texture; break;                                // grain, grains, spread
            case 11: on = type == Texture || type == Noise; break;               // density: grains or crackle
            case 12: on = type == Texture || type == Noise; break;               // pitch follow
            case 15:
            case 16: on = type == Noise; break;
            default: break;
            }
            const int ci = cellForParam(static_cast<ParamId>(static_cast<int>(first[k]) + off));
            if (ci < 0) continue;
            Cell& c = cells_[static_cast<size_t>(ci)];
            if (c.comp->isEnabled() != on) { c.comp->setEnabled(on); c.comp->setAlpha(on ? 1.0f : 0.35f); c.label->setAlpha(on ? 1.0f : 0.35f); }
        }
    }
    auto nameCell = [&](int ci, const juce::String& base, const juce::String& file) {
        if (ci < 0) return;
        Cell& c = cells_[static_cast<size_t>(ci)];
        const juce::String text = file.isNotEmpty() ? file : base;
        if (c.label->getText() != text) c.label->setText(text, juce::dontSendNotification);
    };
    nameCell(tableCell_, "User table", proc_.wavetableName());
    nameCell(textureCell_, "Texture file", proc_.textureName());
    nameCell(impulseCell_, "Dark Hall (built in)", proc_.impulseName());
}

void AmbientSynthEditor::showMappingEditor()
{
    // A plain text editor over the mapping table: one line per mapping,
    //   input param min max smooth deadzone clutch invert
    auto* editor = new juce::TextEditor();
    editor->setMultiLine(true, false);
    editor->setReturnKeyStartsNewLine(true);
    editor->setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain));
    editor->setText(proc_.gestureMappings(), false);
    editor->setSize(640, 420);
    auto* content = new juce::Component();
    content->setSize(640, 470);
    content->addAndMakeVisible(editor);
    editor->setBounds(0, 0, 640, 420);
    auto* hint = new juce::Label(juce::String(), "input param min max [smooth] [deadzone] [clutch|none] [invert]   inputs: HandDistance LeftHeight RightHeight LeftForward RightForward LeftTilt RightTilt LeftPinch RightPinch HeadYaw HeadPitch HeadRoll Custom0..7 (= macros A..H)");
    hint->setFont(juce::FontOptions(11.0f));
    hint->setColour(juce::Label::textColourId, kDim);
    hint->setBounds(0, 424, 640, 44);
    hint->setMinimumHorizontalScale(0.5f);
    content->addAndMakeVisible(hint);
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Gesture and macro mappings";
    o.content.setOwned(content);
    o.componentToCentreAround = this;
    o.dialogBackgroundColour = kGroupFill;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
    // The timer mirrors the text while the dialog lives and applies it once the dialog is gone.
    mapEditor_ = editor;
    mapText_ = editor->getText();
    mapOpen_ = true;
}

void AmbientSynthEditor::chooseSourceFile(bool wavetable)
{
    chooser_ = std::make_unique<juce::FileChooser>(wavetable ? "Load a wavetable (2048-sample frames)" : "Load a texture sample",
                                                   juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, wavetable](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            const bool ok = wavetable ? proc_.loadWavetableFile(file) : proc_.loadTextureFile(file);
            if (!ok)
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, wavetable ? "Wavetable" : "Texture",
                    wavetable ? "Could not read this file as a wavetable (it needs at least one 2048-sample frame)." : "Could not read this audio file.");
            repaint();
        });
}

void AmbientSynthEditor::chooseImpulseFile()
{
    chooser_ = std::make_unique<juce::FileChooser>("Load an impulse response (mono or stereo)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            if (!proc_.loadImpulseFile(file))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Impulse", "Could not read this audio file.");
            repaint();
        });
}

void AmbientSynthEditor::chooseScalaFile()
{
    chooser_ = std::make_unique<juce::FileChooser>("Load a Scala scale", juce::File(), "*.scl");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            if (!proc_.loadScalaText(file.loadFileAsString(), file.getFileNameWithoutExtension()))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Scala", "This file is not a valid .scl scale.");
            repaint();
        });
}

// ---------------------------------------------------------------- painting

void AmbientSynthEditor::paintRoutingMap(juce::Graphics& g, juce::Rectangle<int> area)
{
    // Node boxes in header coordinates; arrows follow the real signal flow.
    struct Node { juce::String text; juce::Colour col; juce::Rectangle<float> r; };
    const float y0 = static_cast<float>(area.getY()), h = 20.0f;
    const float x = static_cast<float>(area.getX());
    std::vector<Node> nodes = {
        { "Brain / MIDI",               kConductor, { x,         y0,        92.0f, h } },
        { "Voices",                     kVoice,     { x + 108,   y0,        60.0f, h } },
        { "Ensemble > Delay > Delay 2 > Near", kFore, { x + 184, y0,        208.0f, h } },
        { "Mid/Side > Out",             kMaster,    { x + 760,   y0,        110.0f, h } },
        { "Cosmos (parallel, returns)", kCosmos,    { x + 184,   y0 + 32,   170.0f, h } },
        { "Cloud",                      kBack,      { x + 410,   y0 + 32,   50.0f, h } },
        { "Far Reverb + Shimmer",       kBack,      { x + 476,   y0 + 32,   150.0f, h } },
        { "Morph A<->B",                kMorph,     { x + 660,   y0 + 32,   84.0f, h } },
    };
    auto arrow = [&](juce::Point<float> a, juce::Point<float> b, juce::Colour c) {
        g.setColour(c.withAlpha(0.8f));
        g.drawLine(juce::Line<float>(a, b), 1.2f);
        const auto d = (b - a); const float len = std::max(d.getDistanceFromOrigin(), 1.0f);
        const juce::Point<float> u(d.x / len, d.y / len), n(-u.y, u.x);
        juce::Path p; p.startNewSubPath(b); p.lineTo(b - u * 6.0f + n * 3.0f); p.lineTo(b - u * 6.0f - n * 3.0f); p.closeSubPath();
        g.fillPath(p);
    };
    auto right = [](const juce::Rectangle<float>& r) { return juce::Point<float>(r.getRight(), r.getCentreY()); };
    auto left  = [](const juce::Rectangle<float>& r) { return juce::Point<float>(r.getX(), r.getCentreY()); };
    auto bottom = [](const juce::Rectangle<float>& r, float fx) { return juce::Point<float>(r.getX() + r.getWidth() * fx, r.getBottom()); };
    auto top = [](const juce::Rectangle<float>& r, float fx) { return juce::Point<float>(r.getX() + r.getWidth() * fx, r.getY()); };

    arrow(right(nodes[0].r), left(nodes[1].r), kConductor);
    arrow(right(nodes[1].r), left(nodes[2].r), kVoice);
    arrow(right(nodes[2].r), left(nodes[3].r), kFore);                                  // foreground -> out
    arrow(bottom(nodes[2].r, 0.15f), top(nodes[4].r, 0.15f), kFore);                    // send to cosmos
    arrow(top(nodes[4].r, 0.5f), bottom(nodes[2].r, 0.5f), kCosmos);                    // cosmos return
    arrow(bottom(nodes[2].r, 0.9f), top(nodes[5].r, 0.3f), kFore);                      // cloud send
    arrow(right(nodes[5].r), left(nodes[6].r), kBack);
    arrow(bottom(nodes[1].r, 0.7f), top(nodes[6].r, 0.1f), kVoice);                     // far send of the voices
    arrow(right(nodes[4].r), left(nodes[6].r), kCosmos);                                // cosmos to far
    arrow(right(nodes[6].r), juce::Point<float>(nodes[3].r.getCentreX(), nodes[3].r.getBottom()), kBack);   // far -> out
    {   // feedback: the mix returns to the voices (bus and phase modulation), drawn between the rows
        const float yf = y0 + h + 6.0f;
        const juce::Point<float> a(nodes[3].r.getX() + 8.0f, yf), b(nodes[1].r.getCentreX() + 12.0f, yf);
        g.setColour(kBack.withAlpha(0.6f));
        g.drawLine(juce::Line<float>(nodes[3].r.getX() + 8.0f, nodes[3].r.getBottom(), a.x, a.y), 1.0f);
        arrow(a, b, kBack.withAlpha(0.7f));
        g.setFont(juce::FontOptions(9.0f));
        g.drawText("feedback", static_cast<int>(b.x) + 30, static_cast<int>(yf) - 6, 60, 12, juce::Justification::centredLeft);
    }
    for (auto& n : nodes) {
        g.setColour(n.col.withAlpha(0.18f));
        g.fillRoundedRectangle(n.r, 4.0f);
        g.setColour(n.col);
        g.drawRoundedRectangle(n.r, 4.0f, 1.0f);
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(n.text, n.r, juce::Justification::centred);
    }
    g.setColour(kDim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("signal flow", area.getX(), area.getBottom() - 12, 80, 12, juce::Justification::centredLeft);
}

// ---------------------------------------------------------------- oscillator scope

void AmbientSynthEditor::ScopeView::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(ui::card.withAlpha(0.55f));
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(ui::cardEdge);
    g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);

    float live[ambient::kMaxPartials] = {};
    const int n = proc.engine().displayPartials(live, ambient::kMaxPartials);
    count = juce::jmax(count, n);
    // Glide towards the engine's values: at 30 Hz the raw numbers would flicker, and the point is
    // to see the slow breathing, not the control-block edges.
    for (int i = 0; i < ambient::kMaxPartials; ++i) {
        const float target = i < n ? std::abs(live[i]) : 0.0f;
        amp[i] += (target - amp[i]) * 0.25f;
    }
    float peak = 1.0e-6f;
    for (int i = 0; i < count; ++i) peak = juce::jmax(peak, amp[i]);
    const bool silent = peak < 1.0e-4f;

    const auto plot = r.reduced(9.0f, 7.0f);
    if (silent) {
        g.setColour(ui::faint);
        g.setFont(ui::body(11.0f));
        g.drawText("oscillator", plot, juce::Justification::centred, false);
        g.setColour(ui::track);
        g.drawLine(plot.getX(), plot.getCentreY(), plot.getRight(), plot.getCentreY(), 1.0f);
        return;
    }

    // Spectrum behind: one thin bar per partial, on a decibel scale.
    {
        const float bw = juce::jmin(9.0f, plot.getWidth() / static_cast<float>(juce::jmax(count, 1)));
        for (int i = 0; i < count; ++i) {
            const float a = amp[i] / peak;
            if (a < 1.0e-4f) continue;
            const float db = juce::jlimit(0.0f, 1.0f, 1.0f + std::log10(a) / 2.5f);   // -50 dB .. 0
            const float h = db * plot.getHeight() * 0.9f;
            g.setColour(ui::accent.withAlpha(0.10f + 0.13f * db));
            g.fillRect(plot.getX() + i * bw + 0.5f, plot.getBottom() - h, juce::jmax(1.0f, bw - 1.2f), h);
        }
    }

    // One cycle of the wave those partials make. The phases are fixed (golden angle) so the shape
    // is characteristic and readable; what moves is the amplitudes, which is what actually moves.
    juce::Path wave;
    const int steps = juce::jlimit(96, 512, static_cast<int>(plot.getWidth()));
    const float mid = plot.getCentreY(), half = plot.getHeight() * 0.42f;
    for (int x = 0; x <= steps; ++x) {
        const float t = static_cast<float>(x) / static_cast<float>(steps);
        float v = 0.0f;
        for (int i = 0; i < count; ++i) {
            if (amp[i] < 1.0e-5f) continue;
            const float ph = 0.381966f * static_cast<float>(i);
            v += amp[i] * std::sin(juce::MathConstants<float>::twoPi * ((i + 1) * t + ph));
        }
        const float y = mid - juce::jlimit(-1.0f, 1.0f, v / (peak * 2.2f)) * half;
        const float px = plot.getX() + t * plot.getWidth();
        if (x == 0) wave.startNewSubPath(px, y); else wave.lineTo(px, y);
    }
    g.setColour(ui::accent.withAlpha(0.20f));
    g.strokePath(wave, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ui::accent);
    g.strokePath(wave, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float hz = proc.engine().displayFrequency();
    if (hz > 0.0f) {
        g.setColour(ui::dim);
        g.setFont(ui::body(10.0f));
        g.drawText(juce::String(hz, 1) + " Hz   " + juce::String(count) + " partials",
                   r.reduced(10.0f, 5.0f), juce::Justification::topRight, false);
    }
}

void AmbientSynthEditor::paint(juce::Graphics& g)
{
    // Everything below is drawn in the design space; one transform scales the whole editor.
    g.fillAll(ui::bg0);
    g.addTransform(juce::AffineTransform::scale(scale_));
    {   // a little vertical lift, so the window is not a flat grey field
        juce::ColourGradient sky(ui::bg1, 0.0f, 0.0f, ui::bg0, 0.0f, static_cast<float>(kHeaderH * 3), false);
        g.setGradientFill(sky);
        g.fillRect(0, 0, designW_, kHeaderH * 3);
    }

    // Header
    g.setColour(kGroupFill);
    g.fillRect(header_);
    g.setColour(kText);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("AmbientSynth", 14, 6, 180, 26, juce::Justification::centredLeft);
    paintRoutingMap(g, routing_);

    // Keyboard strip with near (bright) / far (dim) notes and the brain's root.
    const int root = proc_.engine().brainRoot();
    const int first = 24, last = 108;
    const float keyW = static_cast<float>(keys_.getWidth()) / static_cast<float>(last - first + 1);
    for (int n = first; n <= last; ++n) {
        const float kx = keys_.getX() + (n - first) * keyW;
        const bool black = juce::MidiMessage::isMidiNoteBlack(n);
        juce::Colour col = black ? juce::Colour(0xff2a2e36) : juce::Colour(0xff3a3f4a);
        if (sounding_[n]) {
            const float d = juce::jlimit(0.0f, 1.0f, proc_.engine().noteDistance(n));
            col = kAccent.interpolatedWith(juce::Colour(0xff35506a), d);
        }
        if (n == root) col = col.interpolatedWith(juce::Colours::orange, 0.6f);
        g.setColour(col);
        g.fillRect(kx + 0.5f, static_cast<float>(keys_.getY()), keyW - 1.0f, static_cast<float>(keys_.getHeight()));
    }
    const int voices = proc_.engine().activeVoices();
    juce::String info = juce::String(voices) + " voice" + (voices == 1 ? "" : "s")
        + "   root " + juce::MidiMessage::getMidiNoteName(root, true, true, 4)
        + "   " + juce::String(proc_.engine().scale().name)
        + "   arc " + juce::String(proc_.engine().arcValue(), 2);
    if (proc_.userScaleName().isNotEmpty()) info += "   (user: " + proc_.userScaleName() + ")";
    info += proc_.oscRunning() ? "   OSC :" + juce::String(proc_.oscPort()) + " (" + juce::String(static_cast<juce::int64>(proc_.oscMessages())) + " msg)"
                               : "   OSC off: " + proc_.oscError();
    const auto& gl = proc_.gestures();
    const float clutch = gl.input(GestureInput::RightPinch);
    if (gl.calibrating()) info += "   CALIBRATING " + juce::String(static_cast<int>(gl.calibrationProgress() * 100.0f)) + " %";
    else info += clutch > 0.5f ? "   hands: engaged" : "   hands: free";
    info += "   L " + juce::String(gl.input(GestureInput::LeftHeight), 2) + "  R " + juce::String(gl.input(GestureInput::RightHeight), 2)
          + "  dist " + juce::String(gl.input(GestureInput::HandDistance), 2) + "  pinch " + juce::String(clutch, 2);
    g.setColour(kDim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText(info, routing_.getX() + 90, header_.getBottom() - 16, getWidth() - routing_.getX() - 400, 14, juce::Justification::centredLeft);

    if (proc_.isRecording()) {
        g.setColour(juce::Colour(0xffe05050));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText("REC " + juce::String(static_cast<int>(proc_.recordedSeconds() / 60)) + ":" + juce::String(static_cast<int>(proc_.recordedSeconds()) % 60).paddedLeft('0', 2),
                   760, 8, 90, 24, juce::Justification::centredLeft);
    }
    // The Master section is the only section painted here (it sits in the header).
    if (const Section* ms = const_cast<AmbientSynthEditor*>(this)->findSection("Master")) {
        g.setColour(kSectionFill);
        g.fillRoundedRectangle(ms->bounds.toFloat(), 5.0f);
        g.setColour(kMaster.withAlpha(0.85f));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText("MASTER", ms->bounds.getX() + kPad, ms->bounds.getY() + 1, ms->bounds.getWidth() - 2 * kPad, kTitleH, juce::Justification::centredLeft);
    }
}

void AmbientSynthEditor::paintContent(juce::Graphics& g)
{
    g.fillAll(kBg);
    for (const auto& grp : groups_) {
        g.setColour(kGroupFill);
        g.fillRoundedRectangle(grp.bounds.toFloat(), 8.0f);
        g.setColour(grp.colour);
        g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(grp.bounds.getX()), static_cast<float>(grp.bounds.getY()), 4.0f, static_cast<float>(grp.bounds.getHeight())), 2.0f);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(grp.name, grp.bounds.getX() + 12, grp.bounds.getY() + 2, grp.bounds.getWidth() - 20, kGroupTitleH, juce::Justification::centredLeft);
    }
    for (const auto& s : sections_) {
        if (s.name == "Master") continue;   // painted in the header
        const juce::Colour col = s.group >= 0 ? groups_[static_cast<size_t>(s.group)].colour : kMaster;
        g.setColour(kSectionFill);
        g.fillRoundedRectangle(s.bounds.toFloat(), 5.0f);
        g.setColour(col.withAlpha(0.85f));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(s.name.toUpperCase(), s.bounds.getX() + kPad, s.bounds.getY() + 1, s.bounds.getWidth() - 2 * kPad, kTitleH, juce::Justification::centredLeft);
        if (s.name == "Morph") {
            const float pos = proc_.engine().morphPosition();
            const bool active = proc_.apvts.getRawParameterValue("morph_active")->load() >= 0.5f;
            juce::String txt = "A: " + proc_.morphSlotName(0) + "   B: " + proc_.morphSlotName(1)
                             + (active ? "   playing " + juce::String(static_cast<int>(std::lround((1.0f - pos) * 100))) + " % A / "
                                         + juce::String(static_cast<int>(std::lround(pos * 100))) + " % B" : "   (off)");
            g.setColour(kDim);
            g.setFont(juce::FontOptions(10.5f));
            g.drawText(txt, s.bounds.getX() + 70, s.bounds.getY() + 1, s.bounds.getWidth() - 80, kTitleH, juce::Justification::centredLeft);
            // position bar
            juce::Rectangle<float> bar(static_cast<float>(s.bounds.getX() + kPad), static_cast<float>(s.bounds.getBottom() - 6), static_cast<float>(s.bounds.getWidth() - 2 * kPad), 3.0f);
            g.setColour(juce::Colour(0xff2c3038)); g.fillRect(bar);
            g.setColour(kMorph); g.fillRect(bar.withWidth(bar.getWidth() * pos));
        }
    }
}
