#include "PluginEditor.h"

//==============================================================================
ArtifactAudioProcessorEditor::ArtifactAudioProcessorEditor(ArtifactAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
    spectrumAnalyser(p.spectrumAnalyser)
{
    setLookAndFeel(&laf);
    setSize(860, 610);
    setResizable(true, false);
    setResizeLimits(minW, minH, maxW, maxH);
    addAndMakeVisible(resizeHandle);

    auto& apvts = audioProcessor.apvts;

    // ── Preset bar ────────────────────────────────────────────────────────────
    pluginNameLabel.setText("ARTIFACT", juce::dontSendNotification);
    pluginNameLabel.setFont(juce::Font(juce::FontOptions{}.withHeight(18.0f)).boldened());
    pluginNameLabel.setColour(juce::Label::textColourId,
        juce::Colour(ArtifactLookAndFeel::colAccent));
    addAndMakeVisible(pluginNameLabel);

    presetNameBtn.setButtonText(audioProcessor.presetManager.getCurrentPresetName());
    presetNameBtn.setTooltip("Click to browse presets");
    presetNameBtn.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    presetNameBtn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    presetNameBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    presetNameBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(ArtifactLookAndFeel::colText));
    presetNameBtn.setColour(juce::TextButton::textColourOnId, juce::Colour(ArtifactLookAndFeel::colText));
    presetNameBtn.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible(presetNameBtn);

    presetPrevBtn.setTooltip("Load previous preset");
    presetPrevBtn.onClick = [this]
        {
            audioProcessor.presetManager.loadPreviousPreset();
            updatePresetDisplay();
        };
    addAndMakeVisible(presetPrevBtn);

    presetNextBtn.setTooltip("Load next preset");
    presetNextBtn.onClick = [this]
        {
            audioProcessor.presetManager.loadNextPreset();
            updatePresetDisplay();
        };
    addAndMakeVisible(presetNextBtn);

    presetSaveBtn.setTooltip("Save current settings as a new preset");
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

    presetInitBtn.setTooltip("Reset all parameters to default values");
    presetInitBtn.onClick = [this]
        {
            audioProcessor.initializeParameters();
            presetNameBtn.setButtonText("Default");
        };
    addAndMakeVisible(presetInitBtn);

    starBtn.setTooltip("Toggle favourite for current preset");
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

    shuffleBtn.setTooltip("Load a random preset");
    shuffleBtn.onClick = [this]
        {
            audioProcessor.presetManager.loadRandomPreset();
            updatePresetDisplay();
        };
    addAndMakeVisible(shuffleBtn);

    diceBtn.setTooltip("Randomize all parameters");
    diceBtn.onClick = [this] { audioProcessor.randomizeParameters(); };
    addAndMakeVisible(diceBtn);

    // ── Loss ──────────────────────────────────────────────────────────────────
    lossModeCombo.clear(juce::dontSendNotification);
    lossModeCombo.addItem("Standard", 1);
    lossModeCombo.addItem("Inverse", 2);
    lossModeCombo.addItem("Phase Jitter", 3);
    lossModeCombo.addItem("Packet Repeat", 4);
    lossModeCombo.addItem("Packet Loss", 5);
    lossModeCombo.addItem("Std + Packet Loss", 6);
    lossModeCombo.addItem("Std + Packet Repeat", 7);
    lossModeCombo.addItem("Packet Disorder", 8);
    lossModeCombo.addItem("Disorder + Standard", 9);
    setupCombo(lossModeCombo, lossModeLabel, "MODE",
        "Loss algorithm - sets how the spectral degradation is applied");

    codecModeCombo.clear(juce::dontSendNotification);
    codecModeCombo.addItem("Music", 1);
    codecModeCombo.addItem("Voice", 2);
    codecModeCombo.addItem("Broadcast", 3);
    setupCombo(codecModeCombo, codecModeLabel, "CODEC",
        "Codec mode - shapes which frequency regions degrade first");

    setupRotary(lossAmountSlider, lossAmountLabel, "AMOUNT",
        "Loss Amount - how many spectral bins are affected per frame");
    setupRotary(lossSpeedSlider, lossSpeedLabel, "SPEED",
        "Loss Speed - controls FFT hop size, affecting smear character");
    setupRotary(lossGainSlider, lossGainLabel, "GAIN",
        "Loss Gain - output gain applied after the Loss engine");

    lossModeAttach = std::make_unique<CA>(apvts, "lossMode", lossModeCombo);
    codecModeAttach = std::make_unique<CA>(apvts, "codecMode", codecModeCombo);
    lossAmountAttach = std::make_unique<SA>(apvts, "lossAmount", lossAmountSlider);
    lossSpeedAttach = std::make_unique<SA>(apvts, "lossSpeed", lossSpeedSlider);
    lossGainAttach = std::make_unique<SA>(apvts, "lossGain", lossGainSlider);

    // ── Noise ─────────────────────────────────────────────────────────────────
    noiseColorCombo.clear(juce::dontSendNotification);
    noiseColorCombo.addItem("White", 1);
    noiseColorCombo.addItem("Pink", 2);
    noiseColorCombo.addItem("Brown", 3);
    noiseColorCombo.setTooltip("Noise color - White is bright, Pink is balanced, Brown is deep");
    addAndMakeVisible(noiseColorCombo);

    setupRotary(noiseAmountSlider, noiseAmountLabel, "AMOUNT",
        "Noise Amount - level of noise injected before the Loss engine");
    setupRotary(noiseBiasSlider, noiseBiasLabel, "BIAS",
        "Noise Bias - negative tilts noise toward lows, positive toward highs");

    noiseEnabledBtn.setTooltip("Enable pre-encoding noise injection");
    addAndMakeVisible(noiseEnabledBtn);

    noiseEnabledAttach = std::make_unique<BA>(apvts, "noiseEnabled", noiseEnabledBtn);
    noiseColorAttach = std::make_unique<CA>(apvts, "noiseColor", noiseColorCombo);
    noiseAmountAttach = std::make_unique<SA>(apvts, "noiseAmount", noiseAmountSlider);
    noiseBiasAttach = std::make_unique<SA>(apvts, "noiseBias", noiseBiasSlider);

    // ── Filter ────────────────────────────────────────────────────────────────
    filterModeCombo.clear(juce::dontSendNotification);
    filterModeCombo.addItem("Normal", 1);
    filterModeCombo.addItem("Inverted", 2);
    setupCombo(filterModeCombo, filterModeLabel, "MODE",
        "Filter Mode - Normal is bandpass, Inverted is bandreject");

    filterSlopeCombo.clear(juce::dontSendNotification);
    filterSlopeCombo.addItem("12 dB/oct", 1);
    filterSlopeCombo.addItem("24 dB/oct", 2);
    filterSlopeCombo.addItem("36 dB/oct", 3);
    filterSlopeCombo.addItem("48 dB/oct", 4);
    setupCombo(filterSlopeCombo, filterSlopeLabel, "SLOPE",
        "Filter Slope - steeper slopes give a harder frequency cutoff");

    setupRotary(lowCutSlider, lowCutLabel, "LOW CUT",
        "Low Cut Frequency - high-pass cutoff before the Loss engine");
    setupRotary(highCutSlider, highCutLabel, "HIGH CUT",
        "High Cut Frequency - low-pass cutoff before the Loss engine");

    filterModeAttach = std::make_unique<CA>(apvts, "filterMode", filterModeCombo);
    filterSlopeAttach = std::make_unique<CA>(apvts, "filterSlope", filterSlopeCombo);
    lowCutAttach = std::make_unique<SA>(apvts, "lowCutFreq", lowCutSlider);
    highCutAttach = std::make_unique<SA>(apvts, "highCutFreq", highCutSlider);

    // ── Verb ──────────────────────────────────────────────────────────────────
    verbPositionCombo.clear(juce::dontSendNotification);
    verbPositionCombo.addItem("Pre Loss", 1);
    verbPositionCombo.addItem("Post Loss", 2);
    setupCombo(verbPositionCombo, verbPositionLabel, "POSITION",
        "Reverb Position - Pre sends reverb into the Loss engine for extra destruction");

    setupRotary(verbAmountSlider, verbAmountLabel, "AMOUNT",
        "Reverb Amount - wet level of the Schroeder reverb");
    setupRotary(verbDecaySlider, verbDecayLabel, "DECAY",
        "Reverb Decay - controls comb filter feedback length");

    verbPositionAttach = std::make_unique<CA>(apvts, "verbPosition", verbPositionCombo);
    verbAmountAttach = std::make_unique<SA>(apvts, "verbAmount", verbAmountSlider);
    verbDecayAttach = std::make_unique<SA>(apvts, "verbDecay", verbDecaySlider);

    // ── Master ────────────────────────────────────────────────────────────────
    setupRotary(magnitudeSlider, magnitudeLabel, "MAGNITUDE",
        "Magnitude - master intensity that scales Loss, Noise, Verb, and Filter simultaneously");
    setupRotary(masterMixSlider, masterMixLabel, "MIX",
        "Master Mix - dry/wet blend of the entire plugin signal chain");
    setupRotary(preGainSlider, preGainLabel, "PRE GAIN",
        "Pre Gain - input gain applied before all processing");
    setupRotary(postGainSlider, postGainLabel, "POST GAIN",
        "Post Gain - output gain applied after all processing");

    masterBypassBtn.setTooltip("Bypass all processing");
    addAndMakeVisible(masterBypassBtn);

    magnitudeAttach = std::make_unique<SA>(apvts, "magnitude", magnitudeSlider);
    masterMixAttach = std::make_unique<SA>(apvts, "masterMix", masterMixSlider);
    preGainAttach = std::make_unique<SA>(apvts, "preprocessGain", preGainSlider);
    postGainAttach = std::make_unique<SA>(apvts, "postprocessGain", postGainSlider);
    masterBypassAttach = std::make_unique<BA>(apvts, "masterBypass", masterBypassBtn);

    // ── Advanced ──────────────────────────────────────────────────────────────
    stereoModeCombo.clear(juce::dontSendNotification);
    stereoModeCombo.addItem("Stereo", 1);
    stereoModeCombo.addItem("Joint Stereo", 2);
    stereoModeCombo.addItem("Mono", 3);
    setupCombo(stereoModeCombo, stereoModeLabel, "STEREO MODE",
        "Stereo Mode - Stereo is independent L/R, Joint applies Loss to mid only, Mono sums to centre");

    weightingCombo.clear(juce::dontSendNotification);
    weightingCombo.addItem("Perceptual", 1);
    weightingCombo.addItem("Flat", 2);
    setupCombo(weightingCombo, weightingLabel, "WEIGHTING",
        "Weighting - Perceptual applies pre/de-emphasis to preserve highs, Flat is uniform");

    setupRotary(autoGainSlider, autoGainLabel, "AUTO GAIN",
        "Auto Gain - compensates output level after Loss processing");
    setupRotary(gateThreshSlider, gateThreshLabel, "GATE",
        "Gate Threshold - silences output below this dBFS level");

    stereoModeAttach = std::make_unique<CA>(apvts, "stereoMode", stereoModeCombo);
    weightingAttach = std::make_unique<CA>(apvts, "weighting", weightingCombo);
    autoGainAttach = std::make_unique<SA>(apvts, "autoGain", autoGainSlider);
    gateThreshAttach = std::make_unique<SA>(apvts, "gateThreshold", gateThreshSlider);

    // ── Spectrum ──────────────────────────────────────────────────────────────
    addAndMakeVisible(spectrumAnalyser);

    // ── Value formatting ──────────────────────────────────────────────────────
    setupValueFormatting();
}

