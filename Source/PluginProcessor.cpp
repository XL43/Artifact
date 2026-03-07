#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LossEngine.h"

//==============================================================================
namespace ParamIDs
{
    inline constexpr const char* lossMode = "lossMode";
    inline constexpr const char* lossAmount = "lossAmount";
    inline constexpr const char* lossSpeed = "lossSpeed";
    inline constexpr const char* lossGain = "lossGain";

    inline constexpr const char* noiseEnabled = "noiseEnabled";
    inline constexpr const char* noiseAmount = "noiseAmount";
    inline constexpr const char* noiseColor = "noiseColor";
    inline constexpr const char* noiseBias = "noiseBias";

    inline constexpr const char* filterMode = "filterMode";
    inline constexpr const char* lowCutFreq = "lowCutFreq";
    inline constexpr const char* highCutFreq = "highCutFreq";
    inline constexpr const char* filterSlope = "filterSlope";

    inline constexpr const char* verbAmount = "verbAmount";
    inline constexpr const char* verbPosition = "verbPosition";
    inline constexpr const char* verbDecay = "verbDecay";

    inline constexpr const char* autoGain = "autoGain";
    inline constexpr const char* gateThreshold = "gateThreshold";
    inline constexpr const char* stereoMode = "stereoMode";
    inline constexpr const char* weighting = "weighting";
    inline constexpr const char* codecMode = "codecMode";

    inline constexpr const char* preprocessGain = "preprocessGain";
    inline constexpr const char* postprocessGain = "postprocessGain";
    inline constexpr const char* magnitude = "magnitude";
    inline constexpr const char* masterMix = "masterMix";
    inline constexpr const char* masterBypass = "masterBypass";
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
ArtifactAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ── LOSS ──────────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::lossMode, "Loss Mode",
        juce::StringArray{ "Standard", "Inverse", "Phase Jitter",
                            "Packet Repeat", "Packet Loss",
                            "Std + Packet Loss", "Std + Packet Repeat",
                            "Packet Disorder" },
        0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::lossAmount, "Loss Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::lossSpeed, "Loss Speed",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::lossGain, "Loss Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    // ── NOISE ─────────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::noiseEnabled, "Noise Enabled", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::noiseAmount, "Noise Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::noiseColor, "Noise Color",
        juce::StringArray{ "White", "Pink", "Brown" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::noiseBias, "Noise Bias",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));

    // ── FILTER ────────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::filterMode, "Filter Mode",
        juce::StringArray{ "Normal", "Inverted" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::lowCutFreq, "Low Cut Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::highCutFreq, "High Cut Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), 20000.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::filterSlope, "Filter Slope",
        juce::StringArray{ "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" }, 1));

    // ── VERB ──────────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::verbAmount, "Verb Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::verbPosition, "Verb Position",
        juce::StringArray{ "Pre", "Post" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::verbDecay, "Verb Decay",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f));

    // ── ADVANCED ──────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::autoGain, "Auto Gain",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::gateThreshold, "Gate Threshold",
        juce::NormalisableRange<float>(-96.0f, 0.0f, 0.1f), -96.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::stereoMode, "Stereo Mode",
        juce::StringArray{ "Stereo", "Joint Stereo", "Mono" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::weighting, "Weighting",
        juce::StringArray{ "Perceptual", "Flat" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::codecMode, "Codec Mode",
        juce::StringArray{ "Music", "Voice", "Broadcast" }, 0));

    // ── MASTER ────────────────────────────────────────────────────────────────
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::preprocessGain, "Preprocess Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::postprocessGain, "Postprocess Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::magnitude, "Magnitude",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::masterMix, "Master Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamIDs::masterBypass, "Master Bypass", false));

    return { params.begin(), params.end() };
}

//==============================================================================
ArtifactAudioProcessor::ArtifactAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

ArtifactAudioProcessor::~ArtifactAudioProcessor() {}

//==============================================================================
void ArtifactAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lossEngineL.prepare(sampleRate, samplesPerBlock);
    lossEngineR.prepare(sampleRate, samplesPerBlock);
    dryBuffer.setSize(2, samplesPerBlock, false, true, true);
}

void ArtifactAudioProcessor::releaseResources()
{
    lossEngineL.reset();
    lossEngineR.reset();
}

//==============================================================================
void ArtifactAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ── 1. Snapshot parameters ────────────────────────────────────────────────
    const float lossAmount = apvts.getRawParameterValue(ParamIDs::lossAmount)->load() / 100.0f;
    const float lossSpeed = apvts.getRawParameterValue(ParamIDs::lossSpeed)->load();
    const float lossGain = apvts.getRawParameterValue(ParamIDs::lossGain)->load();
    const float magnitude = apvts.getRawParameterValue(ParamIDs::magnitude)->load() / 100.0f;
    const float masterMix = apvts.getRawParameterValue(ParamIDs::masterMix)->load() / 100.0f;
    const float preGain = apvts.getRawParameterValue(ParamIDs::preprocessGain)->load();
    const float postGain = apvts.getRawParameterValue(ParamIDs::postprocessGain)->load();
    const bool  bypassed = apvts.getRawParameterValue(ParamIDs::masterBypass)->load() > 0.5f;

    // ── 2. Bypass ─────────────────────────────────────────────────────────────
    if (bypassed)
        return;

    // ── 3. Save dry signal ────────────────────────────────────────────────────
    dryBuffer.setSize(numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

    // ── 4. Preprocess gain ────────────────────────────────────────────────────
    const float preGainLinear = juce::Decibels::decibelsToGain(preGain);
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, preGainLinear);

    // ── 5. Magnitude-scaled loss amount ───────────────────────────────────────
    const float effectiveLoss = lossAmount * magnitude;

    // ── 6. Hop size from Loss Speed ───────────────────────────────────────────
    const int hopSize = hopSizeFromSpeed(lossSpeed);

    // ── 7. Run Loss Engine ────────────────────────────────────────────────────
    const auto lossMode = static_cast<LossMode>(
        (int)apvts.getRawParameterValue(ParamIDs::lossMode)->load());

    if (numChannels >= 1)
        lossEngineL.processBlock(buffer.getWritePointer(0), numSamples,
            effectiveLoss, hopSize, lossMode);
    if (numChannels >= 2)
        lossEngineR.processBlock(buffer.getWritePointer(1), numSamples,
            effectiveLoss, hopSize, lossMode);

    // ── 8. Loss Gain ──────────────────────────────────────────────────────────
    const float lossGainLinear = juce::Decibels::decibelsToGain(lossGain);
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, lossGainLinear);

    // ── 9. Master Mix blend ───────────────────────────────────────────────────
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = wet[i] * masterMix + dry[i] * (1.0f - masterMix);
    }

    // ── 10. Postprocess gain ──────────────────────────────────────────────────
    const float postGainLinear = juce::Decibels::decibelsToGain(postGain);
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, postGainLinear);
}

//==============================================================================
void ArtifactAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ArtifactAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
bool ArtifactAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* ArtifactAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

const juce::String ArtifactAudioProcessor::getName() const { return JucePlugin_Name; }
bool ArtifactAudioProcessor::acceptsMidi() const { return false; }
bool ArtifactAudioProcessor::producesMidi() const { return false; }
double ArtifactAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int ArtifactAudioProcessor::getNumPrograms() { return 1; }
int ArtifactAudioProcessor::getCurrentProgram() { return 0; }
void ArtifactAudioProcessor::setCurrentProgram(int) {}
const juce::String ArtifactAudioProcessor::getProgramName(int) { return {}; }
void ArtifactAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ArtifactAudioProcessor();
}