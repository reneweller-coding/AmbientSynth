#include "PluginEditor.h"
#include "AmbientLookAndFeel.h"
#include <cstdlib>
#include "ambient/Params.h"
#include "ambient/Help.h"
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
// A tab bar above a row that pages, and the modulation strip along the bottom; the page height
// follows from these plus the body, so nothing on the main page ever has to scroll.
constexpr int kTabH = 26, kStripH = 300, kDisplayMinW = 300;
const juce::Colour kBg = ui::bg0, kGroupFill = ui::group, kSectionFill = ui::card, kAccent = ui::accent,
                   kText = ui::text, kDim = ui::dim;
const juce::Colour kVoice = ui::voiceCol, kFore = ui::foreCol, kBack = ui::backCol, kCosmos = ui::cosmosCol,
                   kConductor = ui::condCol, kMorph = ui::morphCol, kMaster = ui::masterCol;
}

AmbientSynthEditor::AmbientSynthEditor(AmbientSynthProcessor& p)
    : AudioProcessorEditor(p), proc_(p)
{
    setLookAndFeel(&laf_);

    // Two columns, everything in sight at once: the voice and the morph on the left, the effects,
    // the cosmos and the conductor on the right. Rows whose sections are of a kind (the three
    // sources, the two filters, the effect pairs, the conductor's tables) page through tabs.
    groups_ = {
        { "VOICE",      kVoice,     { { "Source 1", "Strands", "Source 2", "Source 3", "Strike" }, { "Air", "Filter", "Envelope", "Z-Plane" }, { "Space", "Foundation" } }, {}, 0 },   // rows 0 and 1 page through tabs
        { "MORPH",      kMorph,     { { "Morph", "Macros" } }, {}, 0 },
        { "FOREGROUND", kFore,      { { "Ensemble", "Delay", "Delay 2", "Near Reverb", "Blur" } }, {}, 1 },
        { "BACKGROUND", kBack,      { { "Cloud", "Far Reverb", "Feedback", "Room" } }, {}, 1 },
        { "COSMOS",     kCosmos,    { { "Cosmos" } }, {}, 1 },
        { "CONDUCTOR",  kConductor, { { "Cluster Brain", "Tuning", "Coherence", "Clock" } }, {}, 1 },
    };
    tabRows_ = {
        { 0, 0, { "SOURCE 1", "STRANDS", "SOURCE 2", "SOURCE 3", "STRIKE" }, { { "Source 1" }, { "Strands" }, { "Source 2" }, { "Source 3" }, { "Strike" } } },
        { 0, 1, { "FILTER", "Z-PLANE", "AMP ENV" }, { { "Air", "Filter" }, { "Z-Plane" }, { "Envelope" } } },
        { 1, 0, { "MORPH", "MACROS" }, { { "Morph" }, { "Macros" } } },
        { 2, 0, { "ENSEMBLE + DELAY", "DELAY 2 + NEAR REVERB + BLUR" }, { { "Ensemble", "Delay" }, { "Delay 2", "Near Reverb", "Blur" } } },
        { 3, 0, { "CLOUD + FAR REVERB", "FEEDBACK + ROOM" }, { { "Cloud", "Far Reverb" }, { "Feedback", "Room" } } },
        { 5, 0, { "BRAIN", "TUNING", "COHERENCE", "CLOCK" }, { { "Cluster Brain" }, { "Tuning" }, { "Coherence" }, { "Clock" } } },
    };

    content_.onPaint = [this](juce::Graphics& g) { paintContent(g); };
    content_.onMouse = [this](const juce::MouseEvent& e) { if (!e.mods.isPopupMenu()) clickTabs(e.getPosition()); };
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(false, false);
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
    helpButton_ = std::make_unique<juce::TextButton>("Help");
    helpButton_->setTooltip("The manual, by topic (F1)");
    helpButton_->setClickingTogglesState(true);
    helpButton_->setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
    helpButton_->onClick = [this] { setPage(helpButton_->getToggleState() ? 3 : 0); };
    addAndMakeVisible(*helpButton_);
    help_ = std::make_unique<HelpView>(proc_, *this);
    addChildComponent(*help_);
    tooltips_ = std::make_unique<juce::TooltipWindow>(nullptr, 600);
    setWantsKeyboardFocus(true);

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

    mod_ = std::make_unique<ModView>(proc_, *this);
    addAndMakeVisible(*mod_);

    // Displays inside the grid: each fills the room its row leaves after the knobs.
    scope_ = std::make_unique<ScopeView>(proc_);
    filterView_ = std::make_unique<FilterView>(proc_);
    source1View_ = std::make_unique<SourceView>(proc_, 1);
    source2View_ = std::make_unique<SourceView>(proc_, 2);
    source3View_ = std::make_unique<SourceView>(proc_, 3);
    brainView_ = std::make_unique<BrainView>(proc_);
    stageView_ = std::make_unique<StageView>(proc_);
    cosmosView_ = std::make_unique<CosmosView>(proc_);
    envView_ = std::make_unique<EnvView>(proc_);
    for (juce::Component* c : { static_cast<juce::Component*>(scope_.get()), static_cast<juce::Component*>(filterView_.get()),
                                static_cast<juce::Component*>(source2View_.get()), static_cast<juce::Component*>(source3View_.get()),
                                static_cast<juce::Component*>(source1View_.get()), static_cast<juce::Component*>(brainView_.get()),
                                static_cast<juce::Component*>(stageView_.get()), static_cast<juce::Component*>(cosmosView_.get()),
                                static_cast<juce::Component*>(envView_.get()) })
        content_.addAndMakeVisible(*c);
    if (tabRows_.size() > 1) {   // VOICE row 0: OSC 1 | SOURCE 2 | SOURCE 3; row 1: FILTER | Z-PLANE
        tabRows_[0].displays = { source1View_.get(), scope_.get(), source2View_.get(), source3View_.get(), nullptr };
        tabRows_[1].displays = { filterView_.get(), nullptr, envView_.get() };
    }
    if (groups_.size() > 4) {
        groups_[0].displays = { nullptr, nullptr, stageView_.get() };   // VOICE: the Space / Foundation row
        groups_[4].displays = { cosmosView_.get() };                    // COSMOS
    }
    if (tabRows_.size() > 5) tabRows_[5].displays = { brainView_.get(), nullptr, nullptr, nullptr };   // CONDUCTOR: BRAIN | TUNING | COHERENCE

    // Free scaling: the corner is the zoom. The ratio is fixed so the arrangement never changes,
    // only its size, and the window opens at whatever fraction of the screen actually fits.
    setResizable(true, true);
    layoutBody();                       // measure once: the design size is what the body needs
    // Width and height: exactly what header, body and modulation strip need, so nothing scrolls.
    designW_ = std::max(kMinDesignW, bodyW_);
    designH_ = std::max(kMinDesignH, kHeaderH + bodyH_ + kStripH);
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
    if (juce::SystemStats::getEnvironmentVariable("AMBIENT_PERFORM", "").isNotEmpty()) setPage(1);
    {   // AMBIENT_PRESET=<name>: open on a named preset (dev aid for photographing a modulated patch)
        const juce::String ps = juce::SystemStats::getEnvironmentVariable("AMBIENT_PRESET", "");
        for (int i = 0; ps.isNotEmpty() && i < numPresets(); ++i)
            if (ps == preset(i).name) { proc_.applySoundPreset(i); if (soundBox_) soundBox_->setSelectedId(i + 1, juce::dontSendNotification); }
    }   // open on the perform page
    {   // dev aids: AMBIENT_BROWSE=1|map opens the browser (map view with "map"), AMBIENT_ROUTE=<route preset> preloads a route
        const juce::String br = juce::SystemStats::getEnvironmentVariable("AMBIENT_BROWSE", "");
        if (br.isNotEmpty()) { setPage(2); if (br == "map") browse_->setMode(1); }
        const int hv = juce::SystemStats::getEnvironmentVariable("AMBIENT_HELP", "0").getIntValue();   // AMBIENT_HELP=<topic+1>: open the manual there
        if (hv > 0) { setPage(3); if (help_) help_->topics.selectRow(hv - 1); }
        const juce::String sc = juce::SystemStats::getEnvironmentVariable("AMBIENT_SCROLL", "");
        if (sc.isNotEmpty()) juce::MessageManager::callAsync([this, y = sc.getIntValue()] { viewport_.setViewPosition(0, y); });
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
            if (s.name == "Tuning" || s.name == "Far Reverb") s.maxUnits = 8;
            if (s.name == "Cosmos") s.maxUnits = 8;                          // two even rows
            if (s.name == "Tuning") s.maxUnits = 9;
            if (s.name == "Morph") s.maxUnits = 9;
            if (s.name == "Macros") s.maxUnits = 10;                         // one row: the eight macros, Air, Inertia
            if (s.name == "Space") s.maxUnits = 8;                           // two rows each, side by side
            if (s.name == "Foundation") s.maxUnits = 5;
            if (s.name == "Source 1" || s.name == "Source 2" || s.name == "Source 3") s.maxUnits = 12;
            if (s.name == "Strands") s.maxUnits = 10;
            if (s.name == "Delay" || s.name == "Delay 2") s.maxUnits = 12;   // one row with the two Sync choices and Absorb
            if (s.name == "Far Reverb") s.maxUnits = 9;                      // one row with Rotate
            if (s.name == "Filter") s.maxUnits = 9;                          // one row: On, Model, five knobs, Drive
            if (s.name == "Cloud") s.maxUnits = 8;                           // one row with Sync
            if (s.name == "Z-Plane") s.maxUnits = 13;                        // one row with Mode and Route
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
        if (auto* tc = dynamic_cast<juce::SettableTooltipClient*>(c.comp.get())) tc->setTooltip(ambient::paramHelp(d.id));
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

// Which parameter sits under a screen point: the drop target for a dragged modulation source.
int AmbientSynthEditor::cellParamAt(juce::Point<int> screenPos) const
{
    for (const auto& c : cells_) {
        if (c.param < 0 || c.comp == nullptr || !c.comp->isShowing()) continue;
        if (c.comp->getScreenBounds().contains(screenPos)) return c.param;
    }
    return -1;
}

juce::Rectangle<int> AmbientSynthEditor::cellScreenBounds(int cellIndex) const
{
    if (cellIndex < 0 || cellIndex >= static_cast<int>(cells_.size())) return {};
    const Cell& c = cells_[static_cast<size_t>(cellIndex)];
    return c.comp != nullptr ? c.comp->getScreenBounds() : juce::Rectangle<int>();
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
    designH_ = std::max(kMinDesignH, kHeaderH + bodyH_ + kStripH);
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
    if (helpButton_) helpButton_->setBounds(1200, 8, 56, 24);

    const int W = designW_, H = designH_;
    if (perform_) perform_->setBounds(0, kHeaderH, W, H - kHeaderH);
    if (browse_) browse_->setBounds(0, kHeaderH, W, H - kHeaderH);
    if (help_) help_->setBounds(0, kHeaderH, W, H - kHeaderH);
    // The modulation strip sits along the bottom of the main page, the way Pigments puts its
    // modulator lane there: the sources are always in sight, and a route is a drag away.
    const int stripH = kStripH;
    if (mod_) { mod_->setBounds(0, H - stripH, W, stripH); mod_->toFront(false); }
    helpLine_ = { 12, 40, W / 2 - 30, kHeaderH - 62 };
    keys_ = { W / 2, 44, juce::jmax(160, W - W / 2 - 360), 24 };
    if (master_) master_->setBounds(W - 74, 6, 66, 52);

    // The Master section (mid/side) sits in the header, left of the master knob.
    if (Section* ms = findSection("Master")) {
        ms->maxUnits = 8;
        layoutSection(*ms, W - 74 - sectionWidth(*ms) - 6, 4);
        ms->bounds = ms->bounds.withTrimmedTop(-2);
    }

    // Everything else scrolls below the header.
    viewport_.setBounds(0, kHeaderH, W, H - kHeaderH - stripH);

    content_.setSize(std::max(bodyW_, viewport_.getMaximumVisibleWidth()),
                     std::max(bodyH_, viewport_.getMaximumVisibleHeight()));
}

// Places every group and section, and records how much room the whole body needs. The design
// size is measured from this once, in the constructor -- guessing it meant the right-hand column
// kept falling off the edge. A tabbed row is as wide and as tall as its widest and tallest page,
// so switching a tab never moves anything else.
void AmbientSynthEditor::layoutBody()
{
    int nCols = 1;
    for (auto& g : groups_) nCols = std::max(nCols, g.column + 1);
    std::vector<int> colWidth(static_cast<size_t>(nCols), 0), colX(static_cast<size_t>(nCols), 0), colY(static_cast<size_t>(nCols), kPad);
    auto pageWidth = [this](const std::vector<juce::String>& names) {
        int w = kPad;
        for (auto& n : names) if (Section* s = findSection(n)) w += sectionWidth(*s) + kPad;
        return w;
    };
    auto pageHeight = [this](const std::vector<juce::String>& names) {
        int h = 0;
        for (auto& n : names) if (Section* s = findSection(n)) h = std::max(h, sectionHeight(*s));
        return h;
    };
    for (size_t gi = 0; gi < groups_.size(); ++gi) {
        auto& g = groups_[gi];
        for (size_t ri = 0; ri < g.rows.size(); ++ri) {
            // A page with a display asks for room to draw it in, or the picture would be squeezed out.
            int w = 0;
            if (TabRow* t = tabRowFor(static_cast<int>(gi), static_cast<int>(ri))) {
                for (size_t pi = 0; pi < t->pages.size(); ++pi)
                    w = std::max(w, pageWidth(t->pages[pi]) + (pi < t->displays.size() && t->displays[pi] != nullptr ? kDisplayMinW : 0));
            } else {
                w = pageWidth(g.rows[ri]) + (ri < g.displays.size() && g.displays[ri] != nullptr ? kDisplayMinW : 0);
            }
            colWidth[static_cast<size_t>(g.column)] = std::max(colWidth[static_cast<size_t>(g.column)], w);
        }
    }
    colX[0] = kPad;
    for (size_t ci = 1; ci < colX.size(); ++ci) colX[ci] = colX[ci - 1] + colWidth[ci - 1] + kPad;
    const juce::Font tabFont = ui::title(10.5f);
    for (size_t gi = 0; gi < groups_.size(); ++gi) {
        auto& g = groups_[gi];
        const size_t col = static_cast<size_t>(g.column);
        const int x0 = colX[col];
        int y = colY[col] + kGroupTitleH;
        for (size_t ri = 0; ri < g.rows.size(); ++ri) {
            TabRow* t = tabRowFor(static_cast<int>(gi), static_cast<int>(ri));
            const std::vector<juce::String>& names = t != nullptr ? t->pages[static_cast<size_t>(t->active)] : g.rows[ri];
            juce::Component* disp = nullptr;
            int rowH = 0;
            if (t != nullptr) {
                for (auto& pg : t->pages) rowH = std::max(rowH, pageHeight(pg));
                t->bar = { x0 + kPad, y, colWidth[col] - 2 * kPad, kTabH };
                t->tabs.clear();
                int tx = t->bar.getX();
                for (auto& nm : t->names) {
                    const int tw = juce::GlyphArrangement::getStringWidthInt(tabFont, nm) + 30;
                    t->tabs.push_back({ tx, y, tw, kTabH - 4 });
                    tx += tw + 4;
                }
                for (size_t pi = 0; pi < t->pages.size(); ++pi) {
                    if (static_cast<int>(pi) == t->active) continue;
                    for (auto& n : t->pages[pi]) if (Section* s = findSection(n)) setSectionVisible(*s, false);
                    if (pi < t->displays.size() && t->displays[pi] != nullptr) t->displays[pi]->setVisible(false);
                }
                if (static_cast<size_t>(t->active) < t->displays.size()) disp = t->displays[static_cast<size_t>(t->active)];
                y += kTabH;
            } else {
                rowH = pageHeight(names);
                if (ri < g.displays.size()) disp = g.displays[ri];
            }
            int x = x0 + kPad;
            for (auto& n : names) {
                Section* s = findSection(n);
                if (s == nullptr) continue;
                setSectionVisible(*s, true);
                layoutSection(*s, x, y);
                x += s->bounds.getWidth() + kPad;
            }
            // Whatever the row leaves free goes to its display -- that room used to stay empty.
            if (disp != nullptr) {
                const int right = x0 + colWidth[col] - kPad;
                if (right - x >= 120 && rowH > 0) { disp->setBounds(x, y, right - x, rowH); disp->setVisible(true); }
                else disp->setVisible(false);
            }
            y += rowH + kPad;
        }
        g.bounds = { x0, colY[col], colWidth[col], y - colY[col] };
        colY[col] = y + kPad;
    }
    bodyW_ = colX.back() + colWidth.back() + kPad;
    bodyH_ = 0;
    for (int yy : colY) bodyH_ = std::max(bodyH_, yy);
}

AmbientSynthEditor::TabRow* AmbientSynthEditor::tabRowFor(int group, int row)
{
    for (auto& t : tabRows_) if (t.group == group && t.row == row) return &t;
    return nullptr;
}

void AmbientSynthEditor::setSectionVisible(Section& s, bool v)
{
    s.visible = v;
    for (int ci : s.cells) {
        Cell& c = cells_[static_cast<size_t>(ci)];
        if (c.comp) c.comp->setVisible(v);
        if (c.label) c.label->setVisible(v);
    }
}

void AmbientSynthEditor::clickTabs(juce::Point<int> pos)
{
    for (auto& t : tabRows_)
        for (size_t i = 0; i < t.tabs.size(); ++i)
            if (t.tabs[i].contains(pos) && static_cast<int>(i) != t.active) {
                t.active = static_cast<int>(i);
                layoutBody();
                content_.repaint();
                return;
            }
}

// ---------------------------------------------------------------- perform page

void AmbientSynthEditor::setPerforming(bool on) { setPage(on ? 1 : 0); }

void AmbientSynthEditor::setPage(int page)
{
    performing_ = page == 1;
    viewport_.setVisible(page == 0);
    perform_->setVisible(page == 1);
    browse_->setVisible(page == 2);
    if (help_) help_->setVisible(page == 3);
    if (mod_) mod_->setVisible(page == 0);
    if (performButton_->getToggleState() != (page == 1)) performButton_->setToggleState(page == 1, juce::dontSendNotification);
    if (browseButton_->getToggleState() != (page == 2)) browseButton_->setToggleState(page == 2, juce::dontSendNotification);
    if (helpButton_ && helpButton_->getToggleState() != (page == 3)) helpButton_->setToggleState(page == 3, juce::dontSendNotification);

    if (page == 2) browse_->applyFilter();

}


// ---------------------------------------------------------------- modulation strip

namespace {
juce::Colour sourceColour(ambient::ModSource s)
{
    using MS = ambient::ModSource;
    const int i = static_cast<int>(s);
    if (i >= static_cast<int>(MS::Lfo1) && i <= static_cast<int>(MS::Lfo8)) return ui::accent;
    if (i >= static_cast<int>(MS::Env1) && i <= static_cast<int>(MS::Env6)) return ui::foreCol;
    if (i >= static_cast<int>(MS::MacroA) && i <= static_cast<int>(MS::MacroH)) return ui::morphCol;
    if (i >= static_cast<int>(MS::Kura1) && i <= static_cast<int>(MS::Kura4)) return ui::condCol;
    return ui::cosmosCol;
}
}

AmbientSynthEditor::ModView::ModView(AmbientSynthProcessor& p, AmbientSynthEditor& o)
    : proc(p), owner(o)
{
    auto addKnob = [this](Row& row, const juce::String& key, const juce::String& name) {
        const ParamDesc* d = findParam(key.toRawUTF8());
        if (d == nullptr) return;
        if (d->kind == ParamKind::Choice) {
            auto cb = std::make_unique<juce::ComboBox>();
            for (int i = 0; i < d->numChoices; ++i) cb->addItem(d->choices[i], i + 1);
            cb->setTooltip(ambient::paramHelp(d->id));
            addAndMakeVisible(*cb);
            row.combos.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, key, *cb));
            row.controls.push_back(std::move(cb));
        } else {
            auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            if (d->unit[0] != 0) s->setTextValueSuffix(juce::String(" ") + d->unit);
            s->setTooltip(ambient::paramHelp(d->id));
            s->setColour(juce::Slider::rotarySliderFillColourId, ui::accent);
            addAndMakeVisible(*s);
            row.sliders.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, key, *s));
            row.controls.push_back(std::move(s));
        }
        auto l = std::make_unique<juce::Label>(juce::String(), name);
        l->setJustificationType(juce::Justification::centred);
        l->setFont(ui::body(10.0f));
        l->setColour(juce::Label::textColourId, ui::dim);
        addAndMakeVisible(*l);
        row.labels.push_back(std::move(l));
    };

    for (int i = 0; i < ambient::kNumLfos; ++i) {
        Row& r = lfos[static_cast<size_t>(i)];
        r.title = "LFO " + juce::String(i + 1);
        const juce::String n(i + 1);
        addKnob(r, "lfo" + n + "_shape", "Shape");
        addKnob(r, "lfo" + n + "_rate", "Rate");
        addKnob(r, "lfo" + n + "_phase", "Phase");
        addKnob(r, "lfo" + n + "_depth", "Depth");
        addKnob(r, "lfo" + n + "_mode", "Mode");
        addKnob(r, "lfo" + n + "_table", "Table");
        addKnob(r, "lfo" + n + "_sync", "Sync");
    }
    for (int i = 0; i < ambient::kNumModEnvs; ++i) {
        Row& r = envs[static_cast<size_t>(i)];
        r.title = "ENV " + juce::String(i + 1);
        const juce::String n(i + 1);
        addKnob(r, "env" + n + "_mode", "Mode");
        addKnob(r, "env" + n + "_time", "Time");
        addKnob(r, "env" + n + "_depth", "Depth");
        addKnob(r, "env" + n + "_sync", "Sync");
    }

    // The lane: every source that can drive something, in the order the matrix names them.
    using MS = ambient::ModSource;
    auto card = [this](MS s, const juce::String& label, int t, int idx) {
        cards.push_back({ s, label, sourceColour(s), {}, t, idx });
    };
    for (int i = 0; i < ambient::kNumLfos; ++i) card(static_cast<MS>(static_cast<int>(MS::Lfo1) + i), "LFO " + juce::String(i + 1), 0, i);
    for (int i = 0; i < ambient::kNumModEnvs; ++i) card(static_cast<MS>(static_cast<int>(MS::Env1) + i), "ENV " + juce::String(i + 1), 1, i);
    for (int i = 0; i < 8; ++i) card(static_cast<MS>(static_cast<int>(MS::MacroA) + i), juce::String("MACRO ") + static_cast<char>('A' + i), 2, i);
    for (int i = 0; i < 4; ++i) card(static_cast<MS>(static_cast<int>(MS::Kura1) + i), "KURA " + juce::String(i + 1), 2, i);
    card(MS::Amp, "AMP", 2, 0);
    card(MS::Note, "NOTE", 2, 0);
    card(MS::Velocity, "VELO", 2, 0);
    card(MS::Distance, "DIST", 2, 0);
    card(MS::RandomPerNote, "RAND", 2, 0);

    for (auto* b : { &tabLfo, &tabEnv, &tabMatrix }) {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(4711);
        addAndMakeVisible(*b);
    }
    tabLfo.onClick = [this] { setTab(0); };
    tabEnv.onClick = [this] { setTab(1); };
    tabMatrix.onClick = [this] { setTab(2); };
    tabLfo.setToggleState(true, juce::dontSendNotification);

    matrixText.setMultiLine(true, false);
    matrixText.setReturnKeyStartsNewLine(true);
    matrixText.setFont(ui::body(12.0f));
    addChildComponent(matrixText);
    applyMatrix.onClick = [this] { pushMatrix(); };
    clearMatrix.onClick = [this] { matrixText.setText({}, false); pushMatrix(); };
    addChildComponent(applyMatrix);
    addChildComponent(clearMatrix);
    matrixInfo.setFont(ui::body(11.5f));
    matrixInfo.setColour(juce::Label::textColourId, ui::dim);
    addChildComponent(matrixInfo);
    hint.setText("source > target : depth [: via] [: u]   one per line -- or drag a card onto a knob",
                 juce::dontSendNotification);
    hint.setFont(ui::body(11.5f));
    hint.setColour(juce::Label::textColourId, ui::faint);
    addChildComponent(hint);
    pullMatrix();
    setTab(0);
    startTimerHz(30);
}