//==============================================================================
ArtifactAudioProcessorEditor::~ArtifactAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

//==============================================================================
void ArtifactAudioProcessorEditor::setupValueFormatting()
{
    // Frequency knobs — show Hz / kHz
    auto freqFmt = [](double v) -> juce::String
        {
            if (v >= 1000.0)
                return juce::String(v / 1000.0, 2) + " kHz";
            return juce::String((int)v) + " Hz";
        };
    lowCutSlider.textFromValueFunction = freqFmt;
    highCutSlider.textFromValueFunction = freqFmt;

    // Percentage knobs
    auto pctFmt = [](double v) -> juce::String
        {
            return juce::String((int)v) + " %";
        };
    lossAmountSlider.textFromValueFunction = pctFmt;
    lossSpeedSlider.textFromValueFunction = pctFmt;
    noiseAmountSlider.textFromValueFunction = pctFmt;
    magnitudeSlider.textFromValueFunction = pctFmt;
    masterMixSlider.textFromValueFunction = pctFmt;
    verbAmountSlider.textFromValueFunction = pctFmt;
    verbDecaySlider.textFromValueFunction = pctFmt;
    autoGainSlider.textFromValueFunction = pctFmt;

    // dB knobs
    auto dbFmt = [](double v) -> juce::String
        {
            return juce::String(v, 1) + " dB";
        };
    lossGainSlider.textFromValueFunction = dbFmt;
    preGainSlider.textFromValueFunction = dbFmt;
    postGainSlider.textFromValueFunction = dbFmt;
    gateThreshSlider.textFromValueFunction = dbFmt;

    // Noise bias — signed integer
    noiseBiasSlider.textFromValueFunction = [](double v) -> juce::String
        {
            return (v >= 0 ? "+" : "") + juce::String((int)v);
        };

    // Refresh all text boxes
    for (auto* s : { &lossAmountSlider, &lossSpeedSlider,  &lossGainSlider,
                     &noiseAmountSlider, &noiseBiasSlider,
                     &lowCutSlider,      &highCutSlider,
                     &verbAmountSlider,  &verbDecaySlider,
                     &magnitudeSlider,   &masterMixSlider,
                     &preGainSlider,     &postGainSlider,
                     &autoGainSlider,    &gateThreshSlider })
        s->updateText();
}

