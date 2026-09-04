#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include <vector>
#include <memory>

class AmbientSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AmbientSynthEditor(AmbientSynthProcessor&);
    ~AmbientSynthEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseScalaFile();

    struct Cell {
        std::unique_ptr<juce::Component> comp;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> combo;
        int units = 1;
    };
    struct Section {
        juce::String name;
        std::vector<int> cells;
        juce::Rectangle<int> bounds;
        int maxUnits = 7;
    };

    AmbientSynthProcessor& proc_;
    juce::LookAndFeel_V4 laf_;
    std::vector<Cell> cells_;
    std::vector<Section> sections_;
    std::unique_ptr<juce::Slider> master_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterAttach_;
    std::unique_ptr<juce::TextButton> scalaButton_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> header_;
    bool sounding_[128] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientSynthEditor)
};
