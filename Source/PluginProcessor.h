#pragma once
#include <JuceHeader.h>
#include "LossEngine.h"
#include "NoiseGenerator.h"
#include "FilterSection.h"
#include "ReverbEngine.h"
#include "PresetManager.h"
#include "SpectrumAnalyser.h"

class ArtifactAudioProcessor : public juce::AudioProcessor
{
public:
    ArtifactAudioProcessor();
    ~ArtifactAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // ── Preset Manager — public so GUI can call save/load/browse ─────────────
    PresetManager presetManager;

    // ── Spectrum Analyser — GUI reads, audio thread writes ────────────────────
    SpectrumAnalyser spectrumAnalyser;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ── Phase 2: Loss Engine ──────────────────────────────────────────────────
    LossEngine lossEngineL;
    LossEngine lossEngineR;

    // ── Phase 5: Noise Generator ──────────────────────────────────────────────
    NoiseGenerator noiseGenL;
    NoiseGenerator noiseGenR;

    // ── Phase 6: Filter Section ───────────────────────────────────────────────
    FilterSection filterSection;

    // ── Phase 7: Reverb Engine ────────────────────────────────────────────────
    ReverbEngine reverbEngine;

    juce::AudioBuffer<float> dryBuffer;

    static int hopSizeFromSpeed(float lossSpeed)
    {
        const int hopSize = (int)juce::jmap(lossSpeed,
            100.0f, 0.0f,
            (float)(LossEngine::fftSize / 8),
            (float)(LossEngine::fftSize / 2));
        return juce::jlimit(32, LossEngine::fftSize / 2, hopSize);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtifactAudioProcessor)
};