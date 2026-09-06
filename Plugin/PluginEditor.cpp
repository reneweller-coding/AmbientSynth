#include "EditorCommon.h"

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
}

AmbientSynthEditor::AmbientSynthEditor(AmbientSynthProcessor& p)
    : AudioProcessorEditor(p), proc_(p)
{
    setLookAndFeel(&laf_);

    // Two columns, everything in sight at once: the voice and the morph on the left, the effects,
    // the cosmos and the conductor on the right. Rows whose sections are of a kind (the three
    // sources, the two filters, the effect pairs, the conductor's tables) page through tabs.
    groups_ = {
        { "VOICE",      kVoice,     { { "Source 1", "Strands", "Source 2", "Source 3", "Source 4", "Vector" }, { "Air", "Filter", "Envelope", "Z-Plane", "Expression" }, { "Space", "Foundation" } }, {}, 0 },   // rows 0 and 1 page through tabs
        { "MORPH",      kMorph,     { { "Morph", "Macros" } }, {}, 0 },
        { "FOREGROUND", kFore,      { { "Ensemble", "Delay", "Delay 2", "Near Reverb", "Blur" } }, {}, 1 },
        { "BACKGROUND", kBack,      { { "Cloud", "Far Reverb", "Feedback", "Room", "Body", "Patina" } }, {}, 1 },
        // Strike shares the Cosmos group as a tab: like the Cosmos it is a sound source that is
        // not one of the four oscillators, and beside them it read as a fifth.
        { "COSMOS",     kCosmos,    { { "Cosmos", "Strike" } }, {}, 1 },
        // Stretchy as well, and for the same reason from the other side: in Compact the left
        // column is the taller one, and then it is the note roll that grows into the gap.
        { "CONDUCTOR",  kConductor, { { "Cluster Brain", "Autoplay", "Brain 2", "Tuning", "Coherence", "Clock" } }, {}, 1, {}, 0, true },
        // Last in the left column, and the only group with no controls in it: the right column is
        // taller than the left, and the difference used to be an empty rectangle the width of the
        // page. It now holds the output's spectrum, and because the row stretches, the hole cannot
        // come back when a tab changes the height of a row above it.
        { "ANALYSIS",   kVoice,     { {} }, {}, 0, {}, 96, true },
    };
    tabRows_ = {
        // The Vector belongs with the sources it mixes, so it is a page of the same row.
        // Strands is not a page: it belongs to Source 1's additive bank alone and sits under that
        // page's display (TabRow::under), so the row lost a tab that only ever meant "Source 1".
        { 0, 0, { "SOURCE 1", "SOURCE 2", "SOURCE 3", "SOURCE 4", "VECTOR" }, { { "Source 1" }, { "Source 2" }, { "Source 3" }, { "Source 4" }, { "Vector" } } },
        { 0, 1, { "FILTER", "Z-PLANE", "AMP ENV", "EXPRESSION" }, { { "Air", "Filter" }, { "Z-Plane" }, { "Envelope" }, { "Expression" } } },
        { 1, 0, { "MORPH", "MACROS" }, { { "Morph" }, { "Macros" } } },
        { 2, 0, { "ENSEMBLE + DELAY", "DELAY 2 + NEAR REVERB + BLUR" }, { { "Ensemble", "Delay" }, { "Delay 2", "Near Reverb", "Blur" } } },
        { 3, 0, { "CLOUD + FAR REVERB", "FEEDBACK + ROOM", "BODY + PATINA" }, { { "Cloud", "Far Reverb" }, { "Feedback", "Room" }, { "Body", "Patina" } } },
        { 5, 0, { "BRAIN", "AUTOPLAY", "BRAIN 2", "TUNING", "COHERENCE", "CLOCK" }, { { "Cluster Brain" }, { "Autoplay" }, { "Brain 2" }, { "Tuning" }, { "Coherence" }, { "Clock" } } },
        // Appended, so the indices the code above uses for the other rows stay what they were.
        { 4, 0, { "COSMOS", "STRIKE" }, { { "Cosmos" }, { "Strike" } } },
    };
    tabRows_[0].under = { "Strands", "", "", "", "" };   // under Source 1's display only

    content_.onPaint = [this](juce::Graphics& g) { paintContent(g); };
    content_.onMouse = [this](const juce::MouseEvent& e) {
        if (e.mods.isPopupMenu()) return;
        for (const auto& d : diceOf_)
            if (d.second.contains(e.getPosition())) { randomiseSection(d.first, e.mods.isShiftDown()); return; }
        clickTabs(e.getPosition());
    };
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
    // Main: the panel, from wherever you are. It is the one page that had no button of its own --
    // you got back by switching the others off, which is a rule nobody should have to learn.
    mainButton_ = std::make_unique<juce::TextButton>("Main");
    mainButton_->setTooltip("The panel: every section on one page");
    mainButton_->setClickingTogglesState(true);
    mainButton_->setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
    mainButton_->setToggleState(true, juce::dontSendNotification);
    mainButton_->onClick = [this] { setPage(0); };
    addAndMakeVisible(*mainButton_);
    // The two headset controls under one button: on a desk they are the two you never press.
    calibButton_ = std::make_unique<juce::TextButton>("Calibrate");
    calibButton_->onClick = [this] { proc_.gestures().startCalibration(6.0f); };
    mapButton_ = std::make_unique<juce::TextButton>("Gestures...");
    mapButton_->onClick = [this] { showMappingEditor(); };
    vrButton_ = std::make_unique<juce::TextButton>("VR");
    vrButton_->setTooltip("Hand tracking: calibrate the hands, edit the gesture table");
    vrButton_->onClick = [this] {
        juce::PopupMenu m;
        m.addItem(1, "Calibrate hands (6 s: together and apart, low and high, near and far)");
        m.addItem(2, "Gestures... (the gesture / macro mapping table)");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(vrButton_.get()), [this](int r) {
            if (r == 1) proc_.gestures().startCalibration(6.0f);
            else if (r == 2) showMappingEditor();
        });
    };
    addAndMakeVisible(*vrButton_);
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
    undoButton_ = std::make_unique<juce::TextButton>("Undo");
    undoButton_->setTooltip("Undo the last preset, die roll or A/B swap (Ctrl+Z)");
    undoButton_->onClick = [this] { doUndo(); };
    addAndMakeVisible(*undoButton_);
    redoButton_ = std::make_unique<juce::TextButton>("Redo");
    redoButton_->setTooltip("Redo (Ctrl+Y)");
    redoButton_->onClick = [this] { doRedo(); };
    addAndMakeVisible(*redoButton_);
    abButton_ = std::make_unique<juce::TextButton>("A | B");
    abButton_->setTooltip("Two whole snapshots to compare: the first click parks what you have in A and hands you B, every click after swaps");
    abButton_->onClick = [this] { swapAB(); };
    addAndMakeVisible(*abButton_);
    compactButton_ = std::make_unique<juce::TextButton>("Compact");
    compactButton_->setTooltip("Wraps the widest rows into two, so the page is narrower and taller. Still one page, still no scrolling.");
    compactButton_->setClickingTogglesState(true);
    compactButton_->setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
    compactButton_->onClick = [this] {
        compact_ = compactButton_->getToggleState();
        proc_.setCompactLayout(compact_);
        rebuildLayout();
    };
    addAndMakeVisible(*compactButton_);
    // Session recall, and the switch for it. Only in the standalone: in a plugin the host saves
    // the state with the project, which is what a plugin is supposed to do.
    if (AmbientSynthProcessor::sessionRecallAvailable()) {
        recallButton_ = std::make_unique<juce::TextButton>("Recall");
        recallButton_->setTooltip("Start where you left off: the whole state is kept while the instrument runs and comes back next time. Off means it always starts at Init, and what was stored is forgotten.");
        recallButton_->setClickingTogglesState(true);
        recallButton_->setToggleState(proc_.sessionRecall(), juce::dontSendNotification);
        recallButton_->setColour(juce::TextButton::buttonOnColourId, kAccent.withAlpha(0.5f));
        recallButton_->onClick = [this] { proc_.setSessionRecall(recallButton_->getToggleState()); };
        addAndMakeVisible(*recallButton_);
    }
    outputView_ = std::make_unique<LoudnessView>(proc_);
    addAndMakeVisible(*outputView_);
    tooltips_ = std::make_unique<juce::TooltipWindow>(nullptr, 600);
    setWantsKeyboardFocus(true);

    // Header controls
    soundBox_ = std::make_unique<juce::ComboBox>();
    soundBox_->setTextWhenNothingSelected("Sound preset");
    fillPresetBox(*soundBox_);
    soundBox_->setSelectedId(proc_.soundPresetIndex() + 1, juce::dontSendNotification);
    soundBox_->onChange = [this] {
        const int idx = soundBox_->getSelectedId() - 1;
        if (idx >= 0 && idx != proc_.soundPresetIndex()) { pushUndo("preset"); proc_.applySoundPreset(idx); }
    };
    addAndMakeVisible(*soundBox_);

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
    source4View_ = std::make_unique<SourceView>(proc_, 4);
    brainView_ = std::make_unique<BrainView>(proc_);
    brainView2_ = std::make_unique<BrainView>(proc_);
    brainView3_ = std::make_unique<BrainView>(proc_);
    stageView_ = std::make_unique<StageView>(proc_);
    cosmosView_ = std::make_unique<CosmosView>(proc_);
    envView_ = std::make_unique<EnvView>(proc_);
    vectorView_ = std::make_unique<VectorView>(proc_);
    spectrumView_ = std::make_unique<SpectrumView>(proc_);
    for (juce::Component* c : { static_cast<juce::Component*>(scope_.get()), static_cast<juce::Component*>(filterView_.get()),
                                static_cast<juce::Component*>(source2View_.get()), static_cast<juce::Component*>(source3View_.get()),
                                static_cast<juce::Component*>(source4View_.get()),
                                static_cast<juce::Component*>(source1View_.get()), static_cast<juce::Component*>(brainView_.get()),
                                static_cast<juce::Component*>(brainView2_.get()),
                                static_cast<juce::Component*>(brainView3_.get()),
                                static_cast<juce::Component*>(stageView_.get()), static_cast<juce::Component*>(cosmosView_.get()),
                                static_cast<juce::Component*>(envView_.get()),
                                static_cast<juce::Component*>(spectrumView_.get()),
                                static_cast<juce::Component*>(vectorView_.get()) })
        content_.addAndMakeVisible(*c);
    if (tabRows_.size() > 1) {   // VOICE row 0: OSC 1 | SOURCE 2 | SOURCE 3; row 1: FILTER | Z-PLANE
        tabRows_[0].displays = { source1View_.get(), source2View_.get(), source3View_.get(), source4View_.get(), vectorView_.get() };
        // The strand scope drew the bank's cycle on the Strands tab; that tab is gone and the
        // additive-bank picture on Source 1's page shows the same partials. Kept for the manual.
        if (scope_) scope_->setVisible(false);
        tabRows_[1].displays = { filterView_.get(), nullptr, envView_.get(), nullptr };
    }
    if (groups_.size() > 4) {
        groups_[0].displays = { nullptr, nullptr, stageView_.get() };   // VOICE: the Space / Foundation row
        tabRows_.back().displays = { cosmosView_.get(), nullptr };      // COSMOS | STRIKE (the row appended last)
    }
    if (groups_.size() > 6) groups_[6].displays = { spectrumView_.get() };   // ANALYSIS: the strip
    if (tabRows_.size() > 5) tabRows_[5].displays = { brainView_.get(), brainView3_.get(), brainView2_.get(), nullptr, nullptr, nullptr };   // CONDUCTOR: BRAIN | AUTOPLAY | BRAIN 2 | TUNING | COHERENCE | CLOCK

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
    compact_ = proc_.compactLayout() || juce::SystemStats::getEnvironmentVariable("AMBIENT_COMPACT", "").isNotEmpty();
    if (compact_) { compactButton_->setToggleState(true, juce::dontSendNotification); rebuildLayout(); }
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
        // AMBIENT_TAB=<row>,<page>[;<row>,<page>...]: open tabbed rows on a given page, so a page
        // that is not the first one can be photographed at all. Rows count from the top, pages
        // from the left, both from zero.
        for (const auto& one : juce::StringArray::fromTokens(juce::SystemStats::getEnvironmentVariable("AMBIENT_TAB", ""), ";", "")) {
            const auto rc = juce::StringArray::fromTokens(one, ",", "");
            const int r = rc.size() == 2 ? rc[0].getIntValue() : -1, pg = rc.size() == 2 ? rc[1].getIntValue() : -1;
            if (r >= 0 && r < static_cast<int>(tabRows_.size()) && pg >= 0 && pg < static_cast<int>(tabRows_[static_cast<size_t>(r)].names.size()))
                tabRows_[static_cast<size_t>(r)].active = pg;
        }
        // AMBIENT_MANUAL=<folder>: write the manual out and quit. A chord is played first and
        // then twelve seconds pass, because the pictures are of the running instrument and its
        // displays -- the spectrum, the stage, the note roll -- have nothing in them until it is
        // actually sounding. Waiting alone was not enough: with a slow conductor the first draft
        // illustrated the Sources chapter with a display that said "nothing sounding".
        const juce::String man = juce::SystemStats::getEnvironmentVariable("AMBIENT_MANUAL", "");
        if (man.isNotEmpty()) {
            for (int n : { 45, 52, 57, 64, 69 }) proc_.engine().noteOn(n, 0.7f);
            juce::Timer::callAfterDelay(12000, [this, man] {
                exportManual(juce::File(man), [] { juce::JUCEApplicationBase::quit(); });
            });
        }
        // AMBIENT_MOD=lfo|env|matrix: which tab of the modulation strip to open. Only a dev aid,
        // and the only way to photograph the envelope editor.
        {
            const juce::String md = juce::SystemStats::getEnvironmentVariable("AMBIENT_MOD", "");
            if (mod_ != nullptr && md.isNotEmpty())
                mod_->setTab(md == "matrix" ? 2 : (md == "env" ? 1 : 0));
        }
        const juce::String rt = juce::SystemStats::getEnvironmentVariable("AMBIENT_ROUTE", "");
        for (int r = 0; rt.isNotEmpty() && r < numRoutePresets(); ++r) if (rt == routePreset(r).name) proc_.setRouteText(routePreset(r).points);
        rebuildLayout();
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
            if (s.name == "Space") s.maxUnits = 10;                          // two rows of ten
            if (s.name == "Foundation") s.maxUnits = 5;
            // Source 1 is narrower than the others on purpose: its page shares the row with the
            // strand bank under the display, and the display column needs the width for it.
            if (s.name == "Source 1") s.maxUnits = 9;
            else if (s.name == "Source 2" || s.name == "Source 3" || s.name == "Source 4") s.maxUnits = 12;
            s.wideUnits = 0;   // filled in below, after every section knows its natural width
            if (s.name == "Strands") s.maxUnits = 10;
            if (s.name == "Delay" || s.name == "Delay 2") s.maxUnits = 12;   // one row with the two Sync choices and Absorb
            if (s.name == "Far Reverb") s.maxUnits = 12;                     // one row with Rotate, Unmask, Diffuse and Width
            if (s.name == "Master") s.maxUnits = 5;
            if (s.name == "Body") s.maxUnits = 7;                            // one row
            if (s.name == "Room") s.maxUnits = 9;                            // one row with Morph and both impulses
            if (s.name == "Cluster Brain" || s.name == "Brain 2") s.maxUnits = 10;
            if (s.name == "Autoplay") s.maxUnits = 12;   // the seven controls and the button in one row
            if (s.name == "Expression") s.maxUnits = 7;
            if (s.name == "Filter") s.maxUnits = 10;                         // one row: On, Model, five knobs, Drive, Fold
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
            if (d.id == ParamId::ZShape) {
                // A hundred and fifty-five filters in one flat list is a list nobody reads. The
                // item IDs stay the parameter's own numbering -- which is historical and frozen,
                // because the library names its shapes by text -- while the order they are shown
                // in is by family, so the display and the value are free of each other.
                for (int cat = 0; cat < ambient::kZCategories; ++cat) {
                    cb->addSectionHeading(ambient::kZCategoryNames[cat]);
                    for (int i = 0; i < d.numChoices; ++i)
                        if (ambient::kZShapeCategory[i] == cat) cb->addItem(d.choices[i], i + 1);
                }
            } else {
                for (int i = 0; i < d.numChoices; ++i) cb->addItem(d.choices[i], i + 1);
            }
            content_.addAndMakeVisible(*cb);
            c.combo = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc_.apvts, d.key, *cb);
            c.comp = std::move(cb);
            c.units = 2;
            break;
        }
        }
        c.comp->addMouseListener(this, false);
        registerHelp(c.comp.get(), d.id);
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
    // A clip loader in every source section, each for its own slot: four slots typed Texture
    // can play four different recordings, which is what the Vector's four corners are for.
    for (int k = 0; k < ambient::kSlots; ++k) {
        auto texture = std::make_unique<juce::TextButton>("Texture...");
        texture->onClick = [this, k] { chooseSourceFile(false, k); };
        textureCell_[k] = addExtraCell("Source " + juce::String(k + 1), std::move(texture), "Texture file", 2);
    }
    // Autoplay's trigger as a button of its own: the parameter is a switch a host can automate,
    // but by hand you want one press, not a switch you have to put back.
    auto step = std::make_unique<juce::TextButton>("Step now");
    step->setTooltip("Exchange one voice of the cluster now, whatever the interval says");
    step->onClick = [this] { proc_.engine().autoplayStep(); };
    addExtraCell("Autoplay", std::move(step), "by hand", 2);
    // The three section banks, each inside the section it belongs to rather than on the header:
    // 257 cosmos presets, 156 filters and 41 plucks are lists you go looking for, and the header
    // keeps only the Sound box, which is the one preset that is about the whole instrument.
    {
        auto cb = std::make_unique<juce::ComboBox>();
        cb->setTextWhenNothingSelected("Cosmos preset");
        cb->setTooltip("257 presets for the Cosmos section in sixteen families. Touches nothing outside it, so it lands on top of whatever sound is loaded.");
        for (int fam = 0; fam < numCosmosPresetFamilies(); ++fam) {
            cb->addSectionHeading(cosmosPresetFamily(fam));
            for (int i = 0; i < numCosmosPresets(); ++i)
                if (cosmosPresetCategory(i) == fam) cb->addItem(cosmosPreset(i).name, i + 1);
        }
        cb->addSectionHeading("Off");
        cb->addItem(cosmosPreset(0).name, 1);
        cb->setSelectedId(proc_.cosmosPresetIndex() + 1, juce::dontSendNotification);
        cb->onChange = [this] {
            const int idx = cosmosBox_->getSelectedId() - 1;
            if (idx >= 0 && idx != proc_.cosmosPresetIndex()) { pushUndo("cosmos preset"); proc_.applyCosmosPreset(idx); }
        };
        cosmosBox_ = cb.get();
        addExtraCell("Cosmos", std::move(cb), "Preset", 3);
    }
    // The Z-plane and Strike banks. Each is a layer like the Cosmos one -- it resets its own
    // section and nothing else -- and each sits inside the section it belongs to, grouped by
    // family, because a flat list of a hundred and fifty-six filters is a list nobody reads.
    {
        auto zb = std::make_unique<juce::ComboBox>();
        zb->setTextWhenNothingSelected("Filter preset");
        for (int cat = 0; cat < ambient::kZCategories; ++cat) {
            zb->addSectionHeading(ambient::kZCategoryNames[cat]);
            for (int i = 0; i < numZPresets(); ++i)
                if (zPresetCategory(i) == cat) zb->addItem(zPreset(i).name, i + 1);
        }
        zb->addSectionHeading("Off");
        zb->addItem(zPreset(0).name, 1);
        zb->setTooltip("One preset per filter shape: the shape, where its point sits in the cube, how sharp and how much of it you hear. Touches nothing outside the Z-Plane section.");
        zb->onChange = [this] {
            const int idx = zPresetBox_->getSelectedId() - 1;
            if (idx >= 0) { pushUndo("filter preset"); proc_.applyZPreset(idx); }
        };
        zPresetBox_ = zb.get();
        addExtraCell("Z-Plane", std::move(zb), "Preset", 3);
    }
    {
        auto sb = std::make_unique<juce::ComboBox>();
        sb->setTextWhenNothingSelected("Strike preset");
        for (int fam = 0; fam < numStrikePresetFamilies(); ++fam) {
            sb->addSectionHeading(strikePresetFamily(fam));
            for (int i = 0; i < numStrikePresets(); ++i)
                if (strikePresetCategory(i) == fam) sb->addItem(strikePreset(i).name, i + 1);
        }
        sb->addSectionHeading("Off");
        sb->addItem(strikePreset(0).name, 1);
        sb->setTooltip("The Karplus-Strong pluck on its own: what is struck, how long it rings, how dull, and whether the conductor fires it too. Touches nothing outside the Strike section.");
        sb->onChange = [this] {
            const int idx = strikePresetBox_->getSelectedId() - 1;
            if (idx >= 0) { pushUndo("strike preset"); proc_.applyStrikePreset(idx); }
        };
        strikePresetBox_ = sb.get();
        addExtraCell("Strike", std::move(sb), "Preset", 3);
    }
    auto impulse = std::make_unique<juce::TextButton>("Impulse A...");
    impulse->onClick = [this] { chooseImpulseFile(false); };
    impulseCell_ = addExtraCell("Room", std::move(impulse), "Dark Hall (built in)", 2);
    auto impulseB = std::make_unique<juce::TextButton>("Impulse B...");
    impulseB->onClick = [this] { chooseImpulseFile(true); };
    impulseBCell_ = addExtraCell("Room", std::move(impulseB), "no second room", 2);

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
    // The Sound box gets the room the Cosmos box used to take: it holds the longest names, and
    // with the packs loaded it holds five thousand of them.
    if (soundBox_) soundBox_->setBounds(200, 8, 260, 24);
    if (saveButton_) saveButton_->setBounds(474, 8, 64, 24);
    if (loadButton_) loadButton_->setBounds(544, 8, 64, 24);
    if (recButton_) recButton_->setBounds(614, 8, 56, 24);
    // The pages first (Main, Perform, Browse), then what acts on the state, Help last where a
    // manual belongs. Calibrate and Gestures live in the VR menu and take no room here.
    if (mainButton_) mainButton_->setBounds(880, 8, 56, 24);
    if (performButton_) performButton_->setBounds(942, 8, 70, 24);
    if (browseButton_) browseButton_->setBounds(1018, 8, 66, 24);
    if (vrButton_) vrButton_->setBounds(1090, 8, 44, 24);
    if (undoButton_) undoButton_->setBounds(1140, 8, 50, 24);
    if (redoButton_) redoButton_->setBounds(1194, 8, 50, 24);
    if (abButton_) abButton_->setBounds(1248, 8, 52, 24);
    if (compactButton_) compactButton_->setBounds(1304, 8, 70, 24);
    if (recallButton_) recallButton_->setBounds(1378, 8, 62, 24);
    if (helpButton_) helpButton_->setBounds(1446, 8, 56, 24);

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
    if (outputView_) outputView_->setBounds(W / 2, 74, juce::jmax(160, W - W / 2 - 360), kHeaderH - 92);
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
// Compact: a section wider than eight cells wraps into two rows. The page loses width and gains
// height; nothing scrolls either way, and the arrangement is otherwise untouched.
void AmbientSynthEditor::rebuildLayout()
{
    for (auto& s : sections_) {
        if (s.wideUnits == 0) s.wideUnits = s.maxUnits;
        // Only the rows that actually drive the page's width wrap: halving everything made the
        // page taller than it was wide (1.27 : 1), which is worse on a 16:9 screen than the
        // 1.9 : 1 it started from. Ten cells is the measured threshold.
        s.maxUnits = (compact_ && s.wideUnits > 10) ? (s.wideUnits + 1) / 2 : s.wideUnits;
    }
    resized();
    // The design's shape has changed, so the window has to follow: keep the width, take the new
    // height from the new ratio, and tell the constrainer about it or the corner would fight it.
    if (auto* con = getConstrainer()) {
        con->setFixedAspectRatio(static_cast<double>(designW_) / static_cast<double>(designH_));
        con->setSizeLimits(designW_ / 3, designH_ / 3, designW_ * 2, designH_ * 2);
    }
    setSize(getWidth(), juce::roundToInt(getWidth() * static_cast<double>(designH_) / static_cast<double>(designW_)));
    content_.repaint();
    repaint();
}

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
    // The stretchy row of each column, filled in as it is placed and grown at the end of the pass.
    // One per column rather than one in total: which column comes out shorter depends on the tabs
    // and on Compact, and the hole belongs to whichever one it is.
    struct Stretch { juce::Component* disp = nullptr; Group* group = nullptr; };
    std::vector<Stretch> stretch(static_cast<size_t>(nCols));
    for (size_t gi = 0; gi < groups_.size(); ++gi) {
        auto& g = groups_[gi];
        const size_t col = static_cast<size_t>(g.column);
        const int x0 = colX[col];
        int y = colY[col] + kGroupTitleH;
        for (size_t ri = 0; ri < g.rows.size(); ++ri) {
            TabRow* t = tabRowFor(static_cast<int>(gi), static_cast<int>(ri));
            const std::vector<juce::String>& names = t != nullptr ? t->pages[static_cast<size_t>(t->active)] : g.rows[ri];
            juce::Component* disp = nullptr;
            Section* underSec = nullptr;      // a section placed under the display, if the page has one
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
                    if (pi < t->under.size() && t->under[pi].isNotEmpty())
                        if (Section* s = findSection(t->under[pi])) setSectionVisible(*s, false);
                }
                if (static_cast<size_t>(t->active) < t->displays.size()) disp = t->displays[static_cast<size_t>(t->active)];
                // The page's under-section wraps to the display column's width, and the row is
                // tall enough to keep a real picture above it.
                if (static_cast<size_t>(t->active) < t->under.size() && t->under[static_cast<size_t>(t->active)].isNotEmpty()) {
                    if (Section* u = findSection(t->under[static_cast<size_t>(t->active)])) {
                        const int free = colWidth[col] - pageWidth(names) - kPad;
                        u->maxUnits = std::max(2, (free - 2 * kPad) / kCellW);
                        underSec = u;
                        rowH = std::max(rowH, sectionHeight(*u) + kPad + 110);
                    }
                }
                y += kTabH;
            } else {
                rowH = std::max(pageHeight(names), g.minRowH);
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
            // Whatever the row leaves free goes to its display -- that room used to stay empty --
            // and where the page has an under-section, the display keeps the top of that column
            // and the section takes the bottom, wrapped to the column's width.
            if (disp != nullptr) {
                const int right = x0 + colWidth[col] - kPad;
                int dispH = rowH;
                if (underSec != nullptr && right - x >= 120) {
                    const int uh = sectionHeight(*underSec);
                    if (rowH - uh - kPad >= 80) {
                        dispH = rowH - uh - kPad;
                        setSectionVisible(*underSec, true);
                        layoutSection(*underSec, x, y + dispH + kPad);
                    } else setSectionVisible(*underSec, false);
                }
                if (right - x >= 120 && dispH > 0) { disp->setBounds(x, y, right - x, dispH); disp->setVisible(true); }
                else disp->setVisible(false);
                if (g.stretch && disp->isVisible()) { stretch[col].disp = disp; stretch[col].group = &g; }
            } else if (underSec != nullptr) {
                setSectionVisible(*underSec, false);
            }
            y += rowH + kPad;
        }
        g.bounds = { x0, colY[col], colWidth[col], y - colY[col] };
        colY[col] = y + kPad;
    }
    bodyW_ = colX.back() + colWidth.back() + kPad;
    bodyH_ = 0;
    for (int yy : colY) bodyH_ = std::max(bodyH_, yy);
    // The stretch. A stretchy group is the last one in its column, so growing its row only reaches
    // downwards and nothing else has to move: the columns end level, whatever the tabs are set to.
    for (size_t ci = 0; ci < stretch.size(); ++ci) {
        if (stretch[ci].disp == nullptr || stretch[ci].group == nullptr) continue;
        const int deficit = bodyH_ - colY[ci];
        if (deficit <= 0) continue;
        stretch[ci].disp->setBounds(stretch[ci].disp->getBounds().withHeight(stretch[ci].disp->getHeight() + deficit));
        stretch[ci].group->bounds = stretch[ci].group->bounds.withHeight(stretch[ci].group->bounds.getHeight() + deficit);
        colY[ci] += deficit;
    }
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
    if (mainButton_ && mainButton_->getToggleState() != (page == 0)) mainButton_->setToggleState(page == 0, juce::dontSendNotification);
    if (performButton_->getToggleState() != (page == 1)) performButton_->setToggleState(page == 1, juce::dontSendNotification);
    if (browseButton_->getToggleState() != (page == 2)) browseButton_->setToggleState(page == 2, juce::dontSendNotification);
    if (helpButton_ && helpButton_->getToggleState() != (page == 3)) helpButton_->setToggleState(page == 3, juce::dontSendNotification);

    if (page == 2) browse_->applyFilter();

}


