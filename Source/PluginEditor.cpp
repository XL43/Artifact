#include "PluginEditor.h"

//==============================================================================
ArtifactAudioProcessorEditor::ArtifactAudioProcessorEditor(ArtifactAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    spectrumAnalyser(p.spectrumAnalyser)
{
    setLookAndFeel(&laf);
    setSize(860, 610);

    auto& apvts = audioProcessor.apvts;

    // ── Preset bar ────────────────────────────────────────────────────────────
    pluginNameLabel.setText("ARTIFACT", juce::dontSendNotification);
    pluginNameLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(18.0f)).boldened());
    pluginNameLabel.setColour(juce::Label::textColourId,
        juce::Colour(ArtifactLookAndFeel::colAccent));
    addAndMakeVisible(pluginNameLabel);

    presetNameLabel.setText(audioProcessor.presetManager.getCurrentPresetName(),
        juce::dontSendNotification);
    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(12.5f)));
    addAndMakeVisible(presetNameLabel);

    presetPrevBtn.onClick = [this]
        {
            audioProcessor.presetManager.loadPreviousPreset();
            updatePresetDisplay();
        };
    addAndMakeVisible(presetPrevBtn);

    presetNextBtn.onClick = [this]
        {
            audioProcessor.presetManager.loadNextPreset();
            updatePresetDisplay();
        };
    addAndMakeVisible(presetNextBtn);

    presetSaveBtn.onClick = [this]
        {
            auto* dialog = new juce::AlertWindow("Save Preset",
                "Enter a name for this preset:",
                juce::MessageBoxIconType::NoIcon);
            dialog->addTextEditor("name",
                audioProcessor.presetManager.getCurrentPresetName());
            dialog->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
            dialog->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
            dialog->enterModalState(true,
                juce::ModalCallbackFunction::create([this, dialog](int result)
                    {
                        if (result == 1)
                        {
                            const auto name = dialog->getTextEditorContents("name");
                            if (name.isNotEmpty())
                            {
                                audioProcessor.presetManager.savePreset(name);
                                updatePresetDisplay();
                            }
                        }
                    }), true);
        };
    addAndMakeVisible(presetSaveBtn);

    presetInitBtn.onClick = [this]
        {
            audioProcessor.initializeParameters();
            presetNameLabel.setText("Default", juce::dontSendNotification);
        };
    addAndMakeVisible(presetInitBtn);

    // ── Star (favourite) ──────────────────────────────────────────────────────
    starBtn.setClickingTogglesState(true);
    starBtn.setToggleState(
        audioProcessor.presetManager.isFavourite(
            audioProcessor.presetManager.getCurrentPresetName()),
        juce::dontSendNotification);
    starBtn.onClick = [this]
        {
            audioProcessor.presetManager.toggleFavourite(
                audioProcessor.presetManager.getCurrentPresetName());
            starBtn.setToggleState(
                audioProcessor.presetManager.isFavourite(
                    audioProcessor.presetManager.getCurrentPresetName()),
                juce::dontSendNotification);
        };
    addAndMakeVisible(starBtn);

    // ── Shuffle (random preset) ───────────────────────────────────────────────
    shuffleBtn.onClick = [this]
        {
            audioProcessor.presetManager.loadRandomPreset();
            updatePresetDisplay();
            starBtn.setToggleState(
                audioProcessor.presetManager.isFavourite(
                    audioProcessor.presetManager.getCurrentPresetName()),
                juce::dontSendNotification);
        };
    addAndMakeVisible(shuffleBtn);

    // ── Dice (randomize all parameters) ──────────────────────────────────────
    diceBtn.onClick = [this] { audioProcessor.randomizeParameters(); };
    addAndMakeVisible(diceBtn);

    // ── Loss ──────────────────────────────────────────────────────────────────
    lossModeCombo.addItemList({ "Standard", "Inverse", "Phase Jitter",
                                 "Packet Repeat", "Packet Loss",
                                 "Std + Packet Loss", "Std + Packet Repeat",
                                 "Packet Disorder", "Disorder + Standard" }, 1);
    setupCombo(lossModeCombo, lossModeLabel, "MODE");

    codecModeCombo.addItemList({ "Music", "Voice", "Broadcast" }, 1);
    setupCombo(codecModeCombo, codecModeLabel, "CODEC");

    setupRotary(lossAmountSlider, lossAmountLabel, "AMOUNT");
    setupRotary(lossSpeedSlider, lossSpeedLabel, "SPEED");
    setupRotary(lossGainSlider, lossGainLabel, "GAIN");

    lossModeAttach = std::make_unique<CA>(apvts, "lossMode", lossModeCombo);
    codecModeAttach = std::make_unique<CA>(apvts, "codecMode", codecModeCombo);
    lossAmountAttach = std::make_unique<SA>(apvts, "lossAmount", lossAmountSlider);
    lossSpeedAttach = std::make_unique<SA>(apvts, "lossSpeed", lossSpeedSlider);
    lossGainAttach = std::make_unique<SA>(apvts, "lossGain", lossGainSlider);

    // ── Noise ─────────────────────────────────────────────────────────────────
    noiseColorCombo.addItemList({ "White", "Pink", "Brown" }, 1);
    setupCombo(noiseColorCombo, noiseColorLabel, "COLOR");
    setupRotary(noiseAmountSlider, noiseAmountLabel, "AMOUNT");
    setupRotary(noiseBiasSlider, noiseBiasLabel, "BIAS");
    addAndMakeVisible(noiseEnabledBtn);

    noiseEnabledAttach = std::make_unique<BA>(apvts, "noiseEnabled", noiseEnabledBtn);
    noiseColorAttach = std::make_unique<CA>(apvts, "noiseColor", noiseColorCombo);
    noiseAmountAttach = std::make_unique<SA>(apvts, "noiseAmount", noiseAmountSlider);
    noiseBiasAttach = std::make_unique<SA>(apvts, "noiseBias", noiseBiasSlider);

    // ── Filter ────────────────────────────────────────────────────────────────
    filterModeCombo.addItemList({ "Normal", "Inverted" }, 1);
    setupCombo(filterModeCombo, filterModeLabel, "MODE");

    filterSlopeCombo.addItemList({ "12 dB/oct", "24 dB/oct",
                                    "36 dB/oct", "48 dB/oct" }, 1);
    setupCombo(filterSlopeCombo, filterSlopeLabel, "SLOPE");

    setupRotary(lowCutSlider, lowCutLabel, "LOW CUT");
    setupRotary(highCutSlider, highCutLabel, "HIGH CUT");

    filterModeAttach = std::make_unique<CA>(apvts, "filterMode", filterModeCombo);
    filterSlopeAttach = std::make_unique<CA>(apvts, "filterSlope", filterSlopeCombo);
    lowCutAttach = std::make_unique<SA>(apvts, "lowCutFreq", lowCutSlider);
    highCutAttach = std::make_unique<SA>(apvts, "highCutFreq", highCutSlider);

    // ── Verb ──────────────────────────────────────────────────────────────────
    verbPositionCombo.addItemList({ "Pre Loss", "Post Loss" }, 1);
    setupCombo(verbPositionCombo, verbPositionLabel, "POSITION");
    setupRotary(verbAmountSlider, verbAmountLabel, "AMOUNT");
    setupRotary(verbDecaySlider, verbDecayLabel, "DECAY");

    verbPositionAttach = std::make_unique<CA>(apvts, "verbPosition", verbPositionCombo);
    verbAmountAttach = std::make_unique<SA>(apvts, "verbAmount", verbAmountSlider);
    verbDecayAttach = std::make_unique<SA>(apvts, "verbDecay", verbDecaySlider);

    // ── Master ────────────────────────────────────────────────────────────────
    setupRotary(magnitudeSlider, magnitudeLabel, "MAGNITUDE");
    setupRotary(masterMixSlider, masterMixLabel, "MIX");
    setupRotary(preGainSlider, preGainLabel, "PRE GAIN");
    setupRotary(postGainSlider, postGainLabel, "POST GAIN");
    addAndMakeVisible(masterBypassBtn);

    magnitudeAttach = std::make_unique<SA>(apvts, "magnitude", magnitudeSlider);
    masterMixAttach = std::make_unique<SA>(apvts, "masterMix", masterMixSlider);
    preGainAttach = std::make_unique<SA>(apvts, "preprocessGain", preGainSlider);
    postGainAttach = std::make_unique<SA>(apvts, "postprocessGain", postGainSlider);
    masterBypassAttach = std::make_unique<BA>(apvts, "masterBypass", masterBypassBtn);

    // ── Advanced ──────────────────────────────────────────────────────────────
    stereoModeCombo.addItemList({ "Stereo", "Joint Stereo", "Mono" }, 1);
    setupCombo(stereoModeCombo, stereoModeLabel, "STEREO MODE");

    weightingCombo.addItemList({ "Perceptual", "Flat" }, 1);
    setupCombo(weightingCombo, weightingLabel, "WEIGHTING");

    setupRotary(autoGainSlider, autoGainLabel, "AUTO GAIN");
    setupRotary(gateThreshSlider, gateThreshLabel, "GATE");

    stereoModeAttach = std::make_unique<CA>(apvts, "stereoMode", stereoModeCombo);
    weightingAttach = std::make_unique<CA>(apvts, "weighting", weightingCombo);
    autoGainAttach = std::make_unique<SA>(apvts, "autoGain", autoGainSlider);
    gateThreshAttach = std::make_unique<SA>(apvts, "gateThreshold", gateThreshSlider);

    // ── Spectrum ──────────────────────────────────────────────────────────────
    addAndMakeVisible(spectrumAnalyser);
}