void AmbientSynthEditor::ModView::setTab(int t)
{
    tab = t;
    for (int i = 0; i < ambient::kNumLfos; ++i) {
        for (auto& c : lfos[static_cast<size_t>(i)].controls) c->setVisible(t == 0);
        for (auto& l : lfos[static_cast<size_t>(i)].labels) l->setVisible(t == 0);
    }
    for (int i = 0; i < ambient::kNumModEnvs; ++i) {
        for (auto& c : envs[static_cast<size_t>(i)].controls) c->setVisible(t == 1);
        for (auto& l : envs[static_cast<size_t>(i)].labels) l->setVisible(t == 1);
    }
    juce::Component* const matrixParts[] = { &matrixText, &applyMatrix, &clearMatrix, &matrixInfo, &hint };
    for (juce::Component* c : matrixParts) c->setVisible(t == 2);
    if (t == 2) pullMatrix();
    tabLfo.setToggleState(t == 0, juce::dontSendNotification);
    tabEnv.setToggleState(t == 1, juce::dontSendNotification);
    tabMatrix.setToggleState(t == 2, juce::dontSendNotification);
    resized();
    repaint();
}

void AmbientSynthEditor::ModView::pullMatrix()
{
    char buf[4096];
    const int n = proc.engine().writeModMatrix(buf, sizeof(buf));
    juce::String t(juce::CharPointer_UTF8(buf), static_cast<size_t>(juce::jmax(0, n)));
    matrixText.setText(t.replace(";", "\n"), false);
    matrixInfo.setColour(juce::Label::textColourId, ui::dim);
    matrixInfo.setText(juce::String(proc.engine().modMatrix().count()) + " of 32 routes", juce::dontSendNotification);
}

