#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ambient/Engine.h"
#include "ambient/Presets.h"
#include "ambient/Gesture.h"
#include "ambient/Osc.h"
#include <array>
#include <atomic>

class AmbientSynthProcessor : public juce::AudioProcessor, private ambient::OscSink
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
    // Source-slot data: a texture sample (any format JUCE reads; assumed recorded at C4 for
    // Pitch = Note) and a user wavetable (2048-sample frames). Paths are kept in the state.
    bool loadTextureFile(const juce::File& file);
    bool loadWavetableFile(const juce::File& file);
    bool loadImpulseFile(const juce::File& file);   // convolution room, mono or stereo
    juce::String impulseName() const { return impulseFile_.existsAsFile() ? impulseFile_.getFileNameWithoutExtension() : juce::String(); }
    juce::String textureName() const  { return textureFile_.existsAsFile() ? textureFile_.getFileNameWithoutExtension() : juce::String(); }
    juce::String wavetableName() const { return wavetableFile_.existsAsFile() ? wavetableFile_.getFileNameWithoutExtension() : juce::String(); }
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

    // OSC input and gesture layer (see ambient/Osc.h for the namespace).
    ambient::GestureLayer& gestures() { return gestures_; }
    bool oscRunning() const { return osc_.running(); }
    int  oscPort() const { return osc_.port(); }
    juce::String oscError() const { return osc_.lastError(); }
    juce::uint64 oscMessages() const { return osc_.messagesReceived(); }
    bool setGestureMappings(const juce::String& text);
    juce::String gestureMappings() const;

    // Recording the output to a 32-bit float WAV (message thread to start/stop).
    bool startRecording(const juce::File& file);
    void stopRecording();
    bool isRecording() const { return recording_.load(); }
    double recordedSeconds() const { return recordedSamples_.load() / juce::jmax(1.0, getSampleRate()); }

    // MIDI learn: arm a parameter, the next controller message binds to it.
    void armMidiLearn(ambient::ParamId id) { learnTarget_.store(static_cast<int>(id)); }
    void clearMidiLearn(ambient::ParamId id);
    int  midiCcFor(ambient::ParamId id) const;     // -1 if unmapped
    int  learnTarget() const { return learnTarget_.load(); }
    juce::String userScaleName() const { return userScaleName_; }

    // Favourite presets (browser stars), kept in the plugin state.
    bool isFavourite(int preset) const { return preset >= 0 && preset < 1024 && favourites_[preset]; }
    void setFavourite(int preset, bool on) { if (preset >= 0 && preset < 1024) favourites_.setBit(preset, on); }

    juce::AudioProcessorValueTreeState apvts;
    ambient::Engine& engine() { return engine_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ambient::Engine engine_;
    std::array<std::atomic<float>*, ambient::kNumParams> raw_{};
    juce::AudioBuffer<float> scratch_;
    juce::String scalaText_, userScaleName_;
    juce::File textureFile_, wavetableFile_, impulseFile_;
    juce::BigInteger favourites_;
    bool readMono(const juce::File& file, std::vector<float>& mono, double& sampleRate);
    int currentProgram_ = 0;
    int soundIndex_ = 0, cosmosIndex_ = 0;
    void applyScoped(const ambient::Preset& p, ambient::PresetScope scope);
    std::array<std::atomic<int>, 128> ccMap_{};   // controller -> parameter index, -1 = none
    std::atomic<int> learnTarget_{ -1 };
    juce::String slotName_[2] = { "Init", "Init" };

    // OscSink
    void setParam(ambient::ParamId id, float value) override;
    void setParamNormalised(ambient::ParamId id, float norm) override;
    void event(const ambient::ControlEvent& e) override;
    ambient::GestureLayer gestures_;
    ambient::OscServer    osc_;
    ambient::EventQueue   events_;
    juce::String          mappingText_;

    // Recording
    juce::TimeSliceThread recordThread_{ "AmbientSynth recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> recordWriter_;
    juce::CriticalSection recordLock_;
    std::atomic<bool> recording_{ false };
    std::atomic<juce::int64> recordedSamples_{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AmbientSynthProcessor)
};