//==============================================================================
ArtifactAudioProcessorEditor::~ArtifactAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void ArtifactAudioProcessorEditor::setupRotary(juce::Slider& s, juce::Label& l,
    const juce::String& text)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 14);
    s.setColour(juce::Slider::textBoxTextColourId,
        juce::Colour(ArtifactLookAndFeel::colTextDim));
    s.setColour(juce::Slider::textBoxBackgroundColourId,
        juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId,
        juce::Colours::transparentBlack);
    addAndMakeVisible(s);

    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId,
        juce::Colour(ArtifactLookAndFeel::colTextDim));
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)));
    addAndMakeVisible(l);
}

//==============================================================================
void ArtifactAudioProcessorEditor::setupCombo(juce::ComboBox& cb, juce::Label& l,
    const juce::String& text)
{
    addAndMakeVisible(cb);
    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centredLeft);
    l.setColour(juce::Label::textColourId,
        juce::Colour(ArtifactLookAndFeel::colTextDim));
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)));
    addAndMakeVisible(l);
}

//==============================================================================
void ArtifactAudioProcessorEditor::updatePresetDisplay()
{
    presetNameLabel.setText(audioProcessor.presetManager.getCurrentPresetName(),
        juce::dontSendNotification);
    starBtn.setToggleState(
        audioProcessor.presetManager.isFavourite(
            audioProcessor.presetManager.getCurrentPresetName()),
        juce::dontSendNotification);
}

