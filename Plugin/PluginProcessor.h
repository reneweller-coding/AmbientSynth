#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ambient/Engine.h"
#include "ambient/Presets.h"
#include <array>
#include <atomic>

class AmbientSynthProcessor : public juce::AudioProcessor
{
public:
    AmbientSynthProcessor();
    ~AmbientSynthProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 120.0; }

    // Programs == built-in presets from the core.
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Message thread. Returns false if the file is not a valid Scala scale.
    bool loadScalaText(const juce::String& text, const juce::String& displayName);
    // User presets as files (full state including a loaded Scala scale).
    bool savePresetFile(const juce::File& file);
    bool loadPresetFile(const juce::File& file);

    // Independent layers: the sound chain (everything but Cosmos) and the Cosmos chain.
    void applySoundPreset(int index);
    void applyCosmosPreset(int index);
    int  soundPresetIndex() const  { return soundIndex_; }
    int  cosmosPresetIndex() const { return cosmosIndex_; }

    // Morph slots A (0) and B (1).
    void setMorphSlotFromPreset(int slot, int presetIndex);
    void setMorphSlotFromCurrent(int slot);
    juce::String morphSlotName(int slot) const { return slotName_[slot & 1]; }

    // MIDI learn: arm a parameter, the next controller message binds to it.
    void armMidiLearn(ambient::ParamId id) { learnTarget_.store(static_cast<int>(id)); }
    void clearMidiLearn(ambient::ParamId id);
    int  midiCcFor(ambient::ParamId id) const;     // -1 if unmapped
    int  learnTarget() const { return learnTarget_.load(); }
    juce::String userScaleName() const { return userScaleName_; }

    juce::AudioProcessorValueTreeState apvts;
    ambient::Engine& engine() { return engine_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ambient::Engine engine_;
    std::array<std::atomic<float>*, ambient::kNumParams> raw_{};
    juce::AudioBuffer<float> scratch_;
    juce::String scalaText_, userScaleName_;
    int currentProgram_ = 0;
    int soundIndex_ = 0, cosmosIndex_ = 0;
    void applyScoped(const ambient::Preset& p, ambient::PresetScope scope);
    std::array<std::atomic<int>, 128> ccMap_{};   // controller -> parameter index, -1 = none
    std::atomic<int> learnTarget_{ -1 };
    juce::String slotName_[2] = { "Init", "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientSynthProcessor)
};