// ---------------------------------------------------------------- modulation strip

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

void AmbientSynthEditor::registerHelp(juce::Component* c, ambient::ParamId id)
{
    if (c == nullptr) return;
    helpParamOf_[c] = static_cast<int>(id);
    c->addMouseListener(this, false);
}

void AmbientSynthEditor::mouseEnter(const juce::MouseEvent& e)
{
    auto it = helpParamOf_.find(e.eventComponent);
    const int param = it != helpParamOf_.end() ? it->second : -1;
    if (param != hoveredParam_) { hoveredParam_ = param; repaint(); }
}

void AmbientSynthEditor::mouseExit(const juce::MouseEvent& e)
{
    auto it = helpParamOf_.find(e.eventComponent);
    if (it != helpParamOf_.end() && it->second == hoveredParam_) { hoveredParam_ = -1; repaint(); }
}

// ---------------------------------------------------------------- undo, redo, A/B, the die

AmbientSynthEditor::Snapshot AmbientSynthEditor::takeSnapshot(const juce::String& what) const
{
    Snapshot s;
    s.what = what;
    s.v.resize(static_cast<size_t>(kNumParams));
    for (int i = 0; i < kNumParams; ++i)
        if (auto* v = proc_.apvts.getRawParameterValue(paramTable()[static_cast<size_t>(i)].key)) s.v[static_cast<size_t>(i)] = v->load();
    return s;
}