//==============================================================================
void ArtifactAudioProcessorEditor::drawSection(juce::Graphics& g,
    juce::Rectangle<int> b,
    const juce::String& title) const
{
    g.setColour(juce::Colour(ArtifactLookAndFeel::colPanel));
    g.fillRoundedRectangle(b.toFloat(), 4.0f);

    g.setColour(juce::Colour(ArtifactLookAndFeel::colPanelBorder));
    g.drawRoundedRectangle(b.toFloat().reduced(0.5f), 4.0f, 1.0f);

    const auto titleBar = b.removeFromTop(22).reduced(1, 0);
    g.setColour(juce::Colour(ArtifactLookAndFeel::colPanelBorder).withAlpha(0.5f));
    g.fillRect(titleBar);

    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)).boldened());
    g.setColour(juce::Colour(ArtifactLookAndFeel::colAccent));
    g.drawText(title, titleBar.getX() + 8, titleBar.getY(),
        titleBar.getWidth() - 8, titleBar.getHeight(),
        juce::Justification::centredLeft);
}

//==============================================================================
void ArtifactAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(ArtifactLookAndFeel::colBackground));

    // Subtle grid texture
    g.setColour(juce::Colour(0xff131218));
    for (int x = 0; x < getWidth(); x += 20) g.drawVerticalLine(x, 0, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 20) g.drawHorizontalLine(y, 0, (float)getWidth());

    // Preset bar separator
    g.setColour(juce::Colour(ArtifactLookAndFeel::colPanelBorder));
    g.drawHorizontalLine(44, 0.0f, (float)getWidth());

    const int pad = 8;
    const int gap = 6;
    const int barH = 44;
    const int colW = (getWidth() - 2 * pad - 2 * gap) / 3;
    const int row1H = 226;
    const int col1X = pad;
    const int col2X = pad + colW + gap;
    const int col3X = pad + 2 * (colW + gap);
    const int col3W = getWidth() - col3X - pad;
    const int row1Y = barH + pad;
    const int row2Y = row1Y + row1H + 62;
    const int row2H = getHeight() - row2Y - pad;

    drawSection(g, { col1X, row1Y, colW,  row1H }, "LOSS");
    drawSection(g, { col2X, row1Y, colW,  row1H }, "NOISE");
    drawSection(g, { col3X, row1Y, col3W, row1H }, "MASTER");
    drawSection(g, { col1X, row2Y, colW,  row2H }, "FILTER");
    drawSection(g, { col2X, row2Y, colW,  row2H }, "REVERB");
    drawSection(g, { col3X, row2Y, col3W, row2H }, "ADVANCED");
}

