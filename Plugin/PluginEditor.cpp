#include "PluginEditor.h"
#include "ambient/Params.h"
#include "ambient/Presets.h"

using namespace ambient;

namespace {
constexpr int kCellW = 64, kCellH = 78, kPad = 10, kTitleH = 20, kHeaderH = 58;
const juce::Colour kBg(0xff14161a), kPanel(0xff1e2128), kAccent(0xff7fb3d5), kText(0xffd8dbe0), kDim(0xff7c8290);
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

    for (const ParamDesc& d : paramTable()) {
        if (d.id == ParamId::MasterGain) {
            master_ = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            master_->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
            master_->setTextValueSuffix(" dB");
            addAndMakeVisible(*master_);
            masterAttach_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, d.key, *master_);
            continue;
        }
        if (sections_.empty() || sections_.back().name != d.section) {
            Section s; s.name = d.section;
            if (s.name == "Tuning" || s.name == "Far Reverb") s.maxUnits = 8;
            sections_.push_back(std::move(s));
        }
        Cell c;
        c.label = std::make_unique<juce::Label>(juce::String(), d.name);
        c.label->setJustificationType(juce::Justification::centred);
        c.label->setFont(juce::FontOptions(12.0f));
        c.label->setColour(juce::Label::textColourId, kDim);
        addAndMakeVisible(*c.label);
        switch (d.kind) {
        case ParamKind::Float:
        case ParamKind::Int: {
            auto s = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            s->setTextBoxStyle(juce::Slider::TextBoxBelow, false, kCellW - 6, 16);
            if (d.unit[0] != 0) s->setTextValueSuffix(juce::String(" ") + d.unit);
            const float span = d.max - d.min;
            s->setNumDecimalPlacesToDisplay(d.kind == ParamKind::Int ? 0 : (span >= 100.0f ? 0 : (span >= 5.0f ? 1 : 2)));
            addAndMakeVisible(*s);
            c.slider = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, d.key, *s);
            c.comp = std::move(s);
            break;
        }
        case ParamKind::Bool: {
            auto b = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible(*b);
            c.button = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.apvts, d.key, *b);
            c.comp = std::move(b);
            break;
        }
        case ParamKind::Choice: {
            auto cb = std::make_unique<juce::ComboBox>();
            for (int i = 0; i < d.numChoices; ++i) cb->addItem(d.choices[i], i + 1);
            addAndMakeVisible(*cb);
            c.combo = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc_.apvts, d.key, *cb);
            c.comp = std::move(cb);
            c.units = 2;
            break;
        }
        }
        sections_.back().cells.push_back(static_cast<int>(cells_.size()));
        cells_.push_back(std::move(c));
    }

    // Extra cell: Scala loader in the Tuning section.
    scalaButton_ = std::make_unique<juce::TextButton>("Load Scala...");
    scalaButton_->onClick = [this] { chooseScalaFile(); };
    addAndMakeVisible(*scalaButton_);
    for (auto& s : sections_) if (s.name == "Tuning") {
        Cell c; c.units = 2;
        c.label = std::make_unique<juce::Label>(juce::String(), "User scale");
        c.label->setJustificationType(juce::Justification::centred);
        c.label->setFont(juce::FontOptions(12.0f));
        c.label->setColour(juce::Label::textColourId, kDim);
        addAndMakeVisible(*c.label);
        s.cells.push_back(static_cast<int>(cells_.size()));
        cells_.push_back(std::move(c));
    }

    presetBox_ = std::make_unique<juce::ComboBox>();
    for (int i = 0; i < numPresets(); ++i) presetBox_->addItem(preset(i).name, i + 1);
    presetBox_->setSelectedId(proc_.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox_->onChange = [this] {
        const int idx = presetBox_->getSelectedId() - 1;
        if (idx >= 0 && idx != proc_.getCurrentProgram()) proc_.setCurrentProgram(idx);
    };
    addAndMakeVisible(*presetBox_);

    setResizable(true, true);
    setSize(1100, 840);
    startTimerHz(12);
}

AmbientSynthEditor::~AmbientSynthEditor()
{
    setLookAndFeel(nullptr);
}

void AmbientSynthEditor::timerCallback()
{
    proc_.engine().soundingNotes(sounding_);
    if (presetBox_ && presetBox_->getSelectedId() != proc_.getCurrentProgram() + 1)
        presetBox_->setSelectedId(proc_.getCurrentProgram() + 1, juce::dontSendNotification);
    repaint(header_);
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

void AmbientSynthEditor::resized()
{
    header_ = getLocalBounds().removeFromTop(kHeaderH);
    if (master_) master_->setBounds(getWidth() - 84, 4, 76, kHeaderH - 6);
    if (presetBox_) presetBox_->setBounds(200, 8, 190, 24);

    int x = kPad, y = kHeaderH + kPad, rowH = 0;
    for (auto& s : sections_) {
        // Flow the cells (unit widths) into rows of at most maxUnits.
        int unitsInRow = 0, rows = 1;
        for (int ci : s.cells) {
            const int u = cells_[static_cast<size_t>(ci)].units;
            if (unitsInRow + u > s.maxUnits) { ++rows; unitsInRow = 0; }
            unitsInRow += u;
        }
        int widestRow = 0; unitsInRow = 0;
        for (int ci : s.cells) {
            const int u = cells_[static_cast<size_t>(ci)].units;
            if (unitsInRow + u > s.maxUnits) unitsInRow = 0;
            unitsInRow += u; widestRow = std::max(widestRow, unitsInRow);
        }
        const int w = widestRow * kCellW + 2 * kPad;
        const int h = rows * kCellH + kTitleH + kPad;
        if (x + w > getWidth() - kPad && x > kPad) { x = kPad; y += rowH + kPad; rowH = 0; }
        s.bounds = { x, y, w, h };
        rowH = std::max(rowH, h);

        int cx = x + kPad, cy = y + kTitleH; unitsInRow = 0;
        for (int ci : s.cells) {
            Cell& c = cells_[static_cast<size_t>(ci)];
            if (unitsInRow + c.units > s.maxUnits) { unitsInRow = 0; cx = x + kPad; cy += kCellH; }
            juce::Rectangle<int> cell(cx, cy, c.units * kCellW, kCellH);
            auto labelArea = cell.removeFromBottom(16);
            c.label->setBounds(labelArea);
            if (c.comp) {
                if (dynamic_cast<juce::ComboBox*>(c.comp.get()) != nullptr) c.comp->setBounds(cell.withSizeKeepingCentre(cell.getWidth() - 8, 24));
                else if (dynamic_cast<juce::ToggleButton*>(c.comp.get()) != nullptr) c.comp->setBounds(cell.withSizeKeepingCentre(24, 24));
                else c.comp->setBounds(cell.reduced(2, 0));
            } else if (scalaButton_) {
                scalaButton_->setBounds(cell.withSizeKeepingCentre(cell.getWidth() - 8, 24));
            }
            cx += c.units * kCellW; unitsInRow += c.units;
        }
        x += w + kPad;
    }
}

void AmbientSynthEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBg);

    // Header: name, activity, keyboard strip.
    g.setColour(kPanel);
    g.fillRect(header_);
    g.setColour(kText);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("AmbientSynth", 14, 6, 180, 26, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(12.0f));
    g.setColour(kDim);
    const int voices = proc_.engine().activeVoices();
    const int root = proc_.engine().brainRoot();
    const float arc = proc_.engine().arcValue();
    juce::String info = juce::String(voices) + " voice" + (voices == 1 ? "" : "s")
        + "   root " + juce::MidiMessage::getMidiNoteName(root, true, true, 4)
        + "   scale " + juce::String(proc_.engine().scale().name)
        + "   arc " + juce::String(arc, 2);
    if (proc_.userScaleName().isNotEmpty()) info += "   (user: " + proc_.userScaleName() + ")";
    g.drawText(info, 14, 34, 560, 18, juce::Justification::centredLeft);

    // Keyboard strip: notes 24..108; near notes bright, far notes dim (the front-to-back planes).
    const int first = 24, last = 108;
    const int stripX = 590, stripW = getWidth() - 590 - 100;
    const float keyW = static_cast<float>(stripW) / static_cast<float>(last - first + 1);
    for (int n = first; n <= last; ++n) {
        const float kx = stripX + (n - first) * keyW;
        const bool black = juce::MidiMessage::isMidiNoteBlack(n);
        juce::Colour col = black ? juce::Colour(0xff2a2e36) : juce::Colour(0xff3a3f4a);
        if (sounding_[n]) {
            const float d = juce::jlimit(0.0f, 1.0f, proc_.engine().noteDistance(n));
            col = kAccent.interpolatedWith(juce::Colour(0xff35506a), d);
        }
        if (n == root) col = col.interpolatedWith(juce::Colours::orange, 0.6f);
        g.setColour(col);
        g.fillRect(kx + 0.5f, 14.0f, keyW - 1.0f, 30.0f);
    }

    for (const auto& s : sections_) {
        g.setColour(kPanel);
        g.fillRoundedRectangle(s.bounds.toFloat(), 6.0f);
        g.setColour(kAccent);
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText(s.name.toUpperCase(), s.bounds.getX() + kPad, s.bounds.getY() + 2, s.bounds.getWidth() - 2 * kPad, kTitleH, juce::Justification::centredLeft);
    }
}
