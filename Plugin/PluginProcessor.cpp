#include "PluginProcessor.h"
// For the standalone's settings file: the same one JUCE saves the state into when the window is
// closed. Reaching it here is what lets the session be written while the app is still running.
#if JucePlugin_Build_Standalone
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif
#include "ambient/PresetMeta.h"
#include "PluginEditor.h"
#include "ambient/Tuning.h"
#include "ambient/Presets.h"

using namespace ambient;

juce::AudioProcessorValueTreeState::ParameterLayout AmbientSynthProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const ParamDesc& d : paramTable()) {
        const juce::ParameterID id(d.key, 1);
        switch (d.kind) {
        case ParamKind::Float: {
            juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
            const float span = d.max - d.min;
            const int decimals = span >= 100.0f ? 0 : (span >= 5.0f ? 1 : 2);
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                id, d.name, range, d.def,
                juce::AudioParameterFloatAttributes()
                    .withLabel(d.unit)
                    // A rate of 0.004 Hz printed with one decimal is "0.0": below one, show enough
                    // digits that the number means something.
                    .withStringFromValueFunction([decimals](float v, int) {
                        const int dp = std::abs(v) < 1.0f ? juce::jmax(decimals, std::abs(v) < 0.1f ? 3 : 2) : decimals;
                        return juce::String(v, dp);
                    })));
            break;
        }
        case ParamKind::Int:
            layout.add(std::make_unique<juce::AudioParameterInt>(
                id, d.name, static_cast<int>(d.min), static_cast<int>(d.max), static_cast<int>(d.def),
                juce::AudioParameterIntAttributes().withLabel(d.unit)));
            break;
        case ParamKind::Bool:
            layout.add(std::make_unique<juce::AudioParameterBool>(id, d.name, d.def >= 0.5f));
            break;
        case ParamKind::Choice: {
            juce::StringArray names;
            for (int i = 0; i < d.numChoices; ++i) names.add(d.choices[i]);
            layout.add(std::make_unique<juce::AudioParameterChoice>(id, d.name, names, static_cast<int>(d.def)));
            break;
        }
        }
    }
    return layout;
}

AmbientSynthProcessor::AmbientSynthProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "AmbientSynth", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        raw_[static_cast<size_t>(i)] = apvts.getRawParameterValue(paramTable()[static_cast<size_t>(i)].key);
    for (auto& c : ccMap_) c.store(-1);
    // Preset packs (thousands of presets as text files) before anything reads the preset list.
    loadDefaultPresetPacks();
    // OSC on 9000; a second instance in a DAW simply reports the port as taken.
    osc_.start(9000, *this, gestures_);
    // Session recall. The switch lives in the same settings file as the state, so turning it off
    // survives a restart; off also drops the stored state, before JUCE can read it back -- this
    // constructor runs from the holder's, ahead of its reloadPluginState().
    if (auto* settings = standaloneSettings()) {
        sessionRecall_ = settings->getBoolValue("sessionRecall", true);
        if (!sessionRecall_) settings->removeValue("filterState");
        else startTimer(15000);
    }
}

AmbientSynthProcessor::~AmbientSynthProcessor()
{
    stopTimer();
    saveSession();
}

// The standalone's settings, or nothing at all when this is a plugin: there the host owns the
// state, saves it with the project and hands it back, and a synth that quietly loaded somebody
// else's last session into a fresh instance would be a bug, not a feature.
juce::PropertySet* AmbientSynthProcessor::standaloneSettings()
{
   #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance()) return holder->settings.get();
   #endif
    return nullptr;
}

bool AmbientSynthProcessor::sessionRecallAvailable() { return standaloneSettings() != nullptr; }

void AmbientSynthProcessor::setSessionRecall(bool on)
{
    sessionRecall_ = on;
    auto* settings = standaloneSettings();
    if (settings == nullptr) return;
    settings->setValue("sessionRecall", on);
    if (on) { savedStateHash_ = 0; startTimer(15000); saveSession(); }
    else    { stopTimer(); settings->removeValue("filterState"); }
    if (auto* file = dynamic_cast<juce::PropertiesFile*>(settings)) file->saveIfNeeded();
}

// Writes the state only when it has actually changed, so an instrument left running all night
// touches the disk once. Hashing the block is cheaper than deciding what counts as a change:
// every knob, the matrix, the tuning and the loaded files are in there already.
void AmbientSynthProcessor::saveSession()
{
    auto* settings = standaloneSettings();
    if (settings == nullptr || !sessionRecall_) return;
    juce::MemoryBlock data;
    getStateInformation(data);
    if (data.getSize() == 0) return;
    juce::uint32 hash = 2166136261u;                     // FNV-1a over the block
    for (const auto* b = static_cast<const juce::uint8*>(data.getData()), *e = b + data.getSize(); b != e; ++b)
        hash = (hash ^ *b) * 16777619u;
    if (hash == savedStateHash_) return;
    savedStateHash_ = hash;
    settings->setValue("filterState", data.toBase64Encoding());
    if (auto* file = dynamic_cast<juce::PropertiesFile*>(settings)) file->saveIfNeeded();
}