void AmbientSynthEditor::ModView::pushMatrix()
{
    const juce::String t = matrixText.getText().replaceCharacters("\n", ";").removeCharacters(" ");
    if (proc.engine().setModMatrixText(t.toRawUTF8())) {
        matrixInfo.setColour(juce::Label::textColourId, ui::dim);
        matrixInfo.setText(juce::String(proc.engine().modMatrix().count()) + " of 32 routes", juce::dontSendNotification);
    } else {
        matrixInfo.setColour(juce::Label::textColourId, juce::Colour(0xffd08a8a));
        matrixInfo.setText("a line could not be read; the previous matrix is kept", juce::dontSendNotification);
    }
    owner.repaint();
}

bool AmbientSynthEditor::ModView::addRoute(ambient::ModSource src, ParamId target)
{
    char buf[4096];
    const int n = proc.engine().writeModMatrix(buf, sizeof(buf));
    juce::String t(juce::CharPointer_UTF8(buf), static_cast<size_t>(juce::jmax(0, n)));
    if (t.isNotEmpty()) t += ";";
    // A quarter of the target's range: enough to hear, not enough to wreck the preset.
    t += juce::String(ambient::modSourceName(src)) + ">" + paramDesc(target).key + ":0.25";
    if (!proc.engine().setModMatrixText(t.toRawUTF8())) return false;
    if (tab == 2) pullMatrix();
    matrixInfo.setText(juce::String(proc.engine().modMatrix().count()) + " of 32 routes", juce::dontSendNotification);
    owner.repaint();
    return true;
}