void AmbientSynthEditor::restore(const Snapshot& s)
{
    if (static_cast<int>(s.v.size()) != kNumParams) return;
    for (int i = 0; i < kNumParams; ++i)
        if (auto* p = proc_.apvts.getParameter(paramTable()[static_cast<size_t>(i)].key))
            p->setValueNotifyingHost(p->convertTo0to1(s.v[static_cast<size_t>(i)]));
    repaint();
}

void AmbientSynthEditor::pushUndo(const juce::String& what)
{
    undo_.push_back(takeSnapshot(what));
    if (undo_.size() > 32) undo_.erase(undo_.begin());   // a session's worth, not a history
    redo_.clear();
}

void AmbientSynthEditor::doUndo()
{
    if (undo_.empty()) return;
    redo_.push_back(takeSnapshot(undo_.back().what));
    restore(undo_.back());
    undo_.pop_back();
}

void AmbientSynthEditor::doRedo()
{
    if (redo_.empty()) return;
    undo_.push_back(takeSnapshot(redo_.back().what));
    restore(redo_.back());
    redo_.pop_back();
}

// A/B: the first click parks what you have in A and leaves you on B (a copy, so nothing is lost);
// every click after that swaps the two. This is the comparison a sound gets judged by.
void AmbientSynthEditor::swapAB()
{
    const Snapshot now = takeSnapshot("A/B");
    if (slotA_.v.empty()) { slotA_ = now; slotB_ = now; showingB_ = true; }
    else {
        (showingB_ ? slotB_ : slotA_) = now;
        showingB_ = !showingB_;
        restore(showingB_ ? slotB_ : slotA_);
    }
    if (abButton_) abButton_->setButtonText(showingB_ ? "B | a" : "A | b");
}