void AmbientSynthProcessor::timerCallback() { saveSession(); }

// ---------------------------------------------------------------- OSC sink + gestures

void AmbientSynthProcessor::setParam(ParamId id, float value)
{
    if (auto* p = apvts.getParameter(paramTable()[static_cast<size_t>(id)].key))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

void AmbientSynthProcessor::setParamNormalised(ParamId id, float norm)
{
    if (auto* p = apvts.getParameter(paramTable()[static_cast<size_t>(id)].key))
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, norm));
}

void AmbientSynthProcessor::event(const ControlEvent& e)
{
    events_.push(e);   // consumed on the audio thread
}

bool AmbientSynthProcessor::setGestureMappings(const juce::String& text)
{
    if (!gestures_.parseMappings(text.toRawUTF8())) return false;
    mappingText_ = text;
    return true;
}

juce::String AmbientSynthProcessor::gestureMappings() const
{
    char buf[4096];
    const int n = gestures_.writeMappings(buf, sizeof(buf));
    return juce::String(juce::CharPointer_UTF8(buf), static_cast<size_t>(juce::jmax(0, n)));
}

void AmbientSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (int i = 0; i < kNumParams; ++i)
        engine_.setParam(static_cast<ParamId>(i), raw_[static_cast<size_t>(i)]->load());
    engine_.prepare(sampleRate, samplesPerBlock);
    scratch_.setSize(2, samplesPerBlock);
}