void AmbientSynthEditor::ModView::timerCallback() { if (isShowing()) repaint(); }

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
    const int sync = static_cast<int>(std::lround(get("lfo" + n + "_sync")));
    if (ambient::syncOn(sync)) sp.rateHz = static_cast<float>(ambient::syncHz(sync, proc.engine().tempo()));
    return sp;
}


// ---------------------------------------------------------------- strip: drawing

namespace {
void curveFrame(juce::Graphics& g, juce::Rectangle<int> r, bool lit, juce::Colour c)
{
    g.setColour(ui::bg0.withAlpha(0.6f));
    g.fillRoundedRectangle(r.toFloat(), 5.0f);
    g.setColour(lit ? c.withAlpha(0.35f) : ui::cardEdge);
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 5.0f, 1.0f);
    g.setColour(ui::track.withAlpha(0.7f));
    g.drawLine(static_cast<float>(r.getX() + 4), r.toFloat().getCentreY(),
               static_cast<float>(r.getRight() - 4), r.toFloat().getCentreY(), 1.0f);
}
}

void AmbientSynthEditor::ModView::paintCard(juce::Graphics& g, const Card& c, bool hot)
{
    const auto r = c.bounds;
    const float v = proc.engine().modSource(static_cast<int>(c.source));
    const bool used = [&] {
        const ambient::ModMatrix& m = proc.engine().modMatrix();
        for (int i = 0; i < m.count(); ++i) if (m.route(i).source == c.source) return true;
        return false;
    }();
    g.setColour(used ? ui::card.brighter(0.05f) : ui::card.withAlpha(0.55f));
    g.fillRoundedRectangle(r.toFloat(), 4.0f);
    g.setColour(hot ? c.colour.withAlpha(0.9f) : (used ? c.colour.withAlpha(0.45f) : ui::cardEdge));
    g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 4.0f, hot ? 1.6f : 1.0f);

    // Its shape, small. LFOs and envelopes draw their curve; everything else draws its value.
    const auto plot = r.reduced(5, 4).withTrimmedTop(13);
    const float cy = plot.toFloat().getCentreY(), h = plot.getHeight() * 0.42f;
    using MS = ambient::ModSource;
    const int si = static_cast<int>(c.source);
    juce::Path p;
    if (si >= static_cast<int>(MS::Lfo1) && si <= static_cast<int>(MS::Lfo8)) {
        const ambient::LfoSpec sp = specOf(c.index);
        const ambient::Lfo& live = proc.engine().lfo(c.index);
        for (int s = 0; s <= 40; ++s) {
            const float t = s / 40.0f;
            const float y = cy - juce::jlimit(-1.0f, 1.0f, ambient::Lfo::shapeAt(sp, t + sp.phase, nullptr, &live) * sp.depth) * h;
            const float x = plot.getX() + t * plot.getWidth();
            if (s == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
    } else if (si >= static_cast<int>(MS::Env1) && si <= static_cast<int>(MS::Env6)) {
        const ambient::ModEnv& e = proc.engine().envShape(c.index);
        const float len = juce::jmax(0.001f, e.length());
        for (int s = 0; s <= 40; ++s) {
            const float t = s / 40.0f;
            const float y = cy - juce::jlimit(-1.0f, 1.0f, e.at(t * len, ambient::EnvMode::OneShot, true)) * h;
            const float x = plot.getX() + t * plot.getWidth();
            if (s == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
    } else {   // a bar for the plain sources
        const float y = cy - juce::jlimit(-1.0f, 1.0f, v) * h;
        g.setColour(c.colour.withAlpha(used ? 0.7f : 0.3f));
        g.fillRect(static_cast<float>(plot.getX()), juce::jmin(y, cy), static_cast<float>(plot.getWidth()), std::abs(cy - y) + 1.0f);
    }
    if (!p.isEmpty()) {
        g.setColour(c.colour.withAlpha(used ? 0.85f : 0.35f));
        g.strokePath(p, juce::PathStrokeType(1.3f));
    }
    // The live value as a dot on the right edge, so even a still source shows it is alive.
    g.setColour(ui::live.withAlpha(0.9f));
    g.fillEllipse(static_cast<float>(plot.getRight()) - 2.0f, cy - juce::jlimit(-1.0f, 1.0f, v) * h - 2.0f, 4.0f, 4.0f);

    g.setColour(used ? ui::text : ui::dim);
    g.setFont(ui::title(9.5f));
    g.drawText(c.label, r.reduced(5, 3), juce::Justification::topLeft, false);
}

void AmbientSynthEditor::ModView::paintLane(juce::Graphics& g)
{
    g.setColour(ui::group);
    g.fillRect(lane);
    for (size_t i = 0; i < cards.size(); ++i)
        paintCard(g, cards[i], static_cast<int>(i) == hoverCard || static_cast<int>(i) == dragCard);
}

void AmbientSynthEditor::ModView::paintLfo(juce::Graphics& g, int i)
{
    const Row& row = lfos[static_cast<size_t>(i)];
    const auto r = row.curve;
    if (r.isEmpty()) return;
    const ambient::LfoSpec sp = specOf(i);
    const bool lit = sp.depth > 0.001f;
    curveFrame(g, r, lit, ui::accent);
    const ambient::Lfo& live = proc.engine().lfo(i);
    juce::Path p;
    const float x0 = static_cast<float>(r.getX() + 5), w = static_cast<float>(r.getWidth() - 10);
    const float cy = r.toFloat().getCentreY(), h = r.getHeight() * 0.36f;
    const int steps = juce::jlimit(48, 400, r.getWidth());
    for (int s = 0; s <= steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps);
        const float y = cy - juce::jlimit(-1.0f, 1.0f, ambient::Lfo::shapeAt(sp, t + sp.phase, nullptr, &live) * sp.depth) * h;
        if (s == 0) p.startNewSubPath(x0 + t * w, y); else p.lineTo(x0 + t * w, y);
    }
    g.setColour(ui::accent.withAlpha(lit ? 0.22f : 0.08f));
    g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(lit ? ui::accent : ui::faint);
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ph = proc.engine().lfoPhase(i);
    const float v = proc.engine().modSource(static_cast<int>(ambient::ModSource::Lfo1) + i);
    const float dx = x0 + ph * w, dy = cy - juce::jlimit(-1.0f, 1.0f, v) * h;
    g.setColour(ui::live.withAlpha(0.30f)); g.fillEllipse(dx - 7.0f, dy - 7.0f, 14.0f, 14.0f);
    g.setColour(ui::live);                  g.fillEllipse(dx - 3.0f, dy - 3.0f, 6.0f, 6.0f);
    const float period = 1.0f / juce::jmax(1.0e-5f, sp.rateHz);
    g.setColour(ui::dim); g.setFont(ui::body(10.0f));
    g.drawText(period >= 90.0f ? juce::String(period / 60.0f, 1) + " min" : juce::String(period, period < 10.0f ? 2 : 1) + " s",
               r.reduced(7, 3), juce::Justification::topRight, false);
    g.setColour(lit ? ui::text : ui::faint); g.setFont(ui::title(10.5f));
    g.drawText(row.title, r.reduced(7, 3), juce::Justification::topLeft, false);
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
    const float depth = get("env" + n + "_depth"), scale = juce::jmax(0.01f, get("env" + n + "_time"));
    const bool lit = depth > 0.001f && e.count() > 1;
    curveFrame(g, r, lit, ui::foreCol);
    const float x0 = static_cast<float>(r.getX() + 5), w = static_cast<float>(r.getWidth() - 10);
    const float cy = r.toFloat().getCentreY(), h = r.getHeight() * 0.36f;
    const float len = juce::jmax(0.001f, e.length());
    if (e.loopFrom() >= 0 && e.loopTo() > e.loopFrom() && e.loopTo() < e.count()) {
        const float a = e.point(e.loopFrom()).time / len, b = e.point(e.loopTo()).time / len;
        g.setColour(ui::foreCol.withAlpha(0.08f));
        g.fillRect(x0 + a * w, static_cast<float>(r.getY() + 4), (b - a) * w, static_cast<float>(r.getHeight() - 8));
    }
    juce::Path p;
    const int steps = juce::jlimit(48, 400, r.getWidth());
    for (int s = 0; s <= steps; ++s) {
        const float t = static_cast<float>(s) / static_cast<float>(steps) * len;
        const float y = cy - juce::jlimit(-1.0f, 1.0f, e.at(t, ambient::EnvMode::OneShot, true) * depth) * h;
        const float x = x0 + (t / len) * w;
        if (s == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
    }
    g.setColour(ui::foreCol.withAlpha(lit ? 0.22f : 0.08f));
    g.strokePath(p, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(lit ? ui::foreCol : ui::faint);
    g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (int k = 0; k < e.count(); ++k) {
        const ambient::EnvPoint& pt = e.point(k);
        const float x = x0 + (pt.time / len) * w, y = cy - juce::jlimit(-1.0f, 1.0f, pt.value * depth) * h;
        g.setColour(lit ? ui::foreCol.withAlpha(0.85f) : ui::faint);
        g.fillEllipse(x - 2.5f, y - 2.5f, 5.0f, 5.0f);
        if (k == e.sustain()) { g.setColour(ui::live); g.drawEllipse(x - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.4f); }
    }
    const float t = proc.engine().envTime() / scale;
    if (lit && t <= len * 1.02f) {
        const float v = proc.engine().modSource(static_cast<int>(ambient::ModSource::Env1) + i);
        const float dx = x0 + juce::jlimit(0.0f, 1.0f, t / len) * w, dy = cy - juce::jlimit(-1.0f, 1.0f, v) * h;
        g.setColour(ui::live.withAlpha(0.30f)); g.fillEllipse(dx - 7.0f, dy - 7.0f, 14.0f, 14.0f);
        g.setColour(ui::live);                  g.fillEllipse(dx - 3.0f, dy - 3.0f, 6.0f, 6.0f);
    }
    g.setColour(ui::dim); g.setFont(ui::body(10.0f));
    g.drawText(juce::String(len * scale, len * scale < 10.0f ? 1 : 0) + " s", r.reduced(7, 3), juce::Justification::topRight, false);
    g.setColour(lit ? ui::text : ui::faint); g.setFont(ui::title(10.5f));
    g.drawText(row.title, r.reduced(7, 3), juce::Justification::topLeft, false);
}

void AmbientSynthEditor::ModView::paint(juce::Graphics& g)
{
    g.setColour(ui::bg1);
    g.fillRect(getLocalBounds());
    g.setColour(ui::cardEdge);
    g.drawLine(0.0f, 0.5f, static_cast<float>(getWidth()), 0.5f, 1.0f);
    paintLane(g);
    if (tab == 0) for (int i = 0; i < ambient::kNumLfos; ++i) paintLfo(g, i);
    if (tab == 1) for (int i = 0; i < ambient::kNumModEnvs; ++i) paintEnv(g, i);

    // While dragging: a line from the card to the mouse, so the gesture is visible.
    if (dragCard >= 0) {
        const auto from = cards[static_cast<size_t>(dragCard)].bounds.getCentre().toFloat();
        g.setColour(cards[static_cast<size_t>(dragCard)].colour.withAlpha(0.8f));
        g.drawLine(juce::Line<float>(from, dragPos.toFloat()), 2.0f);
        g.fillEllipse(dragPos.x - 4.0f, dragPos.y - 4.0f, 8.0f, 8.0f);
    }
}

// ---------------------------------------------------------------- strip: mouse

void AmbientSynthEditor::ModView::mouseMove(const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < cards.size(); ++i) if (cards[i].bounds.contains(e.getPosition())) h = static_cast<int>(i);
    if (h != hoverCard) { hoverCard = h; repaint(lane); }
}

void AmbientSynthEditor::ModView::mouseDown(const juce::MouseEvent& e)
{
    for (size_t i = 0; i < cards.size(); ++i) {
        if (!cards[i].bounds.contains(e.getPosition())) continue;
        if (e.mods.isPopupMenu()) {   // right-click: what this source drives, and a way to drop it
            const ambient::ModMatrix& m = proc.engine().modMatrix();
            juce::PopupMenu menu;
            menu.addSectionHeader(cards[i].label + " drives");
            int n = 0;
            for (int k = 0; k < m.count(); ++k) {
                if (m.route(k).source != cards[i].source) continue;
                menu.addItem(1000 + k, juce::String(paramDesc(m.route(k).target).name) + "   "
                             + juce::String(m.route(k).depth, 2) + "   (remove)");
                ++n;
            }
            if (n == 0) menu.addItem(1, "nothing yet -- drag this card onto a knob", false, false);
            menu.showMenuAsync(juce::PopupMenu::Options(), [this](int r) {
                if (r >= 1000) {
                    char buf[4096];
                    const int len = proc.engine().writeModMatrix(buf, sizeof(buf));
                    juce::StringArray rows = juce::StringArray::fromTokens(
                        juce::String(juce::CharPointer_UTF8(buf), static_cast<size_t>(juce::jmax(0, len))), ";", "");
                    rows.remove(r - 1000);
                    proc.engine().setModMatrixText(rows.joinIntoString(";").toRawUTF8());
                    if (tab == 2) pullMatrix();
                    owner.repaint();
                }
            });
            return;
        }
        dragCard = static_cast<int>(i);
        dragPos = e.getPosition();
        if (cards[i].tab != 2) setTab(cards[i].tab);
        repaint();
        return;
    }
}

void AmbientSynthEditor::ModView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragCard < 0) return;
    dragPos = e.getPosition();
    repaint();
}

void AmbientSynthEditor::ModView::mouseUp(const juce::MouseEvent& e)
{
    if (dragCard >= 0) {
        const int param = owner.cellParamAt(e.getScreenPosition());
        if (param >= 0) addRoute(cards[static_cast<size_t>(dragCard)].source, static_cast<ParamId>(param));
        dragCard = -1;
        repaint();
    }
}

// ---------------------------------------------------------------- strip: layout

void AmbientSynthEditor::ModView::resized()
{
    auto area = getLocalBounds().reduced(10, 8);
    lane = area.removeFromTop(56);
    {   // the cards, wrapped over as many rows as they need
        const int cw = 74, ch = 26, gap = 4;
        int x = lane.getX(), y = lane.getY();
        for (auto& c : cards) {
            if (x + cw > lane.getRight()) { x = lane.getX(); y += ch + gap; }
            c.bounds = { x, y, cw, ch };
            x += cw + gap;
        }
    }
    area.removeFromTop(6);
    tabsArea = area.removeFromTop(22);
    {
        auto t = tabsArea;
        tabLfo.setBounds(t.removeFromLeft(90));    t.removeFromLeft(4);
        tabEnv.setBounds(t.removeFromLeft(110));   t.removeFromLeft(4);
        tabMatrix.setBounds(t.removeFromLeft(90));
    }
    area.removeFromTop(6);
    content = area;

    auto layoutRow = [](Row& row, juce::Rectangle<int> r) {
        row.curve = r.removeFromTop(juce::jmax(38, r.getHeight() * 52 / 100));
        r.removeFromTop(3);
        const int n = static_cast<int>(std::min(row.controls.size(), row.labels.size()));
        if (n <= 0) return;
        const int cw = r.getWidth() / n;
        for (int k = 0; k < n; ++k) {
            auto cell = r.removeFromLeft(cw);
            row.labels[static_cast<size_t>(k)]->setBounds(cell.removeFromBottom(12));
            if (dynamic_cast<juce::ComboBox*>(row.controls[static_cast<size_t>(k)].get()) != nullptr)
                row.controls[static_cast<size_t>(k)]->setBounds(cell.withSizeKeepingCentre(cell.getWidth() - 4, 20));
            else
                row.controls[static_cast<size_t>(k)]->setBounds(cell.reduced(2, 0));
        }
    };

    if (tab == 0) {
        const int cols = 4, rows = 2;
        const int w = content.getWidth() / cols, h = content.getHeight() / rows;
        for (int i = 0; i < ambient::kNumLfos; ++i)
            layoutRow(lfos[static_cast<size_t>(i)],
                      juce::Rectangle<int>(content.getX() + (i % cols) * w, content.getY() + (i / cols) * h, w, h).reduced(4, 2));
    } else if (tab == 1) {
        const int cols = 3, rows = 2;
        const int w = content.getWidth() / cols, h = content.getHeight() / rows;
        for (int i = 0; i < ambient::kNumModEnvs; ++i)
            layoutRow(envs[static_cast<size_t>(i)],
                      juce::Rectangle<int>(content.getX() + (i % cols) * w, content.getY() + (i / cols) * h, w, h).reduced(4, 2));
    } else {
        auto m = content;
        auto bottom = m.removeFromBottom(24);
        hint.setBounds(m.removeFromBottom(16));
        matrixText.setBounds(m);
        applyMatrix.setBounds(bottom.removeFromLeft(70)); bottom.removeFromLeft(6);
        clearMatrix.setBounds(bottom.removeFromLeft(70)); bottom.removeFromLeft(10);
        matrixInfo.setBounds(bottom);
    }
}

// ---------------------------------------------------------------- in-grid displays

namespace {
float rawParam(AmbientSynthProcessor& proc, const char* key)
{
    auto* v = proc.apvts.getRawParameterValue(key);
    return v != nullptr ? v->load() : 0.0f;
}

void displayFrame(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour c)
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
float xForHz(juce::Rectangle<float> plot, float hz)
{
    const float t = (std::log(juce::jmax(20.0f, hz)) - std::log(20.0f)) / (std::log(20000.0f) - std::log(20.0f));
    return plot.getX() + juce::jlimit(0.0f, 1.0f, t) * plot.getWidth();
}
float yForDb(juce::Rectangle<float> plot, float db)
{
    const float t = (db + 36.0f) / 48.0f;   // -36 .. +12 dB
    return plot.getBottom() - juce::jlimit(0.0f, 1.0f, t) * plot.getHeight();
}
void drawAxes(juce::Graphics& g, juce::Rectangle<float> plot)
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
}

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
    {   // what modulates this knob, each route removable -- the other side of the cards' menu
        const ambient::ModMatrix& m = proc_.engine().modMatrix();
        bool any = false;
        for (int k = 0; k < m.count(); ++k) {
            if (static_cast<int>(m.route(k).target) != param) continue;
            if (!any) { menu.addSeparator(); menu.addSectionHeader("Modulated by"); any = true; }
            menu.addItem(1000 + k, juce::String(ambient::modSourceName(m.route(k).source)) + "   " + juce::String(m.route(k).depth, 2) + "   (remove)");
        }
        if (!any) { menu.addSeparator(); menu.addItem(3, "not modulated -- drag a card from the strip onto the knob", false, false); }
    }
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(e.eventComponent), [this, id](int result) {
        if (result == 1) proc_.armMidiLearn(id);
        else if (result == 2) proc_.clearMidiLearn(id);
        else if (result >= 1000) {
            char buf[4096];
            const int len = proc_.engine().writeModMatrix(buf, sizeof(buf));
            juce::StringArray rows = juce::StringArray::fromTokens(juce::String(juce::CharPointer_UTF8(buf), static_cast<size_t>(juce::jmax(0, len))), ";", "");
            rows.remove(result - 1000);
            proc_.engine().setModMatrixText(rows.joinIntoString(";").toRawUTF8());
            repaint();
        }
    });
}