// The die on a section: every parameter of that section is drawn again. Choices and switches are
// picked at random, numbers land inside the middle of their range (the ends of a range are
// usually where a preset stops being usable), and shift keeps them near where they already are.
void AmbientSynthEditor::randomiseSection(const juce::String& name, bool subtle)
{
    Section* sec = findSection(name);
    if (sec == nullptr) return;
    pushUndo("randomise " + name);
    juce::Random rng;
    for (int ci : sec->cells) {
        const Cell& c = cells_[static_cast<size_t>(ci)];
        if (c.param < 0) continue;
        const ParamId id = static_cast<ParamId>(c.param);
        if (isPerformanceParam(id)) continue;
        const ParamDesc& d = paramDesc(id);
        auto* p = proc_.apvts.getParameter(d.key);
        if (p == nullptr) continue;
        float value;
        if (d.kind == ParamKind::Choice || d.kind == ParamKind::Bool || d.kind == ParamKind::Int) {
            const int lo = static_cast<int>(std::lround(d.min)), hi = static_cast<int>(std::lround(d.max));
            const int cur = static_cast<int>(std::lround(proc_.engine().getParam(id)));
            value = static_cast<float>(subtle ? juce::jlimit(lo, hi, cur + rng.nextInt(3) - 1) : lo + rng.nextInt(hi - lo + 1));
        } else {
            // Drawn in the parameter's own skewed domain, so a logarithmic knob is not biased
            // towards its top end -- the same domain the morph and the map blend in.
            const float span = std::max(d.max - d.min, 1e-9f);
            const float cur = std::pow(juce::jlimit(0.0f, 1.0f, (proc_.engine().getParam(id) - d.min) / span), d.skew);
            const float t = subtle ? juce::jlimit(0.0f, 1.0f, cur + 0.2f * (rng.nextFloat() * 2.0f - 1.0f))
                                   : 0.15f + 0.7f * rng.nextFloat();
            value = d.min + span * std::pow(t, 1.0f / d.skew);
        }
        p->setValueNotifyingHost(p->convertTo0to1(value));
    }
    repaint();
}

bool AmbientSynthEditor::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::F1Key) { setPage(help_ && help_->isVisible() ? 0 : 3); return true; }
    if (k == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) { doUndo(); return true; }
    if (k == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0)) { doRedo(); return true; }
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
    // The Master section is painted in the header and its bounds are the editor's; every other
    // section belongs to the scrolling content. Photographed from the wrong component, the
    // Master came out as a strip of the panel's top-left corner.
    juce::Image img = (name == "Master" ? static_cast<juce::Component&>(*this) : static_cast<juce::Component&>(content_))
                          .createComponentSnapshot(sec->bounds.expanded(2), true, 1.0f);
    if (row != nullptr) { row->active = was; layoutBody(); }
    return img;
}

// One tab of the panel, as it looks when it is open: every section of the page, the tab bar over
// it, the display beside it and -- on Source 1 -- the strand bank under that display. A picture
// per section would have been easier and would have shown the manual's reader something that is
// not on their screen; what they see is a tab.
juce::Image AmbientSynthEditor::snapshotTab(int rowIndex, int page)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(tabRows_.size())) return {};
    TabRow& t = tabRows_[static_cast<size_t>(rowIndex)];
    if (page < 0 || page >= static_cast<int>(t.pages.size())) return {};
    const int was = t.active;
    t.active = page;
    layoutBody();
    juce::Rectangle<int> box = t.bar;                      // the tab bar itself belongs in the picture
    auto add = [&](const juce::String& name) {
        if (name.isEmpty()) return;
        if (Section* s = findSection(name)) box = box.isEmpty() ? s->bounds : box.getUnion(s->bounds);
    };
    for (const auto& n : t.pages[static_cast<size_t>(page)]) add(n);
    if (page < static_cast<int>(t.under.size())) add(t.under[static_cast<size_t>(page)]);
    if (page < static_cast<int>(t.displays.size()) && t.displays[static_cast<size_t>(page)] != nullptr) {
        juce::Component* d = t.displays[static_cast<size_t>(page)];
        if (d->isVisible() && d->getWidth() > 0) box = box.getUnion(d->getBounds());
    }
    juce::Image img;
    if (!box.isEmpty()) img = content_.createComponentSnapshot(box.expanded(4), true, 1.0f);
    t.active = was;
    layoutBody();
    return img;
}

// A whole page of the instrument -- Perform, Browse, the modulation strip, the help page itself --
// rather than one section of it. Pages are siblings of the panel and are shown one at a time, so
// the one being photographed is made visible for the picture and put back afterwards.
juce::Image AmbientSynthEditor::snapshotPage(juce::Component* page)
{
    if (page == nullptr || page->getWidth() <= 0 || page->getHeight() <= 0) return {};
    const bool was = page->isVisible();
    page->setVisible(true);
    juce::Image img = page->createComponentSnapshot(page->getLocalBounds(), true, 1.0f);
    page->setVisible(was);
    return img;
}

juce::Image AmbientSynthEditor::snapshotPerform() { return snapshotPage(perform_.get()); }