//==============================================================================
void ArtifactAudioProcessorEditor::setupRotary(juce::Slider& s, juce::Label& l,
    const juce::String& text,
    const juce::String& tooltip)
{
    s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 14);
    s.setColour(juce::Slider::textBoxTextColourId,
        juce::Colour(ArtifactLookAndFeel::colTextDim));
    s.setColour(juce::Slider::textBoxBackgroundColourId,
        juce::Colours::transparentBlack);
    s.setColour(juce::Slider::textBoxOutlineColourId,
        juce::Colours::transparentBlack);
    s.setTooltip(tooltip);
    addAndMakeVisible(s);

    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId,
        juce::Colour(ArtifactLookAndFeel::colTextDim));
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)));
    l.setTooltip(tooltip);
    addAndMakeVisible(l);
}

//==============================================================================
void ArtifactAudioProcessorEditor::setupCombo(juce::ComboBox& cb, juce::Label& l,
    const juce::String& text,
    const juce::String& tooltip)
{
    cb.setTooltip(tooltip);
    addAndMakeVisible(cb);

    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centredLeft);
    l.setColour(juce::Label::textColourId,
        juce::Colour(ArtifactLookAndFeel::colTextDim));
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f)));
    l.setTooltip(tooltip);
    addAndMakeVisible(l);
}