void AmbientSynthEditor::mouseEnter(const juce::MouseEvent& e)
{
    auto it = cellOf_.find(e.eventComponent);
    const int cell = it != cellOf_.end() ? it->second : -1;
    if (cell != hoveredCell_) { hoveredCell_ = cell; repaint(); }
}

void AmbientSynthEditor::mouseExit(const juce::MouseEvent& e)
{
    auto it = cellOf_.find(e.eventComponent);
    if (it != cellOf_.end() && it->second == hoveredCell_) { hoveredCell_ = -1; repaint(); }
}

bool AmbientSynthEditor::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::F1Key) { setPage(help_ && help_->isVisible() ? 0 : 3); return true; }
    if (k == juce::KeyPress::escapeKey && help_ && help_->isVisible()) { setPage(0); return true; }
    return false;
}

// ---------------------------------------------------------------- help page

juce::Image AmbientSynthEditor::snapshotSection(const juce::String& name)
{
    Section* sec = findSection(name);
    if (sec == nullptr) return {};
    // If the section lives on a tab that is not open, open it for the picture and put it back.
    TabRow* row = nullptr; int was = 0;
    for (auto& t : tabRows_)
        for (size_t pi = 0; pi < t.pages.size(); ++pi)
            for (auto& n : t.pages[pi]) if (n == name && static_cast<int>(pi) != t.active) { row = &t; was = t.active; t.active = static_cast<int>(pi); }
    if (row != nullptr) layoutBody();
    juce::Image img = content_.createComponentSnapshot(sec->bounds.expanded(2), true, 1.0f);
    if (row != nullptr) { row->active = was; layoutBody(); }
    return img;
}