//==============================================================================
void ArtifactAudioProcessorEditor::resized()
{
    const int pad = 8;
    const int gap = 6;
    const int barH = 44;
    const int colW = (getWidth() - 2 * pad - 2 * gap) / 3;
    const int row1H = 226;
    const int col1X = pad;
    const int col2X = pad + colW + gap;
    const int col3X = pad + 2 * (colW + gap);
    const int col3W = getWidth() - col3X - pad;
    const int row1Y = barH + pad;
    const int row2Y = row1Y + row1H + 62;
    const int row2H = getHeight() - row2Y - pad;

    // ── Preset bar ────────────────────────────────────────────────────────────
    pluginNameLabel.setBounds(10, 0, 100, barH);

    const int navW = 220;
    const int navX = (getWidth() - navW) / 2;
    presetPrevBtn.setBounds(navX, 8, 26, 28);
    presetNameLabel.setBounds(navX + 30, 8, navW - 60, 28);
    presetNextBtn.setBounds(navX + navW - 26, 8, 26, 28);

    // Right side: SAVE | INIT | ★ | ⇄ | ⚄
    const int rightEdge = getWidth() - 8;
    presetSaveBtn.setBounds(rightEdge - 50, 10, 46, 24);
    presetInitBtn.setBounds(rightEdge - 50 - 42, 10, 38, 24);
    starBtn.setBounds(rightEdge - 50 - 42 - 30, 10, 26, 24);
    shuffleBtn.setBounds(rightEdge - 50 - 42 - 30 - 28, 10, 26, 24);
    diceBtn.setBounds(rightEdge - 50 - 42 - 30 - 56, 10, 26, 24);

    // ── Spectrum analyser ─────────────────────────────────────────────────────
    spectrumAnalyser.setBounds(pad, row1Y + row1H + 2,
        getWidth() - 2 * pad, 54);

    // ── Knob helper ───────────────────────────────────────────────────────────
    const int knobH = 52;
    const int labelH = 12;
    const int knobBlockH = knobH + 4 + labelH;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l,
        int cellX, int cellY, int cellW)
        {
            const int kx = cellX + (cellW - knobH) / 2;
            s.setBounds(kx, cellY, knobH, knobH);
            l.setBounds(kx - 4, cellY + knobH + 2, knobH + 8, labelH);
        };

    // ── LOSS ──────────────────────────────────────────────────────────────────
    {
        const int sx = col1X + pad;
        const int sw = colW - 2 * pad;
        int y = row1Y + 26;

        lossModeLabel.setBounds(sx, y, sw, 12);
        y += 12;
        lossModeCombo.setBounds(sx, y, sw, 22);
        y += 28;

        const int cellW = sw / 3;
        placeKnob(lossAmountSlider, lossAmountLabel, sx, y, cellW);
        placeKnob(lossSpeedSlider, lossSpeedLabel, sx + cellW, y, cellW);
        placeKnob(lossGainSlider, lossGainLabel, sx + 2 * cellW, y, cellW);
        y += knobBlockH + 10;

        codecModeLabel.setBounds(sx, y, sw, 12);
        y += 12;
        codecModeCombo.setBounds(sx, y, sw, 22);
    }

    // ── NOISE ─────────────────────────────────────────────────────────────────
    {
        const int sx = col2X + pad;
        const int sw = colW - 2 * pad;
        int y = row1Y + 26;

        noiseEnabledBtn.setBounds(sx, y, 80, 22);
        noiseColorLabel.setBounds(sx + 86, y - 12, sw - 86, 12);
        noiseColorCombo.setBounds(sx + 86, y, sw - 86, 22);
        y += 34;

        const int cellW = sw / 2;
        placeKnob(noiseAmountSlider, noiseAmountLabel, sx, y, cellW);
        placeKnob(noiseBiasSlider, noiseBiasLabel, sx + cellW, y, cellW);
    }

    // ── MASTER ────────────────────────────────────────────────────────────────
    {
        const int sx = col3X + pad;
        const int sw = col3W - 2 * pad;
        int y = row1Y + 26;

        const int cellW = sw / 2;
        placeKnob(magnitudeSlider, magnitudeLabel, sx, y, cellW);
        placeKnob(masterMixSlider, masterMixLabel, sx + cellW, y, cellW);
        y += knobBlockH + 10;

        placeKnob(preGainSlider, preGainLabel, sx, y, cellW);
        placeKnob(postGainSlider, postGainLabel, sx + cellW, y, cellW);
        y += knobBlockH + 10;

        masterBypassBtn.setBounds(sx, y, sw, 22);
    }

    // ── FILTER ────────────────────────────────────────────────────────────────
    {
        const int sx = col1X + pad;
        const int sw = colW - 2 * pad;
        int y = row2Y + 26;

        const int comboW = (sw - 6) / 2;
        filterModeLabel.setBounds(sx, y, comboW, 12);
        filterSlopeLabel.setBounds(sx + comboW + 6, y, comboW, 12);
        y += 12;
        filterModeCombo.setBounds(sx, y, comboW, 22);
        filterSlopeCombo.setBounds(sx + comboW + 6, y, comboW, 22);
        y += 30;

        const int cellW = sw / 2;
        placeKnob(lowCutSlider, lowCutLabel, sx, y, cellW);
        placeKnob(highCutSlider, highCutLabel, sx + cellW, y, cellW);
    }

    // ── REVERB ────────────────────────────────────────────────────────────────
    {
        const int sx = col2X + pad;
        const int sw = colW - 2 * pad;
        int y = row2Y + 26;

        verbPositionLabel.setBounds(sx, y, sw, 12);
        y += 12;
        verbPositionCombo.setBounds(sx, y, sw, 22);
        y += 30;

        const int cellW = sw / 2;
        placeKnob(verbAmountSlider, verbAmountLabel, sx, y, cellW);
        placeKnob(verbDecaySlider, verbDecayLabel, sx + cellW, y, cellW);
    }

    // ── ADVANCED ──────────────────────────────────────────────────────────────
    {
        const int sx = col3X + pad;
        const int sw = col3W - 2 * pad;
        int y = row2Y + 26;

        const int comboW = (sw - 6) / 2;
        stereoModeLabel.setBounds(sx, y, comboW, 12);
        weightingLabel.setBounds(sx + comboW + 6, y, comboW, 12);
        y += 12;
        stereoModeCombo.setBounds(sx, y, comboW, 22);
        weightingCombo.setBounds(sx + comboW + 6, y, comboW, 22);
        y += 30;

        const int cellW = sw / 2;
        placeKnob(autoGainSlider, autoGainLabel, sx, y, cellW);
        placeKnob(gateThreshSlider, gateThreshLabel, sx + cellW, y, cellW);
    }
}