juce::Image AmbientSynthEditor::snapshotBrowseMap()
{
    if (!browse_) return {};
    const bool vis = browse_->isVisible();
    const int was = browse_->modeMap.getToggleState() ? 1 : 0;
    browse_->setVisible(true);
    browse_->setMode(1);
    browse_->resized();
    juce::Image img = browse_->createComponentSnapshot(browse_->getLocalBounds(), true, 1.0f);
    browse_->setMode(was);
    browse_->setVisible(vis);
    return img;
}

juce::Image AmbientSynthEditor::snapshotHeader()
{
    // The master's corner of the header: the mid/side section, the loudness meter and the
    // master knob. The whole header is four thousand pixels wide and unreadable on a page.
    // The meter is hidden while the help page is up, which is when the manual is exported, so
    // its bounds are read whether or not it is visible; without that the picture began in the
    // middle of the meter.
    juce::Rectangle<int> box;
    if (Section* ms = findSection("Master")) box = ms->bounds;
    if (outputView_ != nullptr && !outputView_->getBounds().isEmpty()) box = box.isEmpty() ? outputView_->getBounds() : box.getUnion(outputView_->getBounds());
    if (!keys_.isEmpty()) box = box.isEmpty() ? keys_ : box.getUnion(keys_);   // the note roll over the meter
    if (box.isEmpty()) return {};
    box = box.expanded(8).withRight(getWidth()).withTop(0);
    box.setBottom(juce::jmin(kHeaderH - 2, box.getBottom()));
    return createComponentSnapshot(box, true, 1.0f);
}

juce::Image AmbientSynthEditor::snapshotStripTab(int tab, bool detail)
{
    if (!mod_) return {};
    const int was = mod_->tab;
    mod_->setTab(tab);
    juce::Image img;
    if (detail) {
        // The strip is nearly three thousand pixels wide and shrinks to a ribbon on a page; the
        // left third -- the cards and the first panel -- at twice the size is where it can be read.
        const bool vis = mod_->isVisible();
        mod_->setVisible(true);
        img = mod_->createComponentSnapshot(mod_->getLocalBounds().withWidth(mod_->getWidth() * 24 / 70), true, 2.0f);
        mod_->setVisible(vis);
    } else img = snapshotStrip();
    mod_->setTab(was);
    return img;
}

juce::StringArray AmbientSynthEditor::tabSectionNames(int rowIndex, int page) const
{
    juce::StringArray out;
    if (rowIndex < 0 || rowIndex >= static_cast<int>(tabRows_.size())) return out;
    const TabRow& t = tabRows_[static_cast<size_t>(rowIndex)];
    if (page < 0 || page >= static_cast<int>(t.pages.size())) return out;
    for (const auto& n : t.pages[static_cast<size_t>(page)]) out.add(n);
    if (page < static_cast<int>(t.under.size()) && t.under[static_cast<size_t>(page)].isNotEmpty()) out.add(t.under[static_cast<size_t>(page)]);
    return out;
}

// What a tab is called on its own bar, so the manual's caption is the word the reader will look
// for on the screen.
juce::String AmbientSynthEditor::tabName(int rowIndex, int page) const
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(tabRows_.size())) return {};
    const TabRow& t = tabRows_[static_cast<size_t>(rowIndex)];
    if (page < 0 || page >= static_cast<int>(t.names.size())) return {};
    return t.names[static_cast<size_t>(page)];
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

// ---------------------------------------------------------------- the manual, as files
//
// The help page is the manual, and its pictures are snapshots of the panel itself -- the real
// sections with their real values, taken as the page is opened. That is what makes them right
// and what makes them impossible to produce from a script: they only exist while an editor is
// running. So the export runs in one, writes every topic's pictures and text into a folder, and
// Tools/make_manual.py turns that folder into an HTML manual and a PDF.
//
// It runs as a list of steps a third of a second apart rather than as one function, because
// some of the pictures need the instrument to have MOVED between two of them: the gallery of
// source types sets Source 2 to each type in turn, and its display and its greyed-out knobs
// follow on the next timer tick, not in the same call.
struct AmbientSynthEditor::ManualJob {
    juce::File dir;
    std::vector<std::function<void()>> steps;
    size_t next = 0;
    std::function<void()> done;
    // What has been written so far, per topic: the pictures and the tabs with their captions,
    // their blurbs and the sections whose parameters belong under them.
    struct Tab { juce::String file, name, caption, blurb; juce::StringArray sections; };
    struct Pic { juce::String file, caption; };
    std::vector<std::vector<Pic>> images;
    std::vector<std::vector<Tab>> tabs;
    juce::String originalType;    // Source 2's type before the gallery, put back afterwards
};