juce::Image AmbientSynthEditor::snapshotStrip()
{
    if (!mod_) return {};
    const bool vis = mod_->isVisible();
    mod_->setVisible(true);
    juce::Image img = mod_->createComponentSnapshot(mod_->getLocalBounds(), true, 1.0f);
    mod_->setVisible(vis);
    return img;
}

juce::Image AmbientSynthEditor::snapshotBrowse()
{
    if (!browse_) return {};
    const bool vis = browse_->isVisible();
    browse_->setVisible(true);
    juce::Image img = browse_->createComponentSnapshot(browse_->getLocalBounds(), true, 1.0f);
    browse_->setVisible(vis);
    return img;
}

AmbientSynthEditor::HelpView::HelpView(AmbientSynthProcessor& p, AmbientSynthEditor& o)
    : proc(p), owner(o), flow(p)
{
    addChildComponent(flow);
    topics.setModel(this);
    topics.setRowHeight(26);
    topics.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(topics);
    text.setMultiLine(true, true);
    text.setReadOnly(true);
    text.setCaretVisible(false);
    text.setScrollbarsShown(true);
    text.setFont(ui::body(15.0f));
    text.setColour(juce::TextEditor::backgroundColourId, ui::card.withAlpha(0.5f));
    text.setColour(juce::TextEditor::outlineColourId, ui::cardEdge);
    text.setColour(juce::TextEditor::textColourId, ui::text);
    text.setIndents(14, 12);
    addAndMakeVisible(text);
    // The last topic is generated: every parameter, by section, with its help text.
    juce::String last;
    for (const ParamDesc& d : paramTable()) {
        if (last != d.section) { last = d.section; parameters += (parameters.isEmpty() ? "" : "\n") + juce::String(d.section).toUpperCase() + "\n"; }
        juce::String range;
        if (d.kind == ParamKind::Choice) { for (int i = 0; i < d.numChoices; ++i) range += (i ? " / " : "") + juce::String(d.choices[i]); }
        else if (d.kind == ParamKind::Bool) range = "on / off";
        else range = juce::String(d.min, d.kind == ParamKind::Int ? 0 : 2) + " .. " + juce::String(d.max, d.kind == ParamKind::Int ? 0 : 2) + (d.unit[0] ? juce::String(" ") + d.unit : juce::String());
        parameters += "  " + juce::String(d.name) + "  (" + d.key + ", " + range + ")\n      " + ambient::paramHelp(d.id) + "\n";
    }
    topics.updateContent();
    topics.selectRow(0, false, true);
    selectedRowsChanged(0);
}

void AmbientSynthEditor::HelpView::paint(juce::Graphics& g)
{
    g.fillAll(ui::bg0);
    g.setColour(ui::group);
    g.fillRoundedRectangle(getLocalBounds().reduced(10).toFloat(), 8.0f);
    for (size_t i = 0; i < pics.size() && i < picRects.size(); ++i) {
        if (!pics[i].isValid() || picRects[i].isEmpty()) continue;
        g.drawImage(pics[i], picRects[i].toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setColour(ui::cardEdge); g.drawRoundedRectangle(picRects[i].toFloat().reduced(0.5f), 4.0f, 1.0f);
    }
    g.setColour(ui::text);
    g.setFont(ui::title(13.0f));
    g.drawText("MANUAL", 26, 18, 200, 20, juce::Justification::centredLeft, false);
    g.setColour(ui::dim);
    g.setFont(ui::body(11.0f));
    g.drawText("F1 or Help closes it again  --  the pictures are the panel as it stands right now, and the displays are live", 120, 18, getWidth() - 160, 20, juce::Justification::centredLeft, false);
}

void AmbientSynthEditor::HelpView::resized()
{
    auto r = getLocalBounds().reduced(20).withTrimmedTop(26);
    topics.setBounds(r.removeFromLeft(230));
    r.removeFromLeft(12);
    // Text on the left at a readable line length, the pictures in the column to its right.
    const int textW = juce::jlimit(360, 820, r.getWidth() * 42 / 100);
    text.setBounds(r.removeFromLeft(textW));
    r.removeFromLeft(16);
    flow.setBounds(r);
    picRects.clear();
    auto col = r;
    for (const auto& img : pics) {
        if (!img.isValid() || col.getHeight() < 60) { picRects.push_back({}); continue; }
        const float scale = juce::jmin(1.0f, static_cast<float>(col.getWidth()) / static_cast<float>(img.getWidth()),
                                       static_cast<float>(juce::jmin(320, col.getHeight() - (live ? 240 : 0))) / static_cast<float>(img.getHeight()));
        const int w = juce::roundToInt(img.getWidth() * scale), h = juce::roundToInt(img.getHeight() * scale);
        picRects.push_back(col.removeFromTop(h).withWidth(w));
        col.removeFromTop(10);
    }
    if (live) live->setBounds(col.removeFromTop(juce::jmin(260, col.getHeight())));
}

void AmbientSynthEditor::HelpView::showTopic(int row)
{
    topic = row;
    pics.clear();
    live.reset();
    flow.setVisible(row == 0);
    // Which sections and which live display belong to a topic. The order follows kTopics in Help.cpp.
    struct Spec { std::vector<juce::String> sections; int liveKind; };   // liveKind: 0 none, 1 source, 2 filter, 3 stage, 4 cosmos, 5 brain, 6 env
    static const Spec kSpecs[] = {
        { {}, 0 },                                          // overview: the diagram
        { { "Source 1", "Strands", "Source 3" }, 1 },       // sources
        { { "Filter", "Z-Plane" }, 2 },                     // filters
        { { "Space", "Foundation", "Air" }, 3 },            // space
        { { "Delay", "Far Reverb", "Feedback" }, 0 },       // effects
        { { "Cosmos" }, 4 },                                // cosmos
        { { "Cluster Brain", "Tuning", "Coherence" }, 5 },  // conductor
        { {}, 0 },                                          // modulation: the strip
        { { "Morph", "Macros" }, 6 },                       // morph
        { {}, 0 },                                          // presets: the browser
        { { "Clock" }, 0 },                                 // clock
        { { "Master" }, 0 },                                // midi / osc / files
        { {}, 0 },                                          // shortcuts
    };
    const int n = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));
    if (row >= 0 && row < n) {
        for (const auto& name : kSpecs[row].sections) { juce::Image img = owner.snapshotSection(name); if (img.isValid()) pics.push_back(img); }
        if (row == 7) pics.push_back(owner.snapshotStrip());
        if (row == 9) pics.push_back(owner.snapshotBrowse());
        switch (kSpecs[row].liveKind) {
        case 1: live = std::make_unique<SourceView>(proc, 1); break;
        case 2: live = std::make_unique<FilterView>(proc); break;
        case 3: live = std::make_unique<StageView>(proc); break;
        case 4: live = std::make_unique<CosmosView>(proc); break;
        case 5: live = std::make_unique<BrainView>(proc); break;
        case 6: live = std::make_unique<EnvView>(proc); break;
        default: break;
        }
        if (live) addAndMakeVisible(*live);
    }
    resized();
    repaint();
}