bool AmbientSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AmbientSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Macros are gesture inputs Custom0..7: one knob, several parameters.
    for (int m = 0; m < 8; ++m)
        gestures_.setInput(static_cast<GestureInput>(static_cast<int>(GestureInput::Custom0) + m),
                           raw_[static_cast<size_t>(static_cast<int>(ParamId::MacroA) + m)]->load());

    // Gestures write through the host's parameter system, like a MIDI controller would.
    gestures_.update(buffer.getNumSamples() / getSampleRate(), [this](ParamId id, float v) {
        if (auto* p = apvts.getParameter(paramTable()[static_cast<size_t>(id)].key))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    });
    // Events from OSC (notes, presets) arrive on the audio thread through the queue.
    ControlEvent ev;
    while (events_.pop(ev)) {
        switch (ev.type) {
        case ControlEvent::Type::NoteOn:       engine_.noteOn(ev.a, ev.b); break;
        case ControlEvent::Type::NoteOff:      engine_.noteOff(ev.a); break;
        case ControlEvent::Type::Preset:       setCurrentProgram(ev.a); break;
        case ControlEvent::Type::SoundPreset:  applySoundPreset(ev.a); break;
        case ControlEvent::Type::CosmosPreset: applyCosmosPreset(ev.a); break;
        }
    }

    // Leaving the preset map: the sound stays where the map left it, so the gliding blend
    // values become the live parameters (through the host, like a controller would).
    const bool mapNow = raw_[static_cast<size_t>(ParamId::MapActive)]->load() >= 0.5f;
    if (!mapNow && engine_.mapActive()) {
        for (const ParamDesc& d : paramTable()) {
            if (isMapParam(d.id) || isMorphParam(d.id) || isMacroParam(d.id)) continue;
            if (auto* p = apvts.getParameter(d.key)) p->setValueNotifyingHost(p->convertTo0to1(engine_.blendValue(d.id)));
        }
    }

    for (int i = 0; i < kNumParams; ++i)
        engine_.setParam(static_cast<ParamId>(i), raw_[static_cast<size_t>(i)]->load());

    // The host's play head, when there is one (the standalone has none and the engine then runs
    // its own clock).
    if (auto* ph = getPlayHead()) {
        if (const auto pos = ph->getPosition()) {
            const double bpm = pos->getBpm().hasValue() ? *pos->getBpm() : 0.0;
            const double ppq = pos->getPpqPosition().hasValue() ? *pos->getPpqPosition() : 0.0;
            engine_.setHostClock(bpm, ppq, pos->getIsPlaying());
        }
    }

    // Route: the engine walks it and moves the cursor; mirror the cursor (and the switches the
    // route flips) into the host parameters so the GUI and automation see it.
    {
        float rx, ry, rr;
        const bool wasActive = raw_[static_cast<size_t>(ParamId::RouteActive)]->load() >= 0.5f;
        engine_.routeStep(buffer.getNumSamples() / getSampleRate(), rx, ry, rr);
        if (wasActive) {
            auto mirror = [this](ParamId id) {
                if (auto* p = apvts.getParameter(paramTable()[static_cast<size_t>(id)].key)) {
                    const float v = engine_.getParam(id);
                    if (std::fabs(p->convertFrom0to1(p->getValue()) - v) > 1e-6f) p->setValueNotifyingHost(p->convertTo0to1(v));
                }
            };
            mirror(ParamId::MapX); mirror(ParamId::MapY); mirror(ParamId::MapRadius); mirror(ParamId::MapActive); mirror(ParamId::RouteActive);
        }
    }

    // Set timeline: playback feeds parameters (through the host) and notes; recording logs what
    // changed since the last block plus the notes, at the block's time.
    const double blockSeconds = buffer.getNumSamples() / getSampleRate();
    if (setPlaying_.load()) {
        const double t0 = setClock_, t1 = setClock_ + blockSeconds;
        setPlay_.step(t0, t1, [this](const TimelineEvent& e) {
            switch (e.type) {
            case TimelineEvent::Type::Param:
                if (auto* p = apvts.getParameter(paramTable()[static_cast<size_t>(e.a)].key)) p->setValueNotifyingHost(p->convertTo0to1(e.v));
                engine_.setParam(static_cast<ParamId>(e.a), e.v);
                break;
            case TimelineEvent::Type::NoteOn:  engine_.noteOn(e.a, e.v); break;
            case TimelineEvent::Type::NoteOff: engine_.noteOff(e.a); break;
            }
        });
        setClock_ = t1;
        setTime_.store(setClock_);
        if (setPlay_.finished() && setClock_ > setPlay_.length() + 1.0) setPlaying_.store(false);
    }
    if (setRecording_.load()) {
        for (int i = 0; i < kNumParams; ++i) {
            const float v = raw_[static_cast<size_t>(i)]->load();
            if (v != setLast_[i]) { setRec_.add({ setClock_, TimelineEvent::Type::Param, i, v }); setLast_[i] = v; }
        }
        for (const auto meta : midi) {
            const auto m = meta.getMessage();
            if (m.isNoteOn()) setRec_.add({ setClock_ + meta.samplePosition / getSampleRate(), TimelineEvent::Type::NoteOn, m.getNoteNumber(), m.getFloatVelocity() });
            else if (m.isNoteOff()) setRec_.add({ setClock_ + meta.samplePosition / getSampleRate(), TimelineEvent::Type::NoteOff, m.getNoteNumber(), 0.0f });
        }
        setClock_ += blockSeconds;
        setTime_.store(setClock_);
    }

    const bool mpe = raw_[static_cast<size_t>(ParamId::MpeOn)]->load() >= 0.5f;
    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        const int ch = juce::jlimit(1, 16, m.getChannel()) - 1;
        // With MPE every finger has its own channel; without it, expression addresses every
        // sounding voice, which is what channel pressure and the wheel mean on a plain keyboard.
        const int expressed = mpe ? mpeNote_[ch] : -1;
        if (m.isNoteOn())            { if (mpe) mpeNote_[ch] = m.getNoteNumber(); engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity()); }
        else if (m.isNoteOff())      { if (mpe && mpeNote_[ch] == m.getNoteNumber()) mpeNote_[ch] = -1; engine_.noteOff(m.getNoteNumber()); }
        else if (m.isPitchWheel())   engine_.setBend(expressed, (m.getPitchWheelValue() - 8192) / 8192.0f);
        else if (m.isChannelPressure()) engine_.setPressure(expressed, m.getChannelPressureValue() / 127.0f);
        else if (m.isAftertouch())   engine_.setPressure(m.getNoteNumber(), m.getAfterTouchValue() / 127.0f);
        else if (m.isController() && m.getControllerNumber() == 74) engine_.setSlide(expressed, m.getControllerValue() / 127.0f);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine_.allNotesOff();
        else if (m.isMidiClock()) {   // 24 a quarter; the interval between two carries the tempo
            const double t = clockSamples_ + meta.samplePosition;
            engine_.midiClockTick(lastClockSample_ >= 0.0 ? (t - lastClockSample_) / getSampleRate() : 0.0);
            lastClockSample_ = t;
        }
        else if (m.isMidiStart())    engine_.midiClockStart();
        else if (m.isMidiContinue()) engine_.midiClockContinue();
        else if (m.isMidiStop())     engine_.midiClockStop();
        else if (m.isController()) {
            const int cc = m.getControllerNumber();
            if (cc < 0 || cc >= 128) continue;
            const int learn = learnTarget_.exchange(-1);
            if (learn >= 0) {
                for (auto& c : ccMap_) if (c.load() == learn) c.store(-1);   // one controller per parameter
                ccMap_[static_cast<size_t>(cc)].store(learn);
            }
            const int target = ccMap_[static_cast<size_t>(cc)].load();
            if (target >= 0)
                if (auto* p = apvts.getParameter(paramTable()[static_cast<size_t>(target)].key))
                    p->setValueNotifyingHost(static_cast<float>(m.getControllerValue()) / 127.0f);
        }
    }
    midi.clear();
    clockSamples_ += buffer.getNumSamples();

    const int n = buffer.getNumSamples();
    if (scratch_.getNumSamples() < n) scratch_.setSize(2, n, false, false, true);
    if (buffer.getNumChannels() >= 2) {
        engine_.process(buffer.getWritePointer(0), buffer.getWritePointer(1), n);
    } else if (buffer.getNumChannels() == 1) {
        engine_.process(scratch_.getWritePointer(0), scratch_.getWritePointer(1), n);
        buffer.copyFrom(0, 0, scratch_, 0, 0, n);
        buffer.addFrom(0, 0, scratch_, 1, 0, n);
        buffer.applyGain(0.5f);
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);

    if (recording_.load(std::memory_order_relaxed)) {
        const juce::ScopedTryLock sl(recordLock_);
        if (sl.isLocked() && recordWriter_ != nullptr) {
            const float* chans[2] = { buffer.getReadPointer(0), buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0) };
            recordWriter_->write(chans, n);
            recordedSamples_.fetch_add(n, std::memory_order_relaxed);
        }
    }
}