void AmbientSynthEditor::exportManual(const juce::File& dir, std::function<void()> onDone)
{
    dir.createDirectory();
    if (help_ == nullptr) { if (onDone) onDone(); return; }
    manual_ = std::make_unique<ManualJob>();
    ManualJob& job = *manual_;
    job.dir = dir;
    job.done = std::move(onDone);
    const int last = ambient::numHelpTopics();    // the generated "All parameters" topic comes after
    job.images.resize(static_cast<size_t>(last + 1));
    job.tabs.resize(static_cast<size_t>(last + 1));

    auto writePng = [dir](const juce::Image& img, const juce::String& name) -> juce::String {
        if (!img.isValid()) return {};
        const juce::File f = dir.getChildFile(name);
        juce::PNGImageFormat png;
        std::unique_ptr<juce::FileOutputStream> out(f.createOutputStream());
        return (out != nullptr && png.writeImageToStream(img, *out)) ? f.getFileName() : juce::String();
    };
    // Built by concatenation, not by formatted(): JUCE's formatted is wide-character, so a %s
    // handed a const char* writes the bytes as UTF-16 and the file comes out called
    // "topic-00-汦睧.png". It did, once.
    auto stem = [](int row) { return "topic-" + juce::String(row).paddedLeft('0', 2); };

    setPage(3);                                   // the help page, so its views are laid out
    help_->setBounds(0, kHeaderH, designW_, designH_ - kHeaderH);

    // ---- one step per topic: its sections, its live display, its tabs
    for (int row = 0; row <= last; ++row) {
        job.steps.push_back([this, row, stem, writePng, &job] {
            help_->topics.selectRow(row);
            help_->showTopic(row);
            help_->resized();
            auto& files = job.images[static_cast<size_t>(row)];
            // The section pictures the help page shows are the manual's only when the chapter
            // has no tab pictures: where it has, every section is already in one of them.
            for (size_t i = 0; help_->tabPics.empty() && i < help_->pics.size(); ++i) {
                const juce::String f = writePng(help_->pics[i], stem(row) + "-" + juce::String(static_cast<int>(i)) + ".png");
                if (f.isNotEmpty()) files.push_back({ f, i < static_cast<size_t>(help_->picCaptions.size()) ? help_->picCaptions[static_cast<int>(i)] : juce::String() });
            }
            auto shot = [&](juce::Component& c, const juce::String& suffix, const juce::String& caption, juce::Rectangle<int> area = {}) {
                if (c.getWidth() <= 0 || c.getHeight() <= 0) return;
                if (area.isEmpty()) area = c.getLocalBounds();
                const juce::String f = writePng(c.createComponentSnapshot(area, true, 2.0f), stem(row) + "-" + suffix + ".png");
                if (f.isNotEmpty()) files.push_back({ f, caption });
            };
            if (row == 0) shot(help_->flow, "flow", "The signal flow: every unit as a box in its group's colour, the buses as arrows -- the same picture the help page's first topic shows.", help_->flow.drawn());
            if (help_->live != nullptr) shot(*help_->live, "live", help_->liveCaption);
            for (size_t i = 0; i < help_->tabPics.size(); ++i) {
                ManualJob::Tab t;
                t.file = writePng(help_->tabPics[i], stem(row) + "-tab" + juce::String(static_cast<int>(i)) + ".png");
                if (t.file.isEmpty()) continue;
                t.name = i < static_cast<size_t>(help_->tabNames.size()) ? help_->tabNames[static_cast<int>(i)] : juce::String();
                t.caption = i < static_cast<size_t>(help_->tabCaptions.size()) ? help_->tabCaptions[static_cast<int>(i)] : juce::String();
                t.sections = i < help_->tabSections.size() ? help_->tabSections[i] : juce::StringArray();
                t.blurb = juce::CharPointer_UTF8(ambient::tabHelp(t.name.toRawUTF8()));
                job.tabs[static_cast<size_t>(row)].push_back(t);
            }
        });
    }

    // ---- the gallery of source types, on Source 2, one type per pair of steps. The clip for
    // the Texture and Stretch types comes from AMBIENT_MANUAL_CLIP; without it those two show
    // an empty display, which is at least honest.
    {
        const int sourcesRow = 1;
        auto* typeParam = proc_.apvts.getParameter("src2_type");
        const juce::String clip = juce::SystemStats::getEnvironmentVariable("AMBIENT_MANUAL_CLIP", "");
        job.steps.push_back([this, typeParam, &job] {
            job.originalType = typeParam != nullptr ? juce::String(typeParam->getValue()) : juce::String();
        });
        for (const char* type : { "Additive", "Wavetable", "FM", "Texture", "Stretch", "Noise" }) {
            const juce::String typeName(type);
            int index = -1;
            for (int i = 0; i < ambient::kNumSourceTypes; ++i) if (typeName == ambient::kSourceTypeNames[i]) index = i;
            if (index < 0 || typeParam == nullptr) continue;
            job.steps.push_back([this, typeParam, index, typeName, clip] {
                if ((typeName == "Texture" || typeName == "Stretch") && clip.isNotEmpty())
                    proc_.loadTextureFile(1, juce::File(clip));
                typeParam->setValueNotifyingHost(typeParam->convertTo0to1(static_cast<float>(index)));
                if (auto* lvl = proc_.apvts.getParameter("src2_level")) lvl->setValueNotifyingHost(lvl->convertTo0to1(0.5f));
            });
            job.steps.push_back([this, typeName, sourcesRow, stem, writePng, &job] {
                help_->topics.selectRow(sourcesRow);
                help_->showTopic(sourcesRow);
                ManualJob::Tab t;
                t.name = "TYPE " + typeName;
                t.caption = "Source 2 switched to the " + typeName + " type: the knobs that type uses are lit, the rest greyed out, and the display shows "
                          + (typeName == "Additive" ? juce::String("the partials of its bank.")
                           : typeName == "Wavetable" ? juce::String("the current frame of the table.")
                           : typeName == "FM" ? juce::String("the modulated waveform of the pair.")
                           : typeName == "Texture" ? juce::String("the loaded clip with the grains reading it.")
                           : typeName == "Stretch" ? juce::String("the loaded clip with the stretched read position crawling through it.")
                           : juce::String("the spectrum of the noise colour."));
                t.file = writePng(snapshotTab(0, 1), stem(sourcesRow) + "-type-" + typeName.toLowerCase() + ".png");
                if (t.file.isEmpty()) return;
                t.sections.add("Source 2");
                t.blurb = juce::CharPointer_UTF8(ambient::tabHelp(t.name.toRawUTF8()));
                job.tabs[static_cast<size_t>(sourcesRow)].push_back(t);
            });
        }
        job.steps.push_back([typeParam, &job] {
            if (typeParam != nullptr && job.originalType.isNotEmpty()) typeParam->setValueNotifyingHost(job.originalType.getFloatValue());
        });
    }

    // ---- the last step: the file that names it all, and the cover pictures
    job.steps.push_back([this, last, dir, writePng, &job] {
        juce::String json = "{\n  \"topics\": [\n";
        for (int row = 0; row <= last; ++row) {
            const juce::String title = row < last ? juce::String(ambient::helpTopicTitle(row)) : "All parameters";
            const juce::String body  = row < last ? juce::String(juce::CharPointer_UTF8(ambient::helpTopicText(row)))
                                                  : help_->parameters;
            json << "    { \"title\": " << juce::JSON::toString(juce::var(title))
                 << ", \"text\": " << juce::JSON::toString(juce::var(body))
                 << ", \"images\": [";
            const auto& files = job.images[static_cast<size_t>(row)];
            for (size_t i = 0; i < files.size(); ++i)
                json << (i ? ", " : "") << "{ \"file\": " << juce::JSON::toString(juce::var(files[i].file))
                     << ", \"caption\": " << juce::JSON::toString(juce::var(files[i].caption)) << " }";
            json << "], \"tabs\": [";
            const auto& tabs = job.tabs[static_cast<size_t>(row)];
            for (size_t i = 0; i < tabs.size(); ++i) {
                json << (i ? ", " : "") << "{ \"file\": " << juce::JSON::toString(juce::var(tabs[i].file))
                     << ", \"name\": " << juce::JSON::toString(juce::var(tabs[i].name))
                     << ", \"caption\": " << juce::JSON::toString(juce::var(tabs[i].caption))
                     << ", \"blurb\": " << juce::JSON::toString(juce::var(tabs[i].blurb))
                     << ", \"sections\": [";
                for (int k = 0; k < tabs[i].sections.size(); ++k) json << (k ? ", " : "") << juce::JSON::toString(juce::var(tabs[i].sections[k]));
                json << "] }";
            }
            json << "] }" << (row < last ? ",\n" : "\n");
        }
        // Every parameter, structured, so the manual can print each tab's own under its picture.
        json << "  ],\n  \"params\": [\n";
        bool first = true;
        for (const ParamDesc& d : paramTable()) {
            juce::String range;
            if (d.kind == ParamKind::Choice) { for (int i = 0; i < d.numChoices; ++i) range += (i ? " / " : "") + juce::String(d.choices[i]); }
            else if (d.kind == ParamKind::Bool) range = "on / off";
            else range = juce::String(d.min, d.kind == ParamKind::Int ? 0 : 2) + " .. " + juce::String(d.max, d.kind == ParamKind::Int ? 0 : 2) + (d.unit[0] ? juce::String(" ") + d.unit : juce::String());
            json << (first ? "" : ",\n") << "    { \"section\": " << juce::JSON::toString(juce::var(juce::String(d.section)))
                 << ", \"name\": " << juce::JSON::toString(juce::var(juce::String(d.name)))
                 << ", \"key\": " << juce::JSON::toString(juce::var(juce::String(d.key)))
                 << ", \"range\": " << juce::JSON::toString(juce::var(range))
                 << ", \"help\": " << juce::JSON::toString(juce::var(juce::String(juce::CharPointer_UTF8(ambient::paramHelp(d.id))))) << " }";
            first = false;
        }
        json << "\n  ],\n  \"version\": " << juce::JSON::toString(juce::var(juce::String(JucePlugin_VersionString)))
             << ",\n  \"shapes\": " << ambient::kZShapes
             << ",\n  \"presets\": " << numPresets()
             << ",\n  \"cosmos\": " << numCosmosPresets()
             << ",\n  \"zpresets\": " << numZPresets()
             << ",\n  \"strike\": " << numStrikePresets() << "\n}\n";
        dir.getChildFile("manual.json").replaceWithText(json);
        // Coverage: every page of every tab row must have been photographed exactly once. Written
        // as a file so make_manual.py can refuse to print a manual with a hole in it.
        juce::String coverage;
        for (int r = 0; r < static_cast<int>(tabRows_.size()); ++r)
            for (int pg = 0; pg < static_cast<int>(tabRows_[static_cast<size_t>(r)].names.size()); ++pg) {
                const juce::String nm = tabRows_[static_cast<size_t>(r)].names[static_cast<size_t>(pg)];
                int seen = 0;
                for (const auto& chapter : job.tabs) for (const auto& t : chapter) if (t.name == nm) ++seen;
                coverage << nm << ": " << seen << "\n";
            }
        dir.getChildFile("coverage.txt").replaceWithText(coverage);

        // One picture of the whole panel, for the cover: the instrument as it actually looks.
        // And one of the header on its own, which is where the pages, the master and the
        // loudness meter live.
        setPage(0);
        resized();
        writePng(createComponentSnapshot(getLocalBounds(), true, 1.0f), "panel.png");
        writePng(createComponentSnapshot(getLocalBounds().withHeight(kHeaderH), true, 2.0f), "header.png");
        if (job.done) job.done();
    });
    runManualStep();
}

void AmbientSynthEditor::runManualStep()
{
    if (manual_ == nullptr || manual_->next >= manual_->steps.size()) return;
    manual_->steps[manual_->next++]();
    if (manual_ != nullptr && manual_->next < manual_->steps.size())
        juce::Timer::callAfterDelay(350, [this] { runManualStep(); });
}

