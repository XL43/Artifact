#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameter ID constants — use these everywhere instead of raw strings
// so a typo causes a compile error rather than a silent bug
//==============================================================================
namespace ParamIDs
{
    // Loss
    inline constexpr const char* lossMode = "lossMode";
    inline constexpr const char* lossAmount = "lossAmount";
    inline constexpr const char* lossSpeed = "lossSpeed";
    inline constexpr const char* lossGain = "lossGain";

    // Noise
    inline constexpr const char* noiseEnabled = "noiseEnabled";
    inline constexpr const char* noiseAmount = "noiseAmount";
    inline constexpr const char* noiseColor = "noiseColor";
    inline constexpr const char* noiseBias = "noiseBias";

    // Filter
    inline constexpr const char* filterMode = "filterMode";
    inline constexpr const char* lowCutFreq = "lowCutFreq";
    inline constexpr const char* highCutFreq = "highCutFreq";
    inline constexpr const char* filterSlope = "filterSlope";

    // Verb
    inline constexpr const char* verbAmount = "verbAmount";
    inline constexpr const char* verbPosition = "verbPosition";
    inline constexpr const char* verbDecay = "verbDecay";

    // Advanced
    inline constexpr const char* autoGain = "autoGain";
    inline constexpr const char* gateThreshold = "gateThreshold";
    inline constexpr const char* stereoMode = "stereoMode";
    inline constexpr const char* weighting = "weighting";
    inline constexpr const char* codecMode = "codecMode";

    // Master
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

    // ── LOSS ─────────────────────────────────────────────────────────────────

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::lossMode, "Loss Mode",
        juce::StringArray{ "Standard", "Inverse", "Phase Jitter",
                            "Packet Repeat", "Packet Loss",
                            "Std + Packet Loss", "Std + Packet Repeat",
                            "Packet Disorder" },
        0)); // default: Standard

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
        juce::StringArray{ "White", "Pink", "Brown" },
        0)); // default: White

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::noiseBias, "Noise Bias",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));

    // ── FILTER ────────────────────────────────────────────────────────────────

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::filterMode, "Filter Mode",
        juce::StringArray{ "Normal", "Inverted" },
        0)); // default: Normal

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::lowCutFreq, "Low Cut Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), // skew for log feel
        20.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::highCutFreq, "High Cut Freq",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f),
        20000.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::filterSlope, "Filter Slope",
        juce::StringArray{ "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" },
        1)); // default: 24 dB/oct

    // ── VERB ──────────────────────────────────────────────────────────────────

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::verbAmount, "Verb Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::verbPosition, "Verb Position",
        juce::StringArray{ "Pre", "Post" },
        0)); // default: Pre

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
        juce::StringArray{ "Stereo", "Joint Stereo", "Mono" },
        0)); // default: Stereo

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::weighting, "Weighting",
        juce::StringArray{ "Perceptual", "Flat" },
        0)); // default: Perceptual

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamIDs::codecMode, "Codec Mode",
        juce::StringArray{ "Music", "Voice", "Broadcast" },
        0)); // default: Music

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
    // Nothing here yet — DSP setup comes in Phase 2
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void ArtifactAudioProcessor::releaseResources() {}

void ArtifactAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    // Snapshot all parameter values at the top of the block — never read
    // from the APVTS inside the sample loop
    const float currentLossAmount = apvts.getRawParameterValue(ParamIDs::lossAmount)->load();
    const float currentMasterMix = apvts.getRawParameterValue(ParamIDs::masterMix)->load();
    const float currentMagnitude = apvts.getRawParameterValue(ParamIDs::magnitude)->load();
    const bool  bypassed = apvts.getRawParameterValue(ParamIDs::masterBypass)->load() > 0.5f;

    juce::ignoreUnused(currentLossAmount, currentMasterMix, currentMagnitude);

    // If bypassed, pass audio through untouched
    if (bypassed)
        return;

    // Passthrough — no DSP yet, signal passes cleanly
    // (processBlock will be filled in during Phase 2)
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
// Boilerplate below — nothing to change here
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