bool AmbientSynthProcessor::startRecording(const juce::File& file)
{
    stopRecording();
    file.deleteFile();
    auto stream = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
    if (stream == nullptr || stream->failedToOpen()) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), getSampleRate(), 2, 32, {}, 0));
    if (writer == nullptr) return false;
    stream.release();   // the writer owns the stream now
    recordThread_.startThread();
    {
        const juce::ScopedLock sl(recordLock_);
        recordWriter_ = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(writer.release(), recordThread_, 1 << 18);
        recordedSamples_.store(0);
    }
    recording_.store(true);
    return true;
}

void AmbientSynthProcessor::stopRecording()
{
    recording_.store(false);
    {
        const juce::ScopedLock sl(recordLock_);
        recordWriter_.reset();
    }
    recordThread_.stopThread(2000);
}

int AmbientSynthProcessor::getNumPrograms() { return numPresets(); }

const juce::String AmbientSynthProcessor::getProgramName(int index)
{
    return (index >= 0 && index < numPresets()) ? juce::String(preset(index).name) : juce::String();
}

void AmbientSynthProcessor::loadPresetFiles(int index)
{
    // A pack preset can bring its own sample, wavetable and impulse; paths are relative to the pack.
    const juce::String tex(juce::CharPointer_UTF8(presetFilePath(index, 0)));
    const juce::String tab(juce::CharPointer_UTF8(presetFilePath(index, 1)));
    const juce::String imp(juce::CharPointer_UTF8(presetFilePath(index, 2)));
    // The texture field is one path (every slot gets it, as always) or up to four separated by
    // ';', one per slot, an empty one meaning that slot has none. The per-slot form is explicit
    // about every slot, so a slot it leaves empty is cleared rather than left holding whatever
    // the previous preset put there.
    if (tex.isNotEmpty()) {
        if (!tex.containsChar(';')) {
            if (juce::File(tex).existsAsFile()) loadTextureFile(juce::File(tex));
        } else {
            const juce::StringArray parts = juce::StringArray::fromTokens(tex, ";", "");
            for (int k = 0; k < ambient::kSlots; ++k) {
                const juce::String p = k < parts.size() ? parts[k].trim() : juce::String();
                if (p.isNotEmpty() && juce::File(p).existsAsFile()) loadTextureFile(k, juce::File(p));
                else { engine_.clearTexture(k); textureFile_[k] = juce::File(); }
            }
        }
    }
    if (tab.isNotEmpty() && juce::File(tab).existsAsFile()) loadWavetableFile(juce::File(tab));
    if (imp.isNotEmpty() && juce::File(imp).existsAsFile()) loadImpulseFile(juce::File(imp));
}

void AmbientSynthProcessor::applyScoped(const Preset& pr, PresetScope scope)
{
    applyPreset(pr, [this](ParamId id, float v) {
        if (auto* p = apvts.getParameter(paramTable()[static_cast<size_t>(id)].key))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    }, scope);
    // The matrix rows and the envelope shapes are data, not parameters, so they do not travel
    // through the parameter tree: the engine takes them straight from the preset.
    if (scope != PresetScope::Cosmos) engine_.applyPresetModulation(pr);
}

void AmbientSynthProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= numPresets()) return;
    currentProgram_ = index;
    soundIndex_ = index;
    soundName_ = preset(index).name;
    zIndex_ = -1; strikeIndex_ = -1;
    cosmosIndex_ = -1;   // the program brought its own Cosmos layer
    applyScoped(preset(index), PresetScope::Full);
    loadPresetFiles(index);
}

void AmbientSynthProcessor::applySoundPreset(int index)
{
    if (index < 0 || index >= numPresets()) return;
    soundIndex_ = index;
    soundName_ = preset(index).name;
    // A sound preset brings its own Z-plane and its own Strike with it, so whatever layer preset
    // was picked before is no longer what is loaded. Cleared rather than left standing: a box
    // that names a filter the sound preset has just overwritten is worse than an empty one.
    zIndex_ = -1; strikeIndex_ = -1;
    applyScoped(preset(index), PresetScope::Sound);
    loadPresetFiles(index);
    applyLevelMatch(index);
}

// The loudness of every preset was measured from a twelve-second render (Tools/preset_map.py
// for the built-ins, measure_packs.py for the library). A preset that was never measured has
// none, and is then left alone rather than guessed at.
void AmbientSynthProcessor::applyLevelMatch(int index)
{
    if (!levelMatch_ || index < 0 || index >= numPresetMeta()) return;
    const float loud = presetMeta(index).loudDb;
    if (loud >= -0.5f || loud < -80.0f) return;         // 0 means "never measured"
    constexpr float kTarget = -20.5f;                   // the measured median of the built-in presets
    if (auto* p = apvts.getParameter(paramDesc(ParamId::MasterGain).key)) {
        const float now = engine_.getParam(ParamId::MasterGain);
        const float want = juce::jlimit(-40.0f, 12.0f, now + juce::jlimit(-12.0f, 12.0f, kTarget - loud));
        p->setValueNotifyingHost(p->convertTo0to1(want));
    }
}

void AmbientSynthProcessor::applyCosmosPreset(int index)
{
    if (index < 0 || index >= numCosmosPresets()) return;
    cosmosIndex_ = index;
    cosmosName_ = cosmosPreset(index).name;
    applyScoped(cosmosPreset(index), PresetScope::Cosmos);
}

void AmbientSynthProcessor::applyZPreset(int index)
{
    if (index < 0 || index >= numZPresets()) return;
    zIndex_ = index;
    applyScoped(zPreset(index), PresetScope::ZPlane);
}

void AmbientSynthProcessor::applyStrikePreset(int index)
{
    if (index < 0 || index >= numStrikePresets()) return;
    strikeIndex_ = index;
    applyScoped(strikePreset(index), PresetScope::Strike);
}

juce::AudioProcessorEditor* AmbientSynthProcessor::createEditor()
{
    return new AmbientSynthEditor(*this);
}

bool AmbientSynthProcessor::loadScalaText(const juce::String& text, const juce::String& displayName)
{
    FixedScale s;
    if (!parseScala(text.toRawUTF8(), s)) return false;
    engine_.setUserScale(s);
    scalaText_ = text;
    userScaleName_ = displayName.isNotEmpty() ? displayName : juce::String(s.name);
    if (auto* p = apvts.getParameter("scale")) {
        const float norm = p->convertTo0to1(static_cast<float>(kNumScaleChoices - 1));
        p->beginChangeGesture();
        p->setValueNotifyingHost(norm);
        p->endChangeGesture();
    }
    return true;
}

bool AmbientSynthProcessor::readMono(const juce::File& file, std::vector<float>& mono, double& sampleRate)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const juce::int64 maxLen = static_cast<juce::int64>(reader->sampleRate * 120.0);   // two minutes is plenty for a texture
    const int n = static_cast<int>(juce::jmin(reader->lengthInSamples, maxLen));
    juce::AudioBuffer<float> buf(static_cast<int>(reader->numChannels), n);
    if (!reader->read(&buf, 0, n, 0, true, true)) return false;
    mono.assign(static_cast<size_t>(n), 0.0f);
    const float inv = 1.0f / static_cast<float>(buf.getNumChannels());
    for (int c = 0; c < buf.getNumChannels(); ++c) {
        const float* s = buf.getReadPointer(c);
        for (int i = 0; i < n; ++i) mono[static_cast<size_t>(i)] += s[i] * inv;
    }
    sampleRate = reader->sampleRate;
    return true;
}