void AmbientSynthEditor::HelpView::showTopic(int row)
{
    topic = row;
    pics.clear();
    picCaptions.clear();
    tabPics.clear();
    tabNames.clear();
    tabCaptions.clear();
    tabSections.clear();
    live.reset();
    flow.setVisible(row == 0);
    // Which sections and which live display belong to a topic. The order follows kTopics in
    // Help.cpp. `sections` are the pictures the help page itself shows, at most two or three:
    // the column they are stacked in runs out of height after that.
    struct Spec { std::vector<juce::String> sections; int liveKind; };   // liveKind: 0 none, 1 source, 2 filter, 3 stage, 4 cosmos, 5 brain, 6 env
    static const Spec kSpecs[] = {
        { {}, 0 },                                          // overview: the diagram
        { { "Source 1", "Vector" }, 1 },                    // sources
        { { "Filter", "Z-Plane" }, 2 },                     // filters
        { { "Space", "Foundation", "Air" }, 3 },            // space
        { { "Delay", "Far Reverb" }, 0 },                   // effects
        { { "Cosmos" }, 4 },                                // cosmos
        { { "Cluster Brain", "Tuning" }, 5 },               // conductor
        { {}, 0 },                                          // modulation: the strip
        { { "Morph", "Macros" }, 6 },                       // morph
        { {}, 0 },                                          // presets: the browser
        { { "Clock" }, 0 },                                 // clock
        { { "Master" }, 0 },                                // midi / osc / files
        { {}, 0 },                                          // shortcuts
    };
    static const char* const kLiveCaption[7] = {
        "", "The live display of Source 1: the partials of the loudest voice, exactly the amplitudes the oscillator is summing right now.",
        "The live filter response: the voice filter, the z-plane, and what a note actually meets after both, from the same maths the audio path uses.",
        "The live stage: every sounding voice as a dot at its distance from the ear, the far plane at the back.",
        "The live Cosmos display: the shifter, the resonator and the vowel as the section currently has them.",
        "The live conductor: the brain's notes, their holds and where on the planes it has put them.",
        "The live envelope: the six modulation envelopes' shapes and where each one's clock currently is.",
    };
    const int n = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));
    // The sections and the live display, for the help page and the manual both.
    if (row >= 0 && row < n) {
        for (const auto& name : kSpecs[row].sections) {
            juce::Image img = owner.snapshotSection(name);
            if (!img.isValid()) continue;
            pics.push_back(img);
            picCaptions.add("The " + name + " section, with the values of the preset the manual was exported from.");
        }
        switch (kSpecs[row].liveKind) {
        case 1: live = std::make_unique<SourceView>(proc, 1); break;
        case 2: live = std::make_unique<FilterView>(proc); break;
        case 3: live = std::make_unique<StageView>(proc); break;
        case 4: live = std::make_unique<CosmosView>(proc); break;
        case 5: live = std::make_unique<BrainView>(proc); break;
        case 6: live = std::make_unique<EnvView>(proc); break;
        default: break;
        }
        liveCaption = kLiveCaption[kSpecs[row].liveKind];
        if (live) addAndMakeVisible(*live);
    }
    // The tabs, for the manual only. Every page of every tab row belongs to exactly one chapter
    // -- by its row, with three exceptions -- and is enumerated from the rows themselves, so a
    // tab that is added to the panel is in the manual the next time it is exported, and one
    // cannot be forgotten by a list written by hand. (One was, twice.)
    auto addTab = [&](juce::Image img, const juce::String& name, const juce::String& caption, juce::StringArray sections) {
        if (!img.isValid()) return;
        tabPics.push_back(img); tabNames.add(name); tabCaptions.add(caption); tabSections.push_back(sections);
    };
    static const int kRowChapter[] = { 1, 2, 8, 4, 4, 6, 5 };    // tab row -> chapter
    for (int r = 0; r < static_cast<int>(owner.tabRows_.size()); ++r) {
        const auto& t = owner.tabRows_[static_cast<size_t>(r)];
        for (int pg = 0; pg < static_cast<int>(t.pages.size()); ++pg) {
            int chapter = r < 7 ? kRowChapter[r] : 4;
            if (r == 1 && pg == 2) chapter = 7;      // AMP ENV with the modulation
            if (r == 1 && pg == 3) chapter = 11;     // EXPRESSION with MIDI
            if (r == 5 && pg == 5) chapter = 10;     // CLOCK with the clock chapter
            if (chapter != row) continue;
            const juce::StringArray secs = owner.tabSectionNames(r, pg);
            juce::String what = secs.joinIntoString(", ");
            juce::String caption = "The " + t.names[static_cast<size_t>(pg)] + " tab as it opens: the " + what
                                 + (secs.size() > 1 ? " sections" : " section")
                                 + " with the values of the preset the manual was exported from"
                                 + (pg < static_cast<int>(t.displays.size()) && t.displays[static_cast<size_t>(pg)] != nullptr
                                        ? ", and beside them the tab's live display." : ".");
            addTab(owner.snapshotTab(r, pg), t.names[static_cast<size_t>(pg)], caption, secs);
        }
    }
    if (row == 1)   // the strand bank, under Source 1's display
        addTab(owner.snapshotSection("Strands"), "STRANDS",
               "The Strands section, which sits under Source 1's display: the strand bank's ten controls.", { "Strands" });
    if (row == 3) {
        addTab(owner.snapshotSection("Space"), "SPACE", "The Space section: the spatial model's controls, two rows side by side.", { "Space" });
        addTab(owner.snapshotSection("Foundation"), "FOUNDATION", "The Foundation section: the sub and its source.", { "Foundation" });
    }
    if (row == 4)   // the master, in its corner of the header
        addTab(owner.snapshotHeader(), "MASTER",
               "The right-hand half of the header: the note roll (every sounding note as a bar, the keys' notes lit), the loudness meter under it, the Master section (tilt, bass mono, side air, width, mono safe, subsonic) and the master gain knob.", { "Master" });
    if (row == 7) {
        static const char* const kStrip[3] = { "LFO", "ENV", "MATRIX" };
        static const char* const kStripCap[3] = {
            "The modulation strip along the bottom of the page, LFO tab: the eight LFO cards, each a shape, a rate, a phase, a depth and a mode. A card is dragged onto a knob to route it.",
            "The strip's ENV tab: the six envelope cards and the shape editor, where an envelope is drawn as points on a curve.",
            "The strip's MATRIX tab: every route as a row -- source, target, depth, via, and the 0..1 flag." };
        static const char* const kStripSection[3] = { "LFO 1", "Env 1", "" };
        static const char* const kStripDetail[3] = {
            "The left third of the LFO tab at readable size: the source cards -- LFO 1 to 4 are the first four -- and LFO 1's own panel: Shape, Rate, Phase, Depth, Mode, Sync.",
            "The left third of the ENV tab at readable size: the envelope editor of Env 1 -- points on a curve, dragged; double-click adds or removes one -- with its Mode, Time, Depth and Sync.",
            "The left third of the MATRIX tab at readable size: the first routes, each a source, a target, a depth slider, a Via source and the 0..1 flag, and the '+ route' button." };
        for (int t = 0; t < 3; ++t) {
            juce::StringArray secs; if (kStripSection[t][0]) secs.add(kStripSection[t]);
            addTab(owner.snapshotStripTab(t), kStrip[t], kStripCap[t], {});
            addTab(owner.snapshotStripTab(t, true), juce::String(kStrip[t]) + " (detail)", kStripDetail[t], secs);
        }
    }
    if (row == 8)
        addTab(owner.snapshotPerform(), "PERFORM",
               "The Perform page: the macros large, the morph and the map cursor, the note roll and the stage, and the set recorder.", {});
    if (row == 9) {
        addTab(owner.snapshotBrowse(), "BROWSE",
               "The Browse page in its list view: the columns that narrow the library (family, character, motion, features), the search, and the list of every preset the instrument knows.", {});
        addTab(owner.snapshotBrowseMap(), "MAP",
               "The Browse page in its map view: every preset as a point, clustered by what it sounds like; the cursor and its radius, and the Map blend switch that makes the space between presets playable.", { "Map" });
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

juce::Rectangle<int> AmbientSynthEditor::HelpView::FlowDiagram::drawn() const
{
    const float sc = juce::jmin(getWidth() / kCanvasW, getHeight() / kCanvasH);
    if (sc <= 0.0f) return getLocalBounds();
    return juce::Rectangle<int>(0, 0, juce::roundToInt(kCanvasW * sc), juce::roundToInt(kCanvasH * sc));
}

// The signal flow as a picture: the units as boxes in their group colours, the buses as arrows.
void AmbientSynthEditor::HelpView::FlowDiagram::paint(juce::Graphics& g)
{
    // Drawn on a fixed canvas, scaled to fit whatever the column offers.
    const float sx = getWidth() / kCanvasW, sy = getHeight() / kCanvasH, sc = juce::jmin(sx, sy);
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
    auto brain = node(20, 14, 150, 36, "Cluster Brain", K);
    auto keys  = node(185, 14, 120, 36, "MIDI keys", K);
    auto hands = node(320, 14, 130, 36, "OSC / hands / macros", K, 11.0f);
    label(20, 54, "every note gets a DISTANCE: 0 at the ear, 1 the infinite background", ui::dim);
    // the voice
    g.setColour(V.withAlpha(0.35f)); g.drawRoundedRectangle(14.0f, 74.0f, 442.0f, 250.0f, 8.0f, 1.0f);
    g.setColour(V); g.setFont(ui::title(10.5f)); g.drawText("VOICE  x16", 24, 78, 200, 14, juce::Justification::centredLeft, false);
    // Four equal source slots. Slot 1 is the strand bank while its type is Additive, and then the
    // Strands section is its own; set to anything else it renders like the other three.
    auto s1 = node(24, 96, 100, 38, "Source 1\nstrand bank / any", V, 9.5f);
    auto s2 = node(130, 96, 100, 38, "Source 2\nany type", V, 9.5f);
    auto s3 = node(236, 96, 100, 38, "Source 3\nany type", V, 9.5f);
    auto s4 = node(342, 96, 92, 38, "Source 4\nany type", V, 9.5f);
    auto vec = node(24, 142, 410, 26, "Vector: the four slots on the corners of one square   -   Strike: a struck string, wood or metal on top", V, 9.5f);
    auto filt = node(24, 176, 200, 36, "Filter (ten models) + fold", V, 11.0f);
    auto zp   = node(234, 176, 200, 36, "Z-plane filter (155 shapes)\nafter the filter, or beside it", V, 9.5f);
    auto env  = node(24, 228, 410, 36, "Envelope  -  x (1 - distance/2)  -  interaural time difference  -  presence on the near plane", V, 9.5f);
    auto air  = node(24, 274, 410, 36, "Air: noise on the note (band) or resonators on its harmonics (ghost)", V, 10.0f);
    arrow(brain.getBottomLeft().translated(75, 0), { 74, 96 }, K);
    arrow(keys.getBottomLeft().translated(60, 0), { 245, 96 }, K);
    arrow(hands.getBottomLeft().translated(65, 0), { 388, 96 }, K, true);
    for (auto* r : { &s1, &s2, &s3, &s4 }) arrow({ r->getCentreX(), r->getBottom() }, { r->getCentreX(), 142.0f }, V);
    arrow({ 124, 168 }, { 124, 176 }, V); arrow({ 334, 168 }, { 334, 176 }, V);
    arrow({ 124, 212 }, { 124, 228 }, V); arrow({ 334, 212 }, { 334, 228 }, V);
    arrow({ 229, 264 }, { 229, 274 }, V);
    // The split, said once and in words: two labels hung on the two arrows sat across them.
    label(150, 332, "splits by distance:  near = cos(d),  far = sin(d)", ui::dim);
    arrow({ 434, 292 }, { 500, 130 }, F);
    arrow({ 434, 300 }, { 500, 356 }, B);
    // near chain
    label(500, 92, "NEAR  --  the dry, bright foreground", F);
    auto ens = node(500, 110, 108, 38, "Ensemble\nchorus / microshift", F, 9.5f);
    auto d1  = node(614, 110, 96, 38, "Delay", F);
    auto d2  = node(716, 110, 96, 38, "Delay 2", F);
    auto nr  = node(818, 110, 148, 38, "Near reverb\n+ the Haas band", F, 9.5f);
    arrow({ 608, 129 }, { 614, 129 }, F); arrow({ 710, 129 }, { 716, 129 }, F); arrow({ 812, 129 }, { 818, 129 }, F);
    auto blur = node(500, 158, 210, 26, "Blur: attacks wiped into texture", F, 9.5f);
    auto cos = node(500, 194, 320, 38, "Cosmos (parallel): frequency shifter -> resonator -> vowel -> nebula", C, 9.5f);
    arrow({ 540, 148 }, { 540, 194 }, C); arrow({ 780, 194 }, { 780, 148 }, C);
    label(500, 236, "send / return -- added, never replacing", ui::dim);
    auto cloud = node(846, 194, 120, 38, "Cloud (grains)", B, 10.5f);
    arrow({ 906, 148 }, { 906, 194 }, B); arrow({ 906, 232 }, { 906, 348 }, B);
    // To the far plane, down the gap between the Cosmos and the Cloud rather than through them.
    arrow({ 834, 148 }, { 834, 348 }, B, true); label(700, 268, "delay sends to far", B);
    // far
    label(500, 326, "FAR  --  the infinite background, 100 % wet, unmasked band by band", B);
    auto fr  = node(500, 348, 150, 38, "Far reverb\n+ its own width", B, 9.5f);
    auto rm  = node(662, 348, 150, 38, "Room (convolution)", B, 10.5f);
    auto sh  = node(824, 348, 142, 38, "Shimmer loop", B);
    arrow({ 650, 367 }, { 662, 367 }, B);
    arrow({ 895, 386 }, { 580, 394 }, B, true);
    // output: the master chain in the order it actually runs
    auto out1 = node(500, 410, 466, 30, "Body (twelve tuned modes)   ->   Mid / Side  (bass mono, side air, width)", M, 9.5f);
    auto out2 = node(500, 446, 466, 30, "+ Foundation sub   ->   Patina   ->   subsonic   ->   Master   ->   soft clip", M, 9.5f);
    arrow({ 575, 386 }, { 575, 410 }, B); arrow({ 737, 386 }, { 737, 410 }, B);
    arrow({ 966, 129 }, { 972, 406 }, F);      // the near bus, down the right-hand margin
    arrow({ 972, 406 }, { 966, 414 }, F);
    arrow({ 733, 440 }, { 733, 446 }, M);
    label(500, 478, "no compressor anywhere: what you hear is the dynamics of the drone", ui::dim);
    // feedback, modulation
    arrow({ 500, 461 }, { 120, 461 }, A, true); arrow({ 120, 461 }, { 120, 312 }, A, true);
    label(130, 466, "Feedback: to bus / to pitch (phase-modulates every partial), tape", A);
    node(20, 500, 946, 34, "Modulation: 8 LFOs  -  6 envelopes  -  aftertouch, wheel, slide  -  the note, its velocity, its distance  -  matrix -> any knob", A, 10.0f);
    juce::ignoreUnused(ens, d1, d2, nr, cos, cloud, fr, rm, sh, out1, out2, env, filt, zp, s4, vec, air, blur);
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
    // The three section boxes follow the processor rather than the other way round, so a layer
    // loaded from a program change, from OSC or by a whole state coming back shows up in them --
    // and so does a sound preset clearing them, which it does, because a sound preset brings its
    // own filter and its own pluck and the name of a layer preset would then be a lie.
    if (cosmosBox_ && cosmosBox_->getSelectedId() != proc_.cosmosPresetIndex() + 1)
        cosmosBox_->setSelectedId(proc_.cosmosPresetIndex() + 1, juce::dontSendNotification);
    if (zPresetBox_ && zPresetBox_->getSelectedId() != proc_.zPresetIndex() + 1)
        zPresetBox_->setSelectedId(proc_.zPresetIndex() + 1, juce::dontSendNotification);
    if (strikePresetBox_ && strikePresetBox_->getSelectedId() != proc_.strikePresetIndex() + 1)
        strikePresetBox_->setSelectedId(proc_.strikePresetIndex() + 1, juce::dontSendNotification);
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
    enum { Off = 0, Table = 1, Fm = 2, Texture = 3, Noise = 4, Additive = 5, Stretch = 6 };
    // The ids come from Params.h (slotParamIds), so this list and the engine's cannot drift apart.
    // kSlots, not a number: a literal 3 here quietly left Source 4's cells lit whatever its type.
    for (int k = 0; k < ambient::kSlots; ++k) {
        const ParamId* ids = slotParamIds(k);
        const int type = static_cast<int>(std::lround(proc_.engine().getParam(ids[0])));
        for (int off = 1; off <= 27; ++off) {
            bool on = type != Off;
            switch (off) {
            case 5:  on = type == Table; break;                                  // wavetable choice
            case 6:  on = type == Table || type == Texture || type == Noise || type == Stretch; break;   // position / centre
            case 7:  on = type != Off;  break;                                   // pos drift moves all of them
            case 8:
            case 9:  on = type == Fm; break;
            case 10: on = type == Texture || type == Stretch; break;             // grain: the grain, or the spectral window
            case 13:
            case 14: on = type == Texture; break;                                // grains, spread
            case 11: on = type == Texture || type == Noise; break;               // density: grains or crackle
            case 12: on = type == Texture || type == Noise || type == Stretch; break;   // pitch follow
            case 15:
            case 16: on = type == Noise; break;
            case 17: case 18: case 19: case 20: case 21: case 22: case 23: on = type == Additive; break;
            case 24: on = type == Texture || type == Noise; break;                // density sync
            case 25: on = type != Off && type != Noise; break;                    // pitch drift
            case 26:
            case 27: on = type == Stretch; break;                                 // stretch, loop fade
            default: break;
            }
            const int ci = cellForParam(ids[off]);
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
    for (int k = 0; k < ambient::kSlots; ++k) nameCell(textureCell_[k], "Texture file", proc_.textureName(k));
    nameCell(impulseCell_, "Dark Hall (built in)", proc_.impulseName());
    nameCell(impulseBCell_, "no second room", proc_.impulseBName());
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

void AmbientSynthEditor::chooseSourceFile(bool wavetable, int slot)
{
    const juce::String what = wavetable ? juce::String("Load a wavetable (2048-sample frames)")
                            : slot >= 0 ? "Load a clip for Source " + juce::String(slot + 1)
                                        : juce::String("Load a texture sample");
    chooser_ = std::make_unique<juce::FileChooser>(what, juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, wavetable, slot](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            const bool ok = wavetable ? proc_.loadWavetableFile(file)
                          : slot >= 0 ? proc_.loadTextureFile(slot, file) : proc_.loadTextureFile(file);
            if (!ok)
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, wavetable ? "Wavetable" : "Texture",
                    wavetable ? "Could not read this file as a wavetable (it needs at least one 2048-sample frame)." : "Could not read this audio file.");
            repaint();
        });
}

void AmbientSynthEditor::chooseImpulseFile(bool second)
{
    chooser_ = std::make_unique<juce::FileChooser>(second ? "Load the second impulse response (Room Morph fades to it)"
                                                          : "Load an impulse response (mono or stereo)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, second](const juce::FileChooser& fc) {
            const auto file = fc.getResult();
            if (!file.existsAsFile()) return;
            if (!proc_.loadImpulseFile(file, second))
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
        if (hoveredParam_ >= 0 && hoveredParam_ < kNumParams) {
            const ParamId id = static_cast<ParamId>(hoveredParam_);
            const ParamDesc& d = paramDesc(id);
            juce::String value;
            if (auto* v = proc_.apvts.getRawParameterValue(d.key)) {
                const float raw = v->load();
                if (d.kind == ParamKind::Choice) value = d.choices[juce::jlimit(0, d.numChoices - 1, static_cast<int>(std::lround(raw)))];
                else if (d.kind == ParamKind::Bool) value = raw >= 0.5f ? "on" : "off";
                else if (auto* pp = proc_.apvts.getParameter(d.key)) value = pp->getText(pp->convertTo0to1(raw), 24) + (d.unit[0] ? juce::String(" ") + d.unit : juce::String());
            }
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
        {   // The die: five pips in a small square at the right of the title. Click to redraw this
            // section, shift-click to nudge it.
            const juce::Rectangle<int> die(s.bounds.getRight() - kPad - 13, s.bounds.getY() + 4, 13, 13);
            diceOf_[s.name] = die;
            g.setColour(ui::card.brighter(0.10f));
            g.fillRoundedRectangle(die.toFloat(), 3.0f);
            g.setColour(col.withAlpha(0.5f));
            g.drawRoundedRectangle(die.toFloat().reduced(0.5f), 3.0f, 1.0f);
            g.setColour(col.withAlpha(0.75f));
            const float cx = die.toFloat().getCentreX(), cy = die.toFloat().getCentreY(), d = 3.2f;
            for (auto o : { juce::Point<float>(-d, -d), { d, -d }, { 0.0f, 0.0f }, { -d, d }, { d, d } })
                g.fillEllipse(cx + o.x - 1.0f, cy + o.y - 1.0f, 2.0f, 2.0f);
        }
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
