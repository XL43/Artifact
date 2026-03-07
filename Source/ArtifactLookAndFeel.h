#pragma once
#include <JuceHeader.h>

class ArtifactLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr uint32_t colBackground = 0xff101012;
    static constexpr uint32_t colPanel = 0xff1a1a1e;
    static constexpr uint32_t colPanelBorder = 0xff2c2c34;
    static constexpr uint32_t colAccent = 0xffff8c00;
    static constexpr uint32_t colAccentDim = 0xff6b3a00;
    static constexpr uint32_t colText = 0xffdddad4;
    static constexpr uint32_t colTextDim = 0xff666460;
    static constexpr uint32_t colKnobBg = 0xff222226;
    static constexpr uint32_t colKnobTrack = 0xff3a3a40;

    ArtifactLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(colBackground));
        setColour(juce::Label::textColourId, juce::Colour(colText));
        setColour(juce::Slider::thumbColourId, juce::Colour(colAccent));
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(colAccent));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(colKnobTrack));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(colPanel));
        setColour(juce::ComboBox::textColourId, juce::Colour(colText));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(colPanelBorder));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(colAccent));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(colPanel));
        setColour(juce::PopupMenu::textColourId, juce::Colour(colText));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(colAccentDim));
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(colText));
        setColour(juce::TextButton::buttonColourId, juce::Colour(colPanel));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(colAccentDim));
        setColour(juce::TextButton::textColourOffId, juce::Colour(colText));
        setColour(juce::TextButton::textColourOnId, juce::Colour(colAccent));
        setColour(juce::ToggleButton::textColourId, juce::Colour(colText));
        setColour(juce::ToggleButton::tickColourId, juce::Colour(colAccent));
        setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(colTextDim));
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float startAngle, float endAngle,
        juce::Slider&) override
    {
        const float radius = juce::jmin(width, height) * 0.5f - 4.0f;
        const float cx = x + width * 0.5f;
        const float cy = y + height * 0.5f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // Background circle
        g.setColour(juce::Colour(colKnobBg));
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Outer ring
        g.setColour(juce::Colour(colKnobTrack).withAlpha(0.6f));
        g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.2f);

        // Track arc
        {
            juce::Path track;
            track.addCentredArc(cx, cy, radius - 4.0f, radius - 4.0f,
                0.0f, startAngle, endAngle, true);
            g.setColour(juce::Colour(colKnobTrack));
            g.strokePath(track, juce::PathStrokeType(2.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Value arc
        {
            juce::Path valueArc;
            valueArc.addCentredArc(cx, cy, radius - 4.0f, radius - 4.0f,
                0.0f, startAngle, angle, true);
            g.setColour(juce::Colour(colAccent));
            g.strokePath(valueArc, juce::PathStrokeType(2.5f,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Pointer
        {
            const float pLen = radius * 0.5f;
            const float pW = 1.8f;
            juce::Path ptr;
            ptr.addRectangle(-pW * 0.5f, -(radius * 0.82f), pW, pLen);
            ptr.applyTransform(juce::AffineTransform::rotation(angle)
                .translated(cx, cy));
            g.setColour(juce::Colour(colText));
            g.fillPath(ptr);
        }

        // Centre dot
        g.setColour(juce::Colour(colAccent));
        g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool,
        int, int, int, int, juce::ComboBox&) override
    {
        const juce::Rectangle<float> b(0.0f, 0.0f, (float)width, (float)height);
        g.setColour(juce::Colour(colPanel));
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(colPanelBorder));
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

        // Arrow
        const float ax = width - 13.0f;
        const float ay = height * 0.5f;
        juce::Path arrow;
        arrow.addTriangle(ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
        g.setColour(juce::Colour(colAccent));
        g.fillPath(arrow);
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override
    {
        return juce::Font(juce::FontOptions{}.withHeight(11.5f));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& btn,
        const juce::Colour&, bool highlighted, bool down) override
    {
        const auto b = btn.getLocalBounds().toFloat().reduced(0.5f);
        juce::Colour base = juce::Colour(btn.getToggleState() ? colAccentDim : colPanel);
        if (highlighted) base = base.brighter(0.08f);
        if (down)        base = base.darker(0.1f);
        g.setColour(base);
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(juce::Colour(btn.getToggleState() ? colAccent : colPanelBorder));
        g.drawRoundedRectangle(b, 3.0f, 1.0f);
    }

    // Toggle button drawn as LED + label
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
        bool highlighted, bool) override
    {
        const bool on = btn.getToggleState();
        const float ledR = 5.0f;
        const float ledX = (float)btn.getX() - btn.getX() + 5.0f;
        const float ledY = btn.getHeight() * 0.5f;

        // LED glow
        if (on)
        {
            g.setColour(juce::Colour(colAccent).withAlpha(0.25f));
            g.fillEllipse(ledX - ledR * 1.6f, ledY - ledR * 1.6f,
                ledR * 3.2f, ledR * 3.2f);
        }

        // LED
        g.setColour(on ? juce::Colour(colAccent) : juce::Colour(colKnobTrack));
        g.fillEllipse(ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f);
        g.setColour(juce::Colour(colPanelBorder));
        g.drawEllipse(ledX - ledR, ledY - ledR, ledR * 2.0f, ledR * 2.0f, 1.0f);

        // Text
        g.setColour(on ? juce::Colour(colText) : juce::Colour(colTextDim));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        g.drawText(btn.getButtonText(),
            (int)(ledX + ledR + 6), 0,
            btn.getWidth() - (int)(ledX + ledR + 6), btn.getHeight(),
            juce::Justification::centredLeft);
    }

    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(juce::FontOptions{}.withHeight(10.5f));
    }
};