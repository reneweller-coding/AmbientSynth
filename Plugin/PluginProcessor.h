#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ambient/Engine.h"
#include "ambient/Presets.h"
#include "ambient/Gesture.h"
#include "ambient/Osc.h"
#include "ambient/Timeline.h"
#include <array>
#include <atomic>

class AmbientSynthProcessor : public juce::AudioProcessor, private ambient::OscSink, private juce::Timer
{
public:
    AmbientSynthProcessor();
    ~AmbientSynthProcessor() override;

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
    bool loadImpulseFile(const juce::File& file, bool second = false);   // convolution room, mono or stereo
    juce::String impulseName() const { return impulseFile_.existsAsFile() ? impulseFile_.getFileNameWithoutExtension() : juce::String(); }
    juce::String impulseBName() const { return impulseBFile_.existsAsFile() ? impulseBFile_.getFileNameWithoutExtension() : juce::String(); }
    // Level matching: while it is on, loading a preset trims the master gain by the difference
    // between the loudness that was measured for it and a common target, so auditioning a
    // hundred presets is not a ride on the volume knob. It never touches a preset's own settings.
    void setLevelMatch(bool on) { levelMatch_ = on; }
    bool levelMatch() const { return levelMatch_; }
    void setCompactLayout(bool on) { compact_ = on; }
    bool compactLayout() const { return compact_; }
    juce::String textureName() const  { return textureFile_.existsAsFile() ? textureFile_.getFileNameWithoutExtension() : juce::String(); }
    juce::String wavetableName() const { return wavetableFile_.existsAsFile() ? wavetableFile_.getFileNameWithoutExtension() : juce::String(); }
    // User presets as files (full state including a loaded Scala scale).
    bool savePresetFile(const juce::File& file);
    bool loadPresetFile(const juce::File& file);

    // Independent layers: the sound chain (everything but Cosmos) and the Cosmos chain.
    void applySoundPreset(int index);
    void applyCosmosPreset(int index);
    // Session recall (standalone). JUCE writes the whole state into its settings file when the
    // window is closed and reads it back on the next start -- but only then, so a crash, a kill
    // or a power cut loses the evening. The timer here writes it whenever it has actually
    // changed, and the switch turns the whole thing off and forgets what was stored.
    void setSessionRecall(bool on);
    bool sessionRecall() const { return sessionRecall_; }
    static bool sessionRecallAvailable();      // false in a plugin: there the host owns the state
    void saveSession();

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

    // Set timeline: record every parameter change and note with its time, play a set back.
    void startSetRecording();
    bool stopSetRecording(const juce::File& saveTo);   // false if the file could not be written
    bool isRecordingSet() const { return setRecording_.load(); }
    bool playSetFile(const juce::File& file);
    void stopSetPlayback() { setPlaying_.store(false); }
    bool isPlayingSet() const { return setPlaying_.load(); }
    double setTime() const { return setTime_.load(); }
    // Route over the map (text form, see ambient/Route.h), kept in the plugin state.
    bool setRouteText(const juce::String& text) { if (!engine_.setRouteText(text.toRawUTF8())) return false; routeText_ = text; return true; }
    juce::String routeText() const { return routeText_; }
    void clearRoute() { engine_.clearRoute(); routeText_.clear(); }
    bool addRoutePoint(const ambient::Waypoint& w) { if (!engine_.addRoutePoint(w)) return false; char buf[4096]; engine_.writeRoute(buf, sizeof(buf)); routeText_ = buf; return true; }
    // Favourite presets (browser stars), kept in the plugin state.
    // juce::BigInteger grows on demand, so the library's size is not a limit here.
    bool isFavourite(int preset) const { return preset >= 0 && favourites_[preset]; }
    void setFavourite(int preset, bool on) { if (preset >= 0) favourites_.setBit(preset, on); }

    juce::AudioProcessorValueTreeState apvts;
    ambient::Engine& engine() { return engine_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    ambient::Engine engine_;
    std::array<std::atomic<float>*, ambient::kNumParams> raw_{};
    juce::AudioBuffer<float> scratch_;
    juce::String scalaText_, userScaleName_;
    juce::File textureFile_, wavetableFile_, impulseFile_, impulseBFile_;
    bool       levelMatch_ = false, compact_ = false;
    void       applyLevelMatch(int presetIndex);
    juce::BigInteger favourites_;
    juce::String routeText_;
    // set timeline (recording appends on the audio thread; save/load on the message thread while stopped)
    ambient::SetTimeline setRec_, setPlay_;
    std::atomic<bool> setRecording_{ false }, setPlaying_{ false };
    std::atomic<double> setTime_{ 0.0 };
    float setLast_[ambient::kNumParams] = {};
    double setClock_ = 0.0;
    bool readMono(const juce::File& file, std::vector<float>& mono, double& sampleRate);
    int currentProgram_ = 0;
    int soundIndex_ = 0, cosmosIndex_ = 0;
    // The names, not the indices: a pack added or removed between two sessions renumbers every
    // preset behind it, and an index would then name a different sound.
    juce::String soundName_, cosmosName_;
    static juce::PropertySet* standaloneSettings();
    void timerCallback() override;             // session recall: save when the state has changed
    bool sessionRecall_ = true;
    juce::uint32 savedStateHash_ = 0;
    void applyScoped(const ambient::Preset& p, ambient::PresetScope scope);
    void loadPresetFiles(int index);   // a pack preset's own sample and wavetable
    std::array<std::atomic<int>, 128> ccMap_{};   // controller -> parameter index, -1 = none
    std::atomic<int> learnTarget_{ -1 };
    double clockSamples_ = 0.0, lastClockSample_ = -1.0;   // MIDI clock: running sample count, for the tick intervals
    // MPE: which note each channel is currently playing, so its bend, pressure and slide reach
    // the right voice. Channel 1 (index 0) is the master channel and holds no note.
    int   mpeNote_[16] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
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
