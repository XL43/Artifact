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
                            "Packet Disorder", "Disorder + Standard" },  // ← added
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
    apvts(*this, nullptr, "Parameters", createParameterLayout()),
    presetManager(apvts)
{
}

ArtifactAudioProcessor::~ArtifactAudioProcessor() {}

//==============================================================================
void ArtifactAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    lossEngineL.prepare(sampleRate, samplesPerBlock);
    lossEngineR.prepare(sampleRate, samplesPerBlock);
    noiseGenL.prepare(sampleRate, samplesPerBlock);
    noiseGenR.prepare(sampleRate, samplesPerBlock);
    filterSection.prepare(sampleRate, samplesPerBlock);
    reverbEngine.prepare(sampleRate, samplesPerBlock);
    dryBuffer.setSize(2, samplesPerBlock, false, true, true);
}

void ArtifactAudioProcessor::releaseResources()
{
    lossEngineL.reset();
    lossEngineR.reset();
    noiseGenL.reset();
    noiseGenR.reset();
    filterSection.reset();
    reverbEngine.reset();
}

//==============================================================================
void ArtifactAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused(midiMessages);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ── 1. Snapshot all parameters ────────────────────────────────────────────
    const float lossAmount = apvts.getRawParameterValue(ParamIDs::lossAmount)->load() / 100.0f;
    const float lossSpeed = apvts.getRawParameterValue(ParamIDs::lossSpeed)->load();
    const float lossGain = apvts.getRawParameterValue(ParamIDs::lossGain)->load();
    const float magnitude = apvts.getRawParameterValue(ParamIDs::magnitude)->load() / 100.0f;
    const float masterMix = apvts.getRawParameterValue(ParamIDs::masterMix)->load() / 100.0f;
    const float preGain = apvts.getRawParameterValue(ParamIDs::preprocessGain)->load();
    const float postGain = apvts.getRawParameterValue(ParamIDs::postprocessGain)->load();
    const bool  bypassed = apvts.getRawParameterValue(ParamIDs::masterBypass)->load() > 0.5f;
    const float noiseAmount = apvts.getRawParameterValue(ParamIDs::noiseAmount)->load() / 100.0f;
    const float noiseBias = apvts.getRawParameterValue(ParamIDs::noiseBias)->load();
    const bool  noiseEnabled = apvts.getRawParameterValue(ParamIDs::noiseEnabled)->load() > 0.5f;

    const float verbAmountRaw = apvts.getRawParameterValue(ParamIDs::verbAmount)->load() / 100.0f;
    const float verbAmount = verbAmountRaw * magnitude;

    const float verbDecay = apvts.getRawParameterValue(ParamIDs::verbDecay)->load() / 100.0f;
    const float lowCutHz = apvts.getRawParameterValue(ParamIDs::lowCutFreq)->load();
    const float highCutHz = apvts.getRawParameterValue(ParamIDs::highCutFreq)->load();
    const float autoGainAmt = apvts.getRawParameterValue(ParamIDs::autoGain)->load() / 100.0f;
    const float gateThreshDB = apvts.getRawParameterValue(ParamIDs::gateThreshold)->load();

    const auto noiseColor = static_cast<NoiseColor>  ((int)apvts.getRawParameterValue(ParamIDs::noiseColor)->load());
    const auto filterMode = static_cast<FilterMode>  ((int)apvts.getRawParameterValue(ParamIDs::filterMode)->load());
    const auto filterSlope = static_cast<FilterSlope> ((int)apvts.getRawParameterValue(ParamIDs::filterSlope)->load());
    const auto lossMode = static_cast<LossMode>    ((int)apvts.getRawParameterValue(ParamIDs::lossMode)->load());
    const auto codecMode = static_cast<CodecMode>   ((int)apvts.getRawParameterValue(ParamIDs::codecMode)->load());
    const int  verbPosition = (int)apvts.getRawParameterValue(ParamIDs::verbPosition)->load();
    const int  stereoMode = (int)apvts.getRawParameterValue(ParamIDs::stereoMode)->load();
    const int  weighting = (int)apvts.getRawParameterValue(ParamIDs::weighting)->load();
    const bool perceptual = (weighting == 0); // 0 = Perceptual, 1 = Flat

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

    // ── 5. Magnitude scaling ──────────────────────────────────────────────────
    const float effectiveLoss = lossAmount * magnitude;
    const float effectiveNoise = noiseAmount * magnitude;
    const int   hopSize = hopSizeFromSpeed(lossSpeed);

    // ── 6. Filter (pre-Loss) ──────────────────────────────────────────────────
    // Magnitude blends cutoff frequencies toward full-open range.
    // At magnitude = 0: lowCut = 20Hz, highCut = 20kHz → effectively bypassed.
    // At magnitude = 1: cutoffs are exactly as the user set them.
    const float magLowCut = juce::jmap(magnitude, 0.0f, 1.0f, 20.0f, lowCutHz);
    const float magHighCut = juce::jmap(magnitude, 0.0f, 1.0f, 20000.0f, highCutHz);
    const bool filterActive = (magLowCut > 21.0f || magHighCut < 19999.0f);
    if (filterActive)
        filterSection.processBlock(buffer, filterMode, magLowCut, magHighCut, filterSlope);

    // ── 7. Verb Pre ───────────────────────────────────────────────────────────
    if (verbPosition == 0 && verbAmount > 0.0001f)
        reverbEngine.processBlock(buffer, verbAmount, verbDecay);

    // ── 8. Noise injection ────────────────────────────────────────────────────
    if (noiseEnabled && effectiveNoise > 0.0001f)
    {
        if (numChannels >= 1)
            noiseGenL.process(buffer.getWritePointer(0), numSamples,
                effectiveNoise, noiseColor, noiseBias);
        if (numChannels >= 2)
            noiseGenR.process(buffer.getWritePointer(1), numSamples,
                effectiveNoise, noiseColor, noiseBias);
    }

    // ── 9. Stereo Mode routing + Loss Engine ──────────────────────────────────
    // Measure pre-Loss RMS for Auto Gain compensation
    float preLossRMS = 0.0f;
    if (autoGainAmt > 0.0001f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                preLossRMS += data[i] * data[i];
        }
        preLossRMS = std::sqrt(preLossRMS / (float)(numSamples * numChannels));
    }

    if (stereoMode == 2) // Mono — sum L+R to centre
    {
        // Average L and R into both channels
        if (numChannels >= 2)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float mono = (buffer.getSample(0, i)
                    + buffer.getSample(1, i)) * 0.5f;
                buffer.setSample(0, i, mono);
                buffer.setSample(1, i, mono);
            }
        }
        // Process both channels (identical content) through Loss
        if (numChannels >= 1)
            lossEngineL.processBlock(buffer.getWritePointer(0), numSamples,
                effectiveLoss, hopSize, lossMode, codecMode, perceptual);
        if (numChannels >= 2)
            lossEngineR.processBlock(buffer.getWritePointer(1), numSamples,
                effectiveLoss, hopSize, lossMode, codecMode, perceptual);
    }
    else if (stereoMode == 1 && numChannels >= 2) // Joint Stereo — M/S processing
    {
        // Encode to Mid/Side
        for (int i = 0; i < numSamples; ++i)
        {
            const float L = buffer.getSample(0, i);
            const float R = buffer.getSample(1, i);
            buffer.setSample(0, i, (L + R) * 0.5f);  // Mid
            buffer.setSample(1, i, (L - R) * 0.5f);  // Side
        }

        // Apply Loss only to Mid channel — Side is preserved
        // This mimics real Joint Stereo codec behaviour where the
        // stereo difference signal is kept more intact than the sum
        lossEngineL.processBlock(buffer.getWritePointer(0), numSamples,
            effectiveLoss, hopSize, lossMode, codecMode, perceptual);

        // Decode back to L/R
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid = buffer.getSample(0, i);
            const float side = buffer.getSample(1, i);
            buffer.setSample(0, i, mid + side);  // L
            buffer.setSample(1, i, mid - side);  // R
        }
    }
    else // Stereo — independent L and R (default)
    {
        if (numChannels >= 1)
            lossEngineL.processBlock(buffer.getWritePointer(0), numSamples,
                effectiveLoss, hopSize, lossMode, codecMode, perceptual);
        if (numChannels >= 2)
            lossEngineR.processBlock(buffer.getWritePointer(1), numSamples,
                effectiveLoss, hopSize, lossMode, codecMode, perceptual);
    }

    // ── 10. Auto Gain compensation ────────────────────────────────────────────
    // Measures post-Loss RMS and applies compensating gain to match pre-Loss level.
    // Blended by autoGainAmt so 0% = no compensation, 100% = full compensation.
    if (autoGainAmt > 0.0001f && preLossRMS > 0.0001f)
    {
        float postLossRMS = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                postLossRMS += data[i] * data[i];
        }
        postLossRMS = std::sqrt(postLossRMS / (float)(numSamples * numChannels));

        if (postLossRMS > 0.0001f)
        {
            // Compensating gain — clamped to max +18dB to prevent blowups
            const float compensationGain = juce::jlimit(0.0f, 8.0f,
                preLossRMS / postLossRMS);
            // Blend: 0% autoGain = gain of 1.0, 100% = full compensation
            const float blendedGain = 1.0f + (compensationGain - 1.0f) * autoGainAmt;
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.applyGain(ch, 0, numSamples, blendedGain);
        }
    }

    // ── 11. Verb Post ─────────────────────────────────────────────────────────
    if (verbPosition == 1 && verbAmount > 0.0001f)
        reverbEngine.processBlock(buffer, verbAmount, verbDecay);

    // ── 12. Gate ──────────────────────────────────────────────────────────────
    // Silence output below gateThreshold — removes very quiet artefact noise.
    // gateThreshDB default is -96dB which is effectively off.
    if (gateThreshDB > -95.9f)
    {
        const float gateLinear = juce::Decibels::decibelsToGain(gateThreshDB);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                if (std::abs(data[i]) < gateLinear)
                    data[i] = 0.0f;
        }
    }

    // ── 13. Loss Gain ─────────────────────────────────────────────────────────
    const float lossGainLinear = juce::Decibels::decibelsToGain(lossGain);
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, lossGainLinear);

    // ── 14. Master Mix blend ──────────────────────────────────────────────────
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            wet[i] = wet[i] * masterMix + dry[i] * (1.0f - masterMix);
    }

    // ── 15. Postprocess gain ──────────────────────────────────────────────────
    const float postGainLinear = juce::Decibels::decibelsToGain(postGain);
    for (int ch = 0; ch < numChannels; ++ch)
        buffer.applyGain(ch, 0, numSamples, postGainLinear);
}

//==============================================================================
void ArtifactAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    presetManager.saveStateToMemoryBlock(destData);
}

void ArtifactAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    presetManager.loadStateFromMemoryBlock(data, sizeInBytes);
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
