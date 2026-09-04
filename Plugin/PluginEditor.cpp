#include "PluginEditor.h"
#include "ambient/Params.h"
#include "ambient/Presets.h"

using namespace ambient;

namespace {
constexpr int kCellW = 60, kCellH = 70, kPad = 8, kTitleH = 18, kGroupTitleH = 22, kHeaderH = 114;
const juce::Colour kBg(0xff121418), kGroupFill(0xff1a1d23), kSectionFill(0xff21252c), kAccent(0xff7fb3d5),
                   kText(0xffd8dbe0), kDim(0xff7c8290);
const juce::Colour kVoice(0xff7fb3d5), kFore(0xff8fd18f), kBack(0xffb59ce6), kCosmos(0xfff0a35e),
                   kConductor(0xff6fd3c8), kMorph(0xffe58fb8), kMaster(0xffd8dbe0);
}

AmbientSynthEditor::AmbientSynthEditor(AmbientSynthProcessor& p)
    : AudioProcessorEditor(p), proc_(p)
{
    laf_.setColourScheme(juce::LookAndFeel_V4::getDarkColourScheme());
    laf_.setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    laf_.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff2c3038));
    laf_.setColour(juce::Slider::thumbColourId, juce::Colours::white);
    laf_.setColour(juce::Slider::textBoxTextColourId, kText);
    laf_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    laf_.setColour(juce::Label::textColourId, kText);
    laf_.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff2a2e36));
    laf_.setColour(juce::ToggleButton::textColourId, kText);
    laf_.setColour(juce::ToggleButton::tickColourId, kAccent);
    laf_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2e36));
    setLookAndFeel(&laf_);

    groups_ = {
        { "VOICE",      kVoice,     { { "Oscillator", "Air", "Envelope" }, { "Source 2" }, { "Source 3" }, { "Filter", "Space" }, { "Foundation" } }, {}, 0 },
        { "FOREGROUND", kFore,      { { "Ensemble", "Delay" }, { "Delay 2", "Near Reverb" } }, {}, 0 },
        { "BACKGROUND", kBack,      { { "Cloud", "Far Reverb" }, { "Feedback" } }, {}, 0 },
        { "CONDUCTOR",  kConductor, { { "Cluster Brain" }, { "Tuning" } }, {}, 1 },
        { "COSMOS",     kCosmos,    { { "Cosmos" } }, {}, 1 },
        { "MORPH",      kMorph,     { { "Morph" }, { "Macros" } }, {}, 1 },
    };

    content_.onPaint = [this](juce::Graphics& g) { paintContent(g); };
    viewport_.setViewedComponent(&content_, false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);
    buildCells();
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

    // Header controls
    soundBox_ = std::make_unique<juce::ComboBox>();
    soundBox_->setTextWhenNothingSelected("Sound preset");
    for (int i = 0; i < numPresets(); ++i) soundBox_->addItem(preset(i).name, i + 1);
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

    setResizable(true, true);
    setSize(1500, 920);
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
            if (s.name == "Morph" || s.name == "Foundation") s.maxUnits = 9;
            if (s.name == "Source 2" || s.name == "Source 3") s.maxUnits = 10;
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
            auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, kCellW - 6, 15);
            if (d.unit[0] != 0) s->setTextValueSuffix(juce::String(" ") + d.unit);
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

    auto boxA = std::make_unique<juce::ComboBox>();
    boxA->setTextWhenNothingSelected("A: preset");
    for (int i = 0; i < numPresets(); ++i) boxA->addItem(preset(i).name, i + 1);
    morphABox_ = boxA.get();
    boxA->onChange = [this] { if (morphABox_->getSelectedId() > 0) proc_.setMorphSlotFromPreset(0, morphABox_->getSelectedId() - 1); };
    addExtraCell("Morph", std::move(boxA), "A", 2);
    auto boxB = std::make_unique<juce::ComboBox>();
    boxB->setTextWhenNothingSelected("B: preset");
    for (int i = 0; i < numPresets(); ++i) boxB->addItem(preset(i).name, i + 1);
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
    header_ = getLocalBounds().removeFromTop(kHeaderH);
    if (soundBox_) soundBox_->setBounds(200, 8, 180, 24);
    if (cosmosBox_) cosmosBox_->setBounds(388, 8, 160, 24);
    if (saveButton_) saveButton_->setBounds(556, 8, 64, 24);
    if (loadButton_) loadButton_->setBounds(626, 8, 64, 24);
    if (recButton_) recButton_->setBounds(696, 8, 56, 24);
    if (calibButton_) calibButton_->setBounds(880, 8, 76, 24);
    if (mapButton_) mapButton_->setBounds(962, 8, 84, 24);
    routing_ = { 12, 40, 900, kHeaderH - 46 };
    keys_ = { 930, 42, getWidth() - 930 - 340, 26 };
    if (master_) master_->setBounds(getWidth() - 74, 6, 66, 52);

    // The Master section (mid/side) sits in the header, left of the master knob.
    if (Section* ms = findSection("Master")) {
        ms->maxUnits = 8;
        layoutSection(*ms, getWidth() - 74 - sectionWidth(*ms) - 6, 4);
        ms->bounds = ms->bounds.withTrimmedTop(-2);
    }

    // Everything else scrolls below the header.
    viewport_.setBounds(0, kHeaderH, getWidth(), getHeight() - kHeaderH);

    // Column widths from the widest group row in each column.
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
    const int contentW = std::max(colX[1] + colWidth[1] + kPad, viewport_.getMaximumVisibleWidth());
    const int contentH = std::max(std::max(colY[0], colY[1]), viewport_.getMaximumVisibleHeight());
    content_.setSize(contentW, contentH);
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
    // The 13 slot parameters are laid out identically for Source 2 and Source 3 (see Params.h):
    // 0 type 1 level 2 octave 3 ratio 4 pan 5 table 6 position 7 pos drift 8 fm ratio 9 fm index
    // 10 grain 11 density 12 follow. Grey what the chosen type ignores.
    const ParamId first[2] = { ParamId::Src2Type, ParamId::Src3Type };
    for (int k = 0; k < 2; ++k) {
        const int type = static_cast<int>(std::lround(proc_.engine().getParam(first[k])));   // 0 off 1 wavetable 2 fm 3 texture
        for (int off = 1; off <= 12; ++off) {
            bool on = type != 0;
            if (off == 5) on = type == 1;
            else if (off == 6 || off == 7) on = type == 1 || type == 3 || (off == 7 && type == 2);
            else if (off == 8 || off == 9) on = type == 2;
            else if (off >= 10) on = type == 3;
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
    auto* hint = new juce::Label(juce::String(), "input param min max [smooth] [deadzone] [clutch|none] [invert]   inputs: HandDistance LeftHeight RightHeight LeftForward RightForward LeftTilt RightTilt LeftPinch RightPinch HeadYaw HeadPitch HeadRoll Custom0..7 (Custom0..3 = macros)");
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

void AmbientSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);

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