//==============================================================================
void ArtifactAudioProcessorEditor::updatePresetDisplay()
{
    presetNameBtn.setButtonText(audioProcessor.presetManager.getCurrentPresetName());
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

    g.setColour(juce::Colour(0xff131218));
    for (int x = 0; x < getWidth(); x += 20)
        g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 20)
        g.drawHorizontalLine(y, 0.0f, (float)getWidth());

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

    // ── Resize handle ─────────────────────────────────────────────────────────
    resizeHandle.setBounds(getWidth() - 16, getHeight() - 16, 16, 16);

    // ── Preset bar ────────────────────────────────────────────────────────────
    pluginNameLabel.setBounds(10, 0, 100, barH);

    const int navW = 220;
    const int navX = (getWidth() - navW) / 2;
    presetPrevBtn.setBounds(navX, 8, 26, 28);
    presetNameBtn.setBounds(navX + 30, 8, navW - 60, 28);
    presetNextBtn.setBounds(navX + navW - 26, 8, 26, 28);

    const int rightEdge = getWidth() - 8;
    presetSaveBtn.setBounds(rightEdge - 54, 10, 50, 24);
    presetInitBtn.setBounds(rightEdge - 54 - 46, 10, 42, 24);
    starBtn.setBounds(rightEdge - 54 - 46 - 46, 10, 42, 24);
    shuffleBtn.setBounds(rightEdge - 54 - 46 - 92, 10, 42, 24);
    diceBtn.setBounds(rightEdge - 54 - 46 - 138, 10, 42, 24);

    // ── Spectrum ──────────────────────────────────────────────────────────────
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

        // Enabled toggle and color combo on same row, full width split
        const int toggleW = 80;
        noiseEnabledBtn.setBounds(sx, y, toggleW, 22);
        noiseColorCombo.setBounds(sx + toggleW + 6, y, sw - toggleW - 6, 22);
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

//==============================================================================
void ArtifactAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);

    const auto allPresets = audioProcessor.presetManager.getUserPresets();
    const auto favNames = audioProcessor.presetManager.getFavouriteNames();
    const auto currentName = audioProcessor.presetManager.getCurrentPresetName();

    // ── Favourites submenu ────────────────────────────────────────────────────
    if (favNames.size() > 0)
    {
        juce::PopupMenu favMenu;
        favMenu.setLookAndFeel(&laf);

        int favId = 10000;
        for (const auto& name : favNames)
            favMenu.addItem(favId++, name, true, name == currentName);

        menu.addSubMenu("Favourites", favMenu);
        menu.addSeparator();
    }

    // ── Initialize ────────────────────────────────────────────────────────────
    menu.addItem(9999, "-- Initialize --");
    menu.addSeparator();

    // ── All presets ───────────────────────────────────────────────────────────
    int id = 1;
    for (const auto& file : allPresets)
    {
        const juce::String name = file.getFileNameWithoutExtension()
            .replaceCharacter('_', ' ');
        menu.addItem(id++, name, true, name == currentName);
    }

    juce::PopupMenu::Options options;
    options = options.withTargetComponent(&presetNameBtn);

    menu.showMenuAsync(options,
        [this, allPresets, favNames](int result)
        {
            if (result == 0) return;

            if (result == 9999)
            {
                audioProcessor.initializeParameters();
                presetNameBtn.setButtonText("Default");
                return;
            }

            if (result >= 10000)
            {
                const int favIdx = result - 10000;
                if (favIdx < favNames.size())
                {
                    audioProcessor.presetManager.loadPresetByName(favNames[favIdx]);
                    updatePresetDisplay();
                }
                return;
            }

            const int presetIdx = result - 1;
            if (presetIdx < allPresets.size())
            {
                audioProcessor.presetManager.loadPreset(allPresets[presetIdx]);
                updatePresetDisplay();
            }
        });
}