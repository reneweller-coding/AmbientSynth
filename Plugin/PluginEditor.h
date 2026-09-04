#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <vector>
#include <memory>
#include <map>

// The editor groups the sections the way the signal flows:
//   VOICE (Oscillator, Air, Envelope, Filter, Space) -> FOREGROUND (Ensemble, Delay, Delay 2,
//   Near Reverb) -> out; FOREGROUND -> COSMOS (parallel, returns) ; FOREGROUND -> BACKGROUND
//   (Cloud, Far Reverb) -> out ; CONDUCTOR (Cluster Brain, Tuning) drives the voices;
//   MORPH blends two full presets. A routing map in the header draws exactly this.
class AmbientSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AmbientSynthEditor(AmbientSynthProcessor&);
    ~AmbientSynthEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void chooseScalaFile();
    void buildCells();
    int  addExtraCell(const juce::String& section, std::unique_ptr<juce::Component> comp, const juce::String& label, int units);
    void paintRoutingMap(juce::Graphics&, juce::Rectangle<int> area);

    struct Cell {
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
        juce::String baseLabel;
        int units = 1;
        int param = -1;    // ParamId or -1 for extra cells
    };
    struct Section {
        juce::String name;
        std::vector<int> cells;
        juce::Rectangle<int> bounds;
        int maxUnits = 7;
        int group = -1;
    };
    struct Group {
        juce::String name;
        juce::Colour colour;
        std::vector<std::vector<juce::String>> rows;   // section names per row
        juce::Rectangle<int> bounds;
        int column = 0;
    };

    Section* findSection(const juce::String& name);
    int      sectionWidth(const Section&) const;
    int      sectionHeight(const Section&) const;
    void     layoutSection(Section&, int x, int y);

    AmbientSynthProcessor& proc_;
    juce::LookAndFeel_V4 laf_;
    std::vector<Cell> cells_;
    std::vector<Section> sections_;
    std::vector<Group> groups_;
    std::map<juce::Component*, int> cellOf_;
    std::unique_ptr<juce::Slider> master_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttach_;
    // Everything below the header lives in a scrollable content component.
    struct Content : juce::Component {
        std::function<void(juce::Graphics&)> onPaint;
        void paint(juce::Graphics& g) override { if (onPaint) onPaint(g); }
    };
    Content content_;
    juce::Viewport viewport_;
    void paintContent(juce::Graphics&);
    std::unique_ptr<juce::TextButton> saveButton_, loadButton_, recButton_;
    std::unique_ptr<juce::ComboBox> soundBox_, cosmosBox_;
    juce::ComboBox* morphABox_ = nullptr;   // owned by their cells
    juce::ComboBox* morphBBox_ = nullptr;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> header_, routing_, keys_;
    bool sounding_[128] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientSynthEditor)
};
