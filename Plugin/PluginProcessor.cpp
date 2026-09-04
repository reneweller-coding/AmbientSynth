#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ambient/Tuning.h"

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
                    .withStringFromValueFunction([decimals](float v, int) { return juce::String(v, decimals); })));
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
    for (int i = 0; i < kNumParams; ++i)
        engine_.setParam(static_cast<ParamId>(i), raw_[static_cast<size_t>(i)]->load());

    for (const auto meta : midi) {
        const auto m = meta.getMessage();
        if (m.isNoteOn())            engine_.noteOn(m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())      engine_.noteOff(m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff()) engine_.allNotesOff();
    }
    midi.clear();

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

void AmbientSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (scalaText_.isNotEmpty()) {
        state.setProperty("scalaText", scalaText_, nullptr);
        state.setProperty("scalaName", userScaleName_, nullptr);
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
            apvts.replaceState(tree);
            if (text.isNotEmpty()) loadScalaText(text, name);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AmbientSynthProcessor();
}
