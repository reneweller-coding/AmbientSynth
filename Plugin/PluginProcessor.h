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
    // One clip per source slot; the slotless form loads the file into all four, which is what a
    // preset that names a single file means and what the instrument always did.
    bool loadTextureFile(int slot, const juce::File& file);
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
    // 0 normal, 1 compact (wide rows wrap), 2 expanded (every page of every tab row laid out
    // under one another -- the tabs gone, the page tall). Kept in the state like Compact.
    void setLayoutMode(int m) { layoutMode_ = m; compact_ = m == 1; }
    int  layoutMode() const { return layoutMode_; }
    juce::String textureName(int slot = 0) const
    {
        const int k = juce::jlimit(0, ambient::kSlots - 1, slot);
        return textureFile_[k].existsAsFile() ? textureFile_[k].getFileNameWithoutExtension() : juce::String();
    }
    juce::String wavetableName() const { return wavetableFile_.existsAsFile() ? wavetableFile_.getFileNameWithoutExtension() : juce::String(); }
    // User presets as files (full state including a loaded Scala scale).
    bool savePresetFile(const juce::File& file);
    bool loadPresetFile(const juce::File& file);

    // Independent layers: the sound chain (everything but Cosmos) and the Cosmos chain.
    void applySoundPreset(int index);
    void applyCosmosPreset(int index);
    // The two newer layers. Like Cosmos: each resets its own section and touches nothing else.
    void applyZPreset(int index);
    void applyStrikePreset(int index);
    int  zPresetIndex() const { return zIndex_; }
    int  strikePresetIndex() const { return strikeIndex_; }
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
    // Choosing a preset while another one is playing: instead of the cut, the instrument travels.
    // The state now becomes slot A, the chosen preset slot B, and the morph position glides from
    // 0 to 1 over `seconds`; when it arrives, the preset's own values are written into the
    // parameters and the morph switches itself off, so what is left is the preset and not a
    // blend. Off, or with the browser's switch off, a preset still lands the moment it is chosen.
    void selectPreset(int index, bool viaMorph);
    bool morphOnSelect() const { return morphOnSelect_; }
    void setMorphOnSelect(bool on) { morphOnSelect_ = on; }
    float morphSelectSeconds() const { return morphSelectSeconds_; }
    void setMorphSelectSeconds(float s) { morphSelectSeconds_ = juce::jlimit(0.5f, 600.0f, s); }
    // Which preset the instrument is travelling towards, -1 when it is not, and how far it has
    // come (0..1) -- the browser draws both.
    // What the map draws as the travelling line: where the sound is coming from, where it is
    // going, and how far it has come. A change counts as travelling from the moment it is asked
    // for -- the incoming engine may be published a block before the audio thread takes it up,
    // and a line that appeared one frame late would look like a dropped click.
    bool  transitionInFlight() const
    { return fading_.load(std::memory_order_acquire) >= 0 || swapTo_.load(std::memory_order_acquire) >= 0; }
    int   morphingTo() const { return transitionInFlight() ? soundIndex_ : -1; }
    int   morphingFrom() const { return transitionInFlight() ? fadingFrom_ : -1; }
    float morphProgress() const { return transitionInFlight() ? fadePos_.load() : 1.0f; }
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
    bool setRouteText(const juce::String& text) { if (!live().setRouteText(text.toRawUTF8())) return false; routeText_ = text; return true; }
    juce::String routeText() const { return routeText_; }
    void clearRoute() { live().clearRoute(); routeText_.clear(); }
    bool addRoutePoint(const ambient::Waypoint& w) { if (!live().addRoutePoint(w)) return false; char buf[4096]; live().writeRoute(buf, sizeof(buf)); routeText_ = buf; return true; }
    // Favourite presets (browser stars), kept in the plugin state.
    // juce::BigInteger grows on demand, so the library's size is not a limit here.
    bool isFavourite(int preset) const { return preset >= 0 && favourites_[preset]; }
    void setFavourite(int preset, bool on) { if (preset >= 0) favourites_.setBit(preset, on); }

    // Bumped whenever any parameter changes, from wherever. A display that draws nothing but
    // parameters -- the filter response, an envelope, the vector square -- has no business
    // repainting fifteen times a second while the panel stands still, and asks this instead.
    uint32_t paramGeneration() const { return paramGen_.load(std::memory_order_relaxed); }

    juce::AudioProcessorValueTreeState apvts;
    // The instrument that is sounding. There are two of them (see selectPreset): a preset change
    // that is meant to be heard as a transition lets the old one keep playing while the new one
    // fades in, and the two swap roles when it has arrived. Everything that means "the synth"
    // -- parameters, notes, the editor's displays -- means the live one.
    ambient::Engine& engine() { return live(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // One listener on every parameter; counting is all it does, so it costs nothing on the audio
    // thread when a host automates something.
    struct ParamWatch : juce::AudioProcessorParameter::Listener {
        explicit ParamWatch(std::atomic<uint32_t>& g) : gen(g) {}
        void parameterValueChanged(int, float) override { gen.fetch_add(1, std::memory_order_relaxed); }
        void parameterGestureChanged(int, bool) override {}
        std::atomic<uint32_t>& gen;
    };
    std::atomic<uint32_t> paramGen_ { 0 };
    ParamWatch paramWatch_ { paramGen_ };

    ambient::Engine engines_[2];
    // Which of the two is the instrument right now. Read by both threads and written only by the
    // audio thread, at a block boundary (see the swap in processBlock): the message thread
    // prepares the other engine in full and then asks for the change, so no engine is ever
    // written by one thread while the other renders it.
    std::atomic<int> live_ { 0 };
    ambient::Engine& live()  { return engines_[live_.load(std::memory_order_relaxed)]; }
    ambient::Engine& other() { return engines_[live_.load(std::memory_order_relaxed) ^ 1]; }
    // Where the message thread's engine writes go while a transition is being prepared: the
    // incoming engine, which nothing renders yet. Null the rest of the time, when they mean the
    // instrument itself. Message thread only.
    ambient::Engine* prepareTarget_ = nullptr;
    ambient::Engine& target() { return prepareTarget_ != nullptr ? *prepareTarget_ : live(); }
    // The engine the audio thread pushes the parameter tree into, or -1 for "whichever is live".
    // While a transition is being prepared it is the incoming one, so that the preset now being
    // written into the tree does not also land on the engine that is still playing the old sound.
    std::atomic<int>  paramTarget_ { -1 };
    // Published when the incoming engine is ready; the audio thread makes it live and starts the
    // crossfade at the next block. -1 = nothing waiting.
    std::atomic<int>  swapTo_ { -1 };
    // Asks the audio thread to let go of the engine that is fading out, so the message thread may
    // prepare it for the next change. A transition interrupted this way ends where it stands.
    std::atomic<bool> endFade_ { false };
    int  pendingPreset_ = -1;         // a change waiting for the audio thread to free an engine
    void beginTransition(int index);  // message thread: prepare the incoming engine and publish it
    // A change asked for while both engines were busy is served from here, a few milliseconds
    // later. A timer rather than a wait: the message thread must not block on the audio thread,
    // and with no audio device running it would never be let go.
    // It also carries out the preset changes OSC asks for: those write the whole parameter tree
    // and read files, which is not work for the audio thread.
    struct PresetPump : juce::Timer {
        explicit PresetPump(AmbientSynthProcessor& p) : proc(p) {}
        void timerCallback() override { proc.servePresetRequests(); proc.servePendingPreset(); }
        AmbientSynthProcessor& proc;
    };
    PresetPump presetPump_ { *this };
    ambient::EventQueue presetEvents_;   // OSC preset changes, waiting for the message thread
public:
    void servePendingPreset();        // message thread; does nothing until an engine is free
    void servePresetRequests();       // message thread; the preset changes OSC asked for
private:
    // A transition in flight: the engine on its way out, and how far the crossfade has come.
    // -1 when nothing is fading. Equal-power, so the sum never dips in the middle. Written on the
    // audio thread and read by the map, which draws the crossing, so both are atomic.
    std::atomic<int>   fading_  { -1 };
    std::atomic<float> fadePos_ { 0.0f };
    // Which preset the leaving engine is playing, so the map can draw the line from there. The
    // program number is the arriving one from the moment the change is made -- the name, the
    // parameters and the ring all move at once -- so the departure has to be remembered here.
    int   fadingFrom_ = -1;
    // Before the ramp starts, the incoming engine is given time to speak: a brain preset's first
    // note comes when the brain decides to play it, a sample preset's clip may still be loading,
    // a drone's attack may be ten seconds long. Until the incoming engine is audible (or
    // kFadeHeadStart seconds have passed) the ramp stands at zero and the outgoing one plays on
    // at full level -- otherwise the old sound fades into silence and the new one arrives into it.
    float fadeHead_ = 0.0f;
    static constexpr float kFadeHeadStart = 8.0f;
    // The notes held right now, from MIDI, OSC and the set timeline alike. They are handed to the
    // incoming engine of a transition: a chord held through a preset change stays held.
    std::array<float, 128> heldVel_{};
    void noteOn(int note, float vel);
    void noteOff(int note);
    void allNotesOff();
    double sampleRate_ = 48000.0;   // from prepareToPlay; the fade cannot trust getSampleRate() before the host sets it
    juce::AudioBuffer<float> fadeBuf_;
    // Output muted at the device: set by AMBIENT_MUTE=1, and implied by AMBIENT_MANUAL (the
    // export plays a chord for its pictures; nobody asked to hear it). Read once, at start.
    const bool muteOutput_ = juce::SystemStats::getEnvironmentVariable("AMBIENT_MUTE", "").isNotEmpty()
                          || juce::SystemStats::getEnvironmentVariable("AMBIENT_MANUAL", "").isNotEmpty()
                          || juce::SystemStats::getEnvironmentVariable("AMBIENT_SHOT", "").isNotEmpty();
    std::array<std::atomic<float>*, ambient::kNumParams> raw_{};
    juce::AudioBuffer<float> scratch_;
    juce::String scalaText_, userScaleName_;
    juce::File textureFile_[ambient::kSlots], wavetableFile_, impulseFile_, impulseBFile_;
    bool       levelMatch_ = false, compact_ = false;
    // The travelling preset change (selectPreset): the target, and what the browser last set.
    int        morphTarget_ = -1;
    bool       morphOnSelect_ = true;
    float      morphSelectSeconds_ = 20.0f;
    int        layoutMode_ = 0;
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
    int soundIndex_ = 0, cosmosIndex_ = 0, zIndex_ = 0, strikeIndex_ = 0;
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
    void setHeadYaw(float degrees) override { live().setHeadYaw(degrees); }
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