int AmbientSynthEditor::HelpView::getNumRows() { return ambient::numHelpTopics() + 1; }

void AmbientSynthEditor::HelpView::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (selected) { g.setColour(ui::accent.withAlpha(0.22f)); g.fillRoundedRectangle(2.0f, 1.0f, static_cast<float>(w - 4), static_cast<float>(h - 2), 4.0f); }
    g.setColour(selected ? ui::text : ui::dim);
    g.setFont(ui::body(12.5f));
    const juce::String title = row < ambient::numHelpTopics() ? ambient::helpTopicTitle(row) : "All parameters";
    g.drawText(title, 10, 0, w - 14, h, juce::Justification::centredLeft, true);
}

void AmbientSynthEditor::HelpView::selectedRowsChanged(int row)
{
    if (row < 0) return;
    text.setText(row < ambient::numHelpTopics() ? juce::String(juce::CharPointer_UTF8(ambient::helpTopicText(row))) : parameters, false);
    text.moveCaretToTop(false);
    showTopic(row);
}

// The signal flow as a picture: the units as boxes in their group colours, the buses as arrows.
void AmbientSynthEditor::HelpView::FlowDiagram::paint(juce::Graphics& g)
{
    // Drawn on a 1000 x 560 canvas, scaled to fit whatever the column offers.
    const float sx = getWidth() / 1000.0f, sy = getHeight() / 560.0f, sc = juce::jmin(sx, sy);
    if (sc <= 0.05f) return;
    g.addTransform(juce::AffineTransform::scale(sc));
    auto node = [&](float x, float y, float w, float h, const juce::String& t, juce::Colour c, float fs = 12.0f) {
        juce::Rectangle<float> r(x, y, w, h);
        g.setColour(ui::card); g.fillRoundedRectangle(r, 6.0f);
        g.setColour(c.withAlpha(0.9f)); g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.2f);
        g.setColour(ui::text); g.setFont(ui::body(fs));
        g.drawFittedText(t, r.reduced(6.0f, 2.0f).toNearestInt(), juce::Justification::centred, 3, 0.9f);
        return r;
    };
    auto arrow = [&](juce::Point<float> a, juce::Point<float> b, juce::Colour c, bool dashed = false) {
        g.setColour(c.withAlpha(0.85f));
        juce::Line<float> l(a, b);
        if (dashed) { const float d[] = { 5.0f, 4.0f }; juce::Path p; p.startNewSubPath(a); p.lineTo(b); juce::PathStrokeType(1.4f).createDashedStroke(p, p, d, 2); g.fillPath(p); }
        else g.drawLine(l, 1.4f);
        juce::Path head; const auto u = (b - a) / juce::jmax(1.0f, l.getLength()); const juce::Point<float> nrm(-u.y, u.x);
        head.startNewSubPath(b); head.lineTo(b - u * 7.0f + nrm * 3.5f); head.lineTo(b - u * 7.0f - nrm * 3.5f); head.closeSubPath();
        g.fillPath(head);
    };
    auto label = [&](float x, float y, const juce::String& t, juce::Colour c) { g.setColour(c); g.setFont(ui::body(10.5f)); g.drawText(t, juce::roundToInt(x), juce::roundToInt(y), 480, 14, juce::Justification::centredLeft, false); };

    const juce::Colour V = ui::voiceCol, F = ui::foreCol, B = ui::backCol, C = ui::cosmosCol, K = ui::condCol, M = ui::masterCol, A = ui::accent;
    // conductors
    auto brain = node(20, 16, 150, 40, "Cluster Brain", K);
    auto keys  = node(185, 16, 120, 40, "MIDI keys", K);
    auto hands = node(320, 16, 130, 40, "OSC / hands / macros", K, 11.0f);
    label(20, 60, "every note gets a DISTANCE: 0 at the ear, 1 the infinite background", ui::dim);
    // the voice
    g.setColour(V.withAlpha(0.35f)); g.drawRoundedRectangle(14.0f, 82.0f, 442.0f, 210.0f, 8.0f, 1.0f);
    g.setColour(V); g.setFont(ui::title(10.5f)); g.drawText("VOICE  x16", 24, 86, 200, 14, juce::Justification::centredLeft, false);
    auto s1 = node(24, 104, 130, 40, "Source 1\nadditive bank / any", V, 11.0f);
    auto s2 = node(164, 104, 130, 40, "Source 2\nwavetable / FM / grains / noise", V, 10.0f);
    auto s3 = node(304, 104, 130, 40, "Source 3\n+ Air (noise on the note)", V, 10.0f);
    auto filt = node(24, 160, 200, 40, "Filter (nine models)", V);
    auto zp   = node(234, 160, 200, 40, "Z-plane filter", V);
    auto env  = node(24, 216, 410, 40, "Envelope  -  x (1 - distance/2)  -  interaural time difference  -  presence on the near plane", V, 10.5f);
    arrow(brain.getBottomLeft().translated(75, 0), { 95, 104 }, K);
    arrow(keys.getBottomLeft().translated(60, 0), { 229, 104 }, K);
    arrow(hands.getBottomLeft().translated(65, 0), { 369, 104 }, K, true);
    for (auto* r : { &s1, &s2, &s3 }) arrow({ r->getCentreX(), r->getBottom() }, { juce::jlimit(124.0f, 334.0f, r->getCentreX()), 160.0f }, V);
    label(232, 146, "series / parallel", ui::dim);
    arrow({ 124, 200 }, { 124, 216 }, V); arrow({ 334, 200 }, { 334, 216 }, V);
    // the split
    arrow({ 434, 236 }, { 500, 130 }, F); label(440, 172, "near = cos(d)", F);
    arrow({ 434, 236 }, { 500, 330 }, B); label(440, 290, "far = sin(d)", B);
    // near chain
    auto ens = node(500, 110, 100, 40, "Ensemble", F);
    auto d1  = node(612, 110, 100, 40, "Delay", F);
    auto d2  = node(724, 110, 100, 40, "Delay 2", F);
    auto nr  = node(836, 110, 130, 40, "Near reverb", F);
    arrow({ 600, 130 }, { 612, 130 }, F); arrow({ 712, 130 }, { 724, 130 }, F); arrow({ 824, 130 }, { 836, 130 }, F);
    label(500, 92, "NEAR  --  the dry, bright foreground", F);
    auto cos = node(500, 190, 330, 40, "Cosmos (parallel): frequency shifter -> resonator -> vowel -> nebula", C, 10.5f);
    arrow({ 550, 150 }, { 550, 190 }, C); arrow({ 780, 190 }, { 780, 150 }, C); label(590, 236, "send / return -- added, never replacing", ui::dim);
    auto cloud = node(846, 190, 120, 40, "Cloud (grains)", B, 11.0f);
    arrow({ 900, 150 }, { 900, 190 }, B); arrow({ 906, 230 }, { 906, 320 }, B);
    arrow({ 668, 150 }, { 668, 320 }, B, true); label(672, 250, "to far", B);
    // far
    label(500, 302, "FAR  --  the infinite background, 100 % wet", B);
    auto fr  = node(500, 320, 150, 40, "Far reverb", B);
    auto rm  = node(662, 320, 150, 40, "Room (convolution)", B, 11.0f);
    auto sh  = node(824, 320, 142, 40, "Shimmer loop", B);
    arrow({ 650, 340 }, { 662, 340 }, B); arrow({ 895, 360 }, { 575, 380 }, B, true);
    // output
    auto out = node(500, 420, 466, 44, "Mid / Side  (bass mono, side air, width)   ->   Master   ->   soft clip", M, 11.5f);
    arrow({ 901, 150 }, { 940, 420 }, F); arrow({ 575, 360 }, { 575, 420 }, B); arrow({ 737, 360 }, { 737, 420 }, B);
    label(500, 470, "no compressor anywhere: what you hear is the dynamics of the drone", ui::dim);
    // feedback, modulation, clock
    arrow({ 500, 452 }, { 120, 452 }, A, true); arrow({ 120, 452 }, { 120, 256 }, A, true);
    label(130, 458, "Feedback: to bus / to pitch (phase-modulates every partial), tape", A);
    node(20, 494, 436, 44, "Modulation: 8 LFOs - 6 envelopes - matrix -> any knob        Clock: internal / host / MIDI, Sync on every rate", A, 10.5f);
    node(500, 494, 466, 44, "Foundation sub (root or difference tone), mono, after mid/side", M, 11.0f);
    juce::ignoreUnused(ens, d1, d2, nr, cos, cloud, fr, rm, sh, out, env, filt, zp);
}

void AmbientSynthEditor::timerCallback()
{
    proc_.engine().soundingNotes(sounding_);
    {   // mark the knobs the matrix drives, and how far it is pushing them right now
        const ambient::ModMatrix& m = proc_.engine().modMatrix();
        for (auto& c : cells_) {
            auto* sl = dynamic_cast<juce::Slider*>(c.comp.get());
            if (sl == nullptr || c.param < 0) continue;
            int colour = 0;
            for (int k = 0; k < m.count(); ++k) {
                if (static_cast<int>(m.route(k).target) != c.param) continue;
                using MS = ambient::ModSource;
                const int si = static_cast<int>(m.route(k).source);
                juce::Colour col = ui::cosmosCol;
                if (si >= static_cast<int>(MS::Lfo1) && si <= static_cast<int>(MS::Lfo8)) col = ui::accent;
                else if (si >= static_cast<int>(MS::Env1) && si <= static_cast<int>(MS::Env6)) col = ui::foreCol;
                else if (si >= static_cast<int>(MS::MacroA) && si <= static_cast<int>(MS::MacroH)) col = ui::morphCol;
                else if (si >= static_cast<int>(MS::Kura1) && si <= static_cast<int>(MS::Kura4)) col = ui::condCol;
                colour = static_cast<int>(col.getARGB());
                break;
            }
            const bool had = sl->getProperties().contains("modColour");
            if (colour != 0) {
                const ParamDesc& d = paramDesc(static_cast<ParamId>(c.param));
                const float off = proc_.engine().modAmount(static_cast<ParamId>(c.param)) / juce::jmax(1.0e-6f, d.max - d.min);
                sl->getProperties().set("modColour", colour);
                sl->getProperties().set("modOffset", off);
                sl->repaint();
            } else if (had) {
                sl->getProperties().remove("modColour");
                sl->getProperties().remove("modOffset");
                sl->repaint();
            }
        }
    }
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
        if (learn == c.param) text += " - learn";
        else if (cc >= 0) text += " - CC" + juce::String(cc);
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
    // The 24 slot fields (see Params.h): 0 type 1 level 2 octave 3 ratio 4 pan 5 table 6 position
    // 7 pos drift 8 fm ratio 9 fm index 10 grain 11 density 12 follow 13 grains 14 spread 15 noise
    // 16 noise q 17 partials 18 tilt 19 bright 20 odd/even 21 inharmonic 22 shimmer 23 shimmer rate.
    // Source 1 has the same fields under other ids. Grey out what the chosen type ignores; the
    // Strands section (unison, detune, stack...) belongs to Source 1's additive bank alone.
    enum { Off = 0, Table = 1, Fm = 2, Texture = 3, Noise = 4, Additive = 5 };
    static const ParamId kIds[3][26] = {
        { ParamId::Src1Type, ParamId::OscLevel, ParamId::Src1Octave, ParamId::Src1Ratio, ParamId::Src1Pan, ParamId::Src1Table,
          ParamId::Src1Position, ParamId::Src1PosDrift, ParamId::Src1FmRatio, ParamId::Src1FmIndex, ParamId::Src1Grain, ParamId::Src1Density,
          ParamId::Src1Follow, ParamId::Src1Grains, ParamId::Src1Spread, ParamId::Src1Noise, ParamId::Src1NoiseQ,
          ParamId::Partials, ParamId::Tilt, ParamId::Brightness, ParamId::OddEven, ParamId::Inharmonic, ParamId::Shimmer, ParamId::ShimmerRate, ParamId::Src1DensitySync, ParamId::Src1Drift },
        { ParamId::Src2Type, ParamId::Src2Level, ParamId::Src2Octave, ParamId::Src2Ratio, ParamId::Src2Pan, ParamId::Src2Table,
          ParamId::Src2Position, ParamId::Src2PosDrift, ParamId::Src2FmRatio, ParamId::Src2FmIndex, ParamId::Src2Grain, ParamId::Src2Density,
          ParamId::Src2Follow, ParamId::Src2Grains, ParamId::Src2Spread, ParamId::Src2Noise, ParamId::Src2NoiseQ,
          ParamId::Src2Partials, ParamId::Src2Tilt, ParamId::Src2Bright, ParamId::Src2OddEven, ParamId::Src2Inharm, ParamId::Src2Shimmer, ParamId::Src2ShimmerRate, ParamId::Src2DensitySync, ParamId::Src2Drift },
        { ParamId::Src3Type, ParamId::Src3Level, ParamId::Src3Octave, ParamId::Src3Ratio, ParamId::Src3Pan, ParamId::Src3Table,
          ParamId::Src3Position, ParamId::Src3PosDrift, ParamId::Src3FmRatio, ParamId::Src3FmIndex, ParamId::Src3Grain, ParamId::Src3Density,
          ParamId::Src3Follow, ParamId::Src3Grains, ParamId::Src3Spread, ParamId::Src3Noise, ParamId::Src3NoiseQ,
          ParamId::Src3Partials, ParamId::Src3Tilt, ParamId::Src3Bright, ParamId::Src3OddEven, ParamId::Src3Inharm, ParamId::Src3Shimmer, ParamId::Src3ShimmerRate, ParamId::Src3DensitySync, ParamId::Src3Drift },
    };
    for (int k = 0; k < 3; ++k) {
        const int type = static_cast<int>(std::lround(proc_.engine().getParam(kIds[k][0])));
        for (int off = 1; off <= 25; ++off) {
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
            case 17: case 18: case 19: case 20: case 21: case 22: case 23: on = type == Additive; break;
            case 24: on = type == Texture || type == Noise; break;                // density sync
            case 25: on = type != Off && type != Noise; break;                    // pitch drift
            default: break;
            }
            const int ci = cellForParam(kIds[k][off]);
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
    if (Section* strands = findSection("Strands")) {   // the strand bank exists only while Source 1 is additive
        const bool on = std::lround(proc_.engine().getParam(ParamId::Src1Type)) == Additive;
        for (int ci : strands->cells) {
            Cell& c = cells_[static_cast<size_t>(ci)];
            if (c.comp && c.comp->isEnabled() != on) { c.comp->setEnabled(on); c.comp->setAlpha(on ? 1.0f : 0.35f); c.label->setAlpha(on ? 1.0f : 0.35f); }
        }
    }
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
    {   // The help line: the control under the mouse, its value and what it does -- otherwise
        // the three things worth knowing first. This is where the routing map used to be; the
        // signal flow is in the manual now, where it can be read rather than deciphered.
        g.setColour(ui::card.withAlpha(0.6f));
        g.fillRoundedRectangle(helpLine_.toFloat(), 5.0f);
        juce::String title, body;
        if (hoveredCell_ >= 0 && hoveredCell_ < static_cast<int>(cells_.size()) && cells_[static_cast<size_t>(hoveredCell_)].param >= 0) {
            const Cell& c = cells_[static_cast<size_t>(hoveredCell_)];
            const ParamId id = static_cast<ParamId>(c.param);
            const ParamDesc& d = paramDesc(id);
            juce::String value;
            if (auto* sl = dynamic_cast<juce::Slider*>(c.comp.get())) value = sl->getTextFromValue(sl->getValue());
            else if (auto* cb = dynamic_cast<juce::ComboBox*>(c.comp.get())) value = cb->getText();
            else if (auto* tb = dynamic_cast<juce::ToggleButton*>(c.comp.get())) value = tb->getToggleState() ? "on" : "off";
            title = juce::String(d.section).toUpperCase() + "   " + d.name + "      " + value;
            body = ambient::paramHelp(id);
            const int mods = [&] { int n = 0; const auto& m = proc_.engine().modMatrix(); for (int k = 0; k < m.count(); ++k) if (m.route(k).target == id) ++n; return n; }();
            if (mods > 0) body += "   [" + juce::String(mods) + (mods == 1 ? " modulation route" : " modulation routes") + " -- right-click to see them]";
        } else {
            title = "AmbientSynth";
            body = "Point at any control to read what it does. Help (or F1) opens the manual. Right-click a knob for MIDI learn and its modulation routes; drag a card from the strip at the bottom onto a knob to modulate it.";
        }
        g.setColour(kText);
        g.setFont(ui::title(10.5f));
        g.drawText(title, helpLine_.reduced(10, 5), juce::Justification::topLeft, false);
        g.setColour(kDim);
        g.setFont(ui::body(11.5f));
        g.drawFittedText(body, helpLine_.reduced(10, 5).withTrimmedTop(17), juce::Justification::topLeft, 3, 1.0f);
    }

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
        + "   arc " + juce::String(proc_.engine().arcValue(), 2)
        + "   " + juce::String(proc_.engine().tempo(), 1) + " bpm  bar " + juce::String(1 + static_cast<int>(std::floor(proc_.engine().beatPosition() / 4.0)))
        + (proc_.engine().clockRunning() ? "" : " (stopped)");
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
    g.drawText(info, 14, header_.getBottom() - 16, designW_ - 420, 14, juce::Justification::centredLeft);

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
    for (const auto& t : tabRows_) {
        const juce::Colour col = groups_[static_cast<size_t>(t.group)].colour;
        for (size_t i = 0; i < t.tabs.size(); ++i) {
            const bool on = static_cast<int>(i) == t.active;
            const auto r = t.tabs[i].toFloat();
            g.setColour(on ? col.withAlpha(0.26f) : kSectionFill.brighter(0.04f));
            g.fillRoundedRectangle(r, 5.0f);
            if (on) { g.setColour(col.withAlpha(0.9f)); g.drawRoundedRectangle(r.reduced(0.5f), 5.0f, 1.0f); }
            g.setColour(on ? kText : kDim);
            g.setFont(ui::title(10.5f));
            g.drawText(t.names[i], t.tabs[i], juce::Justification::centred);
        }
    }
    for (const auto& s : sections_) {
        if (s.name == "Master" || !s.visible) continue;   // painted in the header / behind another tab
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
