#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ArtifactLookAndFeel.h"

class ArtifactAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ArtifactAudioProcessorEditor(ArtifactAudioProcessor&);
    ~ArtifactAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ArtifactAudioProcessor& audioProcessor;
    ArtifactLookAndFeel     laf;

    void setupRotary(juce::Slider& s, juce::Label& l,
        const juce::String& labelText,
        const juce::String& tooltip);
    void setupCombo(juce::ComboBox& cb, juce::Label& l,
        const juce::String& labelText,
        const juce::String& tooltip);
    void drawSection(juce::Graphics& g,
        juce::Rectangle<int> b,
        const juce::String& title) const;
    void updatePresetDisplay();
    void setupValueFormatting();

    void showPresetMenu();

    // ── Tooltip window ────────────────────────────────────────────────────────
    juce::TooltipWindow tooltipWindow{ this, 1200 };

    // ── Resize handle ─────────────────────────────────────────────────────────
    juce::ResizableCornerComponent resizeHandle{ this, nullptr };
    static constexpr int minW = 700;
    static constexpr int minH = 500;
    static constexpr int maxW = 1200;
    static constexpr int maxH = 900;

    // ── Preset bar ────────────────────────────────────────────────────────────
    juce::Label      pluginNameLabel;
    juce::TextButton presetPrevBtn{ "<" };
    juce::TextButton presetNextBtn{ ">" };
    juce::TextButton presetSaveBtn{ "SAVE" };
    juce::TextButton presetInitBtn{ "INIT" };
    juce::TextButton starBtn{ "FAV" };
    juce::TextButton shuffleBtn{ "RND" };
    juce::TextButton diceBtn{ "RAND" };
    juce::TextButton presetNameBtn;

    // ── Loss ──────────────────────────────────────────────────────────────────
    juce::ComboBox lossModeCombo, codecModeCombo;
    juce::Label    lossModeLabel, codecModeLabel;
    juce::Slider   lossAmountSlider, lossSpeedSlider, lossGainSlider;
    juce::Label    lossAmountLabel, lossSpeedLabel, lossGainLabel;

    // ── Noise ─────────────────────────────────────────────────────────────────
    juce::ToggleButton noiseEnabledBtn{ "Enabled" };
    juce::ComboBox     noiseColorCombo;
    juce::Slider       noiseAmountSlider, noiseBiasSlider;
    juce::Label        noiseAmountLabel, noiseBiasLabel;

    // ── Filter ────────────────────────────────────────────────────────────────
    juce::ComboBox filterModeCombo, filterSlopeCombo;
    juce::Label    filterModeLabel, filterSlopeLabel;
    juce::Slider   lowCutSlider, highCutSlider;
    juce::Label    lowCutLabel, highCutLabel;

    // ── Verb ──────────────────────────────────────────────────────────────────
    juce::ComboBox verbPositionCombo;
    juce::Label    verbPositionLabel;
    juce::Slider   verbAmountSlider, verbDecaySlider;
    juce::Label    verbAmountLabel, verbDecayLabel;

    // ── Master ────────────────────────────────────────────────────────────────
    juce::Slider       magnitudeSlider, masterMixSlider;
    juce::Label        magnitudeLabel, masterMixLabel;
    juce::Slider       preGainSlider, postGainSlider;
    juce::Label        preGainLabel, postGainLabel;
    juce::ToggleButton masterBypassBtn{ "Bypass" };

    // ── Advanced ──────────────────────────────────────────────────────────────
    juce::ComboBox stereoModeCombo, weightingCombo;
    juce::Label    stereoModeLabel, weightingLabel;
    juce::Slider   autoGainSlider, gateThreshSlider;
    juce::Label    autoGainLabel, gateThreshLabel;

    // ── Spectrum ──────────────────────────────────────────────────────────────
    SpectrumAnalyser& spectrumAnalyser;

    // ── Attachments ───────────────────────────────────────────────────────────
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<CA> lossModeAttach, codecModeAttach;
    std::unique_ptr<SA> lossAmountAttach, lossSpeedAttach, lossGainAttach;
    std::unique_ptr<BA> noiseEnabledAttach;
    std::unique_ptr<CA> noiseColorAttach;
    std::unique_ptr<SA> noiseAmountAttach, noiseBiasAttach;
    std::unique_ptr<CA> filterModeAttach, filterSlopeAttach;
    std::unique_ptr<SA> lowCutAttach, highCutAttach;
    std::unique_ptr<CA> verbPositionAttach;
    std::unique_ptr<SA> verbAmountAttach, verbDecayAttach;
    std::unique_ptr<SA> magnitudeAttach, masterMixAttach;
    std::unique_ptr<SA> preGainAttach, postGainAttach;
    std::unique_ptr<BA> masterBypassAttach;
    std::unique_ptr<CA> stereoModeAttach, weightingAttach;
    std::unique_ptr<SA> autoGainAttach, gateThreshAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtifactAudioProcessorEditor)
};