bool AmbientSynthProcessor::loadTextureFile(int slot, const juce::File& file)
{
    if (slot < 0 || slot >= ambient::kSlots) return false;
    std::vector<float> mono; double rate = 0.0;
    if (!readMono(file, mono, rate)) return false;
    const double base = baseHzFromName(file.getFileName().toRawUTF8());   // "_A3" suffix from TextureGen
    engine_.setTexture(slot, mono.data(), static_cast<int>(mono.size()), rate, base > 0.0 ? base : 261.6256,
                       ambient::loopFromName(file.getFileName().toRawUTF8()));
    textureFile_[slot] = file;
    return true;
}

bool AmbientSynthProcessor::loadTextureFile(const juce::File& file)
{
    std::vector<float> mono; double rate = 0.0;
    if (!readMono(file, mono, rate)) return false;
    const double base = baseHzFromName(file.getFileName().toRawUTF8());
    engine_.setTexture(mono.data(), static_cast<int>(mono.size()), rate, base > 0.0 ? base : 261.6256,
                       ambient::loopFromName(file.getFileName().toRawUTF8()));
    for (auto& f : textureFile_) f = file;
    return true;
}

bool AmbientSynthProcessor::loadImpulseFile(const juce::File& file, bool second)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int n = static_cast<int>(juce::jmin(reader->lengthInSamples, static_cast<juce::int64>(reader->sampleRate * 12.0)));
    juce::AudioBuffer<float> buf(static_cast<int>(reader->numChannels), n);
    if (!reader->read(&buf, 0, n, 0, true, true)) return false;
    const float* L = buf.getReadPointer(0);
    const float* R = buf.getNumChannels() > 1 ? buf.getReadPointer(1) : nullptr;
    if (second) { engine_.setImpulseB(L, R, n, reader->sampleRate); impulseBFile_ = file; }
    else        { engine_.setImpulse(L, R, n, reader->sampleRate);  impulseFile_ = file; }
    return true;
}

bool AmbientSynthProcessor::loadWavetableFile(const juce::File& file)
{
    std::vector<float> mono; double rate = 0.0;
    if (!readMono(file, mono, rate)) return false;
    if (!engine_.loadUserWavetable(mono.data(), static_cast<int>(mono.size()))) return false;
    wavetableFile_ = file;
    return true;
}

void AmbientSynthProcessor::startSetRecording()
{
    setPlaying_.store(false);
    setRec_.clear();
    // The starting state goes in at t = 0 so playback begins from the same sound.
    for (int i = 0; i < kNumParams; ++i) {
        const float v = raw_[static_cast<size_t>(i)]->load();
        setLast_[i] = v;
        setRec_.add({ 0.0, TimelineEvent::Type::Param, i, v });
    }
    setClock_ = 0.0;
    setTime_.store(0.0);
    setRecording_.store(true);
}

bool AmbientSynthProcessor::stopSetRecording(const juce::File& saveTo)
{
    setRecording_.store(false);
    if (saveTo == juce::File()) return true;
    return setRec_.save(saveTo.getFullPathName().toRawUTF8());
}

bool AmbientSynthProcessor::playSetFile(const juce::File& file)
{
    setPlaying_.store(false);
    setRecording_.store(false);
    if (!setPlay_.load(file.getFullPathName().toRawUTF8())) return false;
    setPlay_.seek(0.0);
    setClock_ = 0.0;
    setTime_.store(0.0);
    setPlaying_.store(true);
    return true;
}

bool AmbientSynthProcessor::savePresetFile(const juce::File& file)
{
    juce::MemoryBlock block;
    getStateInformation(block);
    auto xml = getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    if (xml == nullptr) return false;
    return xml->writeTo(file);
}

bool AmbientSynthProcessor::loadPresetFile(const juce::File& file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName(apvts.state.getType())) return false;
    juce::MemoryBlock block;
    copyXmlToBinary(*xml, block);
    setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    return true;
}

void AmbientSynthProcessor::setMorphSlotFromPreset(int slot, int presetIndex)
{
    if (presetIndex < 0 || presetIndex >= numPresets()) return;
    float values[kNumParams];
    for (int i = 0; i < kNumParams; ++i) values[i] = raw_[static_cast<size_t>(i)]->load();
    applyPreset(preset(presetIndex), [&](ParamId id, float v) { values[static_cast<int>(id)] = v; });
    engine_.setMorphSlot(slot, values);
    slotName_[slot & 1] = preset(presetIndex).name;
}

void AmbientSynthProcessor::setMorphSlotFromCurrent(int slot)
{
    float values[kNumParams];
    for (int i = 0; i < kNumParams; ++i) values[i] = raw_[static_cast<size_t>(i)]->load();
    engine_.setMorphSlot(slot, values);
    slotName_[slot & 1] = "(captured)";
}

void AmbientSynthProcessor::clearMidiLearn(ParamId id)
{
    for (auto& c : ccMap_) if (c.load() == static_cast<int>(id)) c.store(-1);
    if (learnTarget_.load() == static_cast<int>(id)) learnTarget_.store(-1);
}

int AmbientSynthProcessor::midiCcFor(ParamId id) const
{
    for (int cc = 0; cc < 128; ++cc) if (ccMap_[static_cast<size_t>(cc)].load() == static_cast<int>(id)) return cc;
    return -1;
}

void AmbientSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (scalaText_.isNotEmpty()) {
        state.setProperty("scalaText", scalaText_, nullptr);
        state.setProperty("scalaName", userScaleName_, nullptr);
    }
    state.setProperty("gestureMappings", gestureMappings(), nullptr);
    {   // modulation: the matrix and the six envelope shapes (see ambient/Modulation.h)
        char buf[4096];
        if (engine_.writeModMatrix(buf, sizeof(buf)) > 0) state.setProperty("modMatrix", juce::String(buf), nullptr);
        juce::String envs;
        for (int i = 0; i < ambient::kNumModEnvs; ++i) {
            if (engine_.writeEnvShape(i, buf, sizeof(buf)) > 0) envs += juce::String(buf);
            if (i + 1 < ambient::kNumModEnvs) envs += "~";
        }
        state.setProperty("modEnvs", envs, nullptr);
    }
    if (!favourites_.isZero()) state.setProperty("favourites", favourites_.toString(16), nullptr);
    if (routeText_.isNotEmpty()) state.setProperty("route", routeText_, nullptr);
    // One property per slot. An older state carries a single "textureFile", which is read below
    // as "the same clip in every slot" -- the only thing it could have meant at the time.
    for (int k = 0; k < ambient::kSlots; ++k)
        if (textureFile_[k].existsAsFile())
            state.setProperty("textureFile" + juce::String(k + 1), textureFile_[k].getFullPathName(), nullptr);
    if (wavetableFile_.existsAsFile()) state.setProperty("wavetableFile", wavetableFile_.getFullPathName(), nullptr);
    if (impulseFile_.existsAsFile())   state.setProperty("impulseFile", impulseFile_.getFullPathName(), nullptr);
    if (impulseBFile_.existsAsFile())  state.setProperty("impulseBFile", impulseBFile_.getFullPathName(), nullptr);
    // The names of the two loaded presets, so the boxes still say what is loaded after a restart.
    if (soundName_.isNotEmpty())  state.setProperty("soundPreset", soundName_, nullptr);
    if (cosmosName_.isNotEmpty()) state.setProperty("cosmosPreset", cosmosName_, nullptr);
    if (compact_) state.setProperty("compact", 1, nullptr);          // how the editor is laid out
    if (levelMatch_) state.setProperty("levelMatch", 1, nullptr);
    juce::ValueTree midi("midi");
    for (int cc = 0; cc < 128; ++cc) {
        const int target = ccMap_[static_cast<size_t>(cc)].load();
        if (target >= 0) midi.setProperty("cc" + juce::String(cc), paramTable()[static_cast<size_t>(target)].key, nullptr);
    }
    state.addChild(midi, -1, nullptr);
    for (int slot = 0; slot < 2; ++slot) {
        juce::ValueTree m(slot == 0 ? "morphA" : "morphB");
        m.setProperty("name", slotName_[slot], nullptr);
        float values[kNumParams];
        engine_.morphSlot(slot, values);
        for (int i = 0; i < kNumParams; ++i) m.setProperty(paramTable()[static_cast<size_t>(i)].key, values[i], nullptr);
        state.addChild(m, -1, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void AmbientSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes)) {
        if (xml->hasTagName(apvts.state.getType())) {
            auto tree = juce::ValueTree::fromXml(*xml);
            const juce::String text = tree.getProperty("scalaText").toString();
            const juce::String name = tree.getProperty("scalaName").toString();
            for (auto& c : ccMap_) c.store(-1);
            const juce::String mappings = tree.getProperty("gestureMappings").toString();
            if (mappings.isNotEmpty()) setGestureMappings(mappings);
            auto midi = tree.getChildWithName("midi");
            if (midi.isValid())
                for (int cc = 0; cc < 128; ++cc) {
                    const juce::String key = midi.getProperty("cc" + juce::String(cc)).toString();
                    if (const ParamDesc* d = findParam(key.toRawUTF8())) ccMap_[static_cast<size_t>(cc)].store(static_cast<int>(d->id));
                }
            for (int slot = 0; slot < 2; ++slot) {
                auto m = tree.getChildWithName(slot == 0 ? "morphA" : "morphB");
                if (!m.isValid()) continue;
                float values[kNumParams];
                for (int i = 0; i < kNumParams; ++i) {
                    const ParamDesc& d = paramTable()[static_cast<size_t>(i)];
                    values[i] = m.hasProperty(d.key) ? static_cast<float>(static_cast<double>(m.getProperty(d.key))) : d.def;
                }
                engine_.setMorphSlot(slot, values);
                slotName_[slot] = m.getProperty("name").toString();
            }
            tree.removeChild(tree.getChildWithName("midi"), nullptr);
            tree.removeChild(tree.getChildWithName("morphA"), nullptr);
            tree.removeChild(tree.getChildWithName("morphB"), nullptr);
            const juce::String texPath = tree.getProperty("textureFile").toString();
            juce::String texPaths[ambient::kSlots];
            for (int k = 0; k < ambient::kSlots; ++k) texPaths[k] = tree.getProperty("textureFile" + juce::String(k + 1)).toString();
            const juce::String tabPath = tree.getProperty("wavetableFile").toString();
            const juce::String favs = tree.getProperty("favourites").toString();
            const juce::String irPath = tree.getProperty("impulseFile").toString();
            const juce::String irBPath = tree.getProperty("impulseBFile").toString();
            // Which presets the two boxes should say are loaded. Looked up by name, and simply
            // not selected when the pack that held it is gone -- the sound is in the state
            // either way, only the label would have been a lie.
            soundName_  = tree.getProperty("soundPreset").toString();
            cosmosName_ = tree.getProperty("cosmosPreset").toString();
            // A whole state came in: the layer boxes have no name to show, because the state is
            // the parameters themselves and not the presets they may once have come from.
            zIndex_ = -1; strikeIndex_ = -1;
            if (soundName_.isNotEmpty()) {
                soundIndex_ = -1;
                for (int i = 0; i < numPresets(); ++i) if (soundName_ == preset(i).name) { soundIndex_ = i; break; }
            }
            if (cosmosName_.isNotEmpty()) {
                cosmosIndex_ = -1;
                for (int i = 0; i < numCosmosPresets(); ++i) if (cosmosName_ == cosmosPreset(i).name) { cosmosIndex_ = i; break; }
            }
            compact_ = static_cast<int>(tree.getProperty("compact", 0)) != 0;
            levelMatch_ = static_cast<int>(tree.getProperty("levelMatch", 0)) != 0;
            const juce::String route = tree.getProperty("route").toString();
            const juce::String modMatrix = tree.getProperty("modMatrix").toString();
            const juce::String modEnvs = tree.getProperty("modEnvs").toString();
            if (modMatrix.isNotEmpty()) engine_.setModMatrixText(modMatrix.toRawUTF8());
            if (modEnvs.isNotEmpty()) {
                const juce::StringArray parts = juce::StringArray::fromTokens(modEnvs, "~", "");
                for (int i = 0; i < juce::jmin(parts.size(), ambient::kNumModEnvs); ++i)
                    if (parts[i].isNotEmpty()) engine_.setEnvShape(i, parts[i].toRawUTF8());
            }
            if (favs.isNotEmpty()) favourites_.parseString(favs, 16);
            if (route.isNotEmpty()) setRouteText(route);
            tree.removeProperty("route", nullptr);
            tree.removeProperty("textureFile", nullptr);
            for (int k = 0; k < ambient::kSlots; ++k) tree.removeProperty("textureFile" + juce::String(k + 1), nullptr);
            tree.removeProperty("wavetableFile", nullptr);
            tree.removeProperty("impulseFile", nullptr);
            tree.removeProperty("favourites", nullptr);
            tree.removeProperty("soundPreset", nullptr);
            tree.removeProperty("cosmosPreset", nullptr);
            apvts.replaceState(tree);
            if (text.isNotEmpty()) loadScalaText(text, name);
            if (texPath.isNotEmpty() && juce::File(texPath).existsAsFile()) loadTextureFile(juce::File(texPath));
            for (int k = 0; k < ambient::kSlots; ++k)
                if (texPaths[k].isNotEmpty() && juce::File(texPaths[k]).existsAsFile()) loadTextureFile(k, juce::File(texPaths[k]));
            if (tabPath.isNotEmpty() && juce::File(tabPath).existsAsFile()) loadWavetableFile(juce::File(tabPath));
            if (irPath.isNotEmpty() && juce::File(irPath).existsAsFile()) loadImpulseFile(juce::File(irPath));
            if (irBPath.isNotEmpty() && juce::File(irBPath).existsAsFile()) loadImpulseFile(juce::File(irBPath), true);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmbientSynthProcessor();
}
