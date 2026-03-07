#include "SpectrumAnalyser.h"

//==============================================================================
SpectrumAnalyser::SpectrumAnalyser()
{
    preFifoBuffer.fill(0.0f);
    postFifoBuffer.fill(0.0f);
    fftWorkBuffer.fill(0.0f);
    preMagnitudes.fill(0.0f);
    postMagnitudes.fill(0.0f);
    preAccum.fill(0.0f);
    postAccum.fill(0.0f);

    startTimerHz(30);
}

//==============================================================================
void SpectrumAnalyser::pushSamples(const float* data, int numSamples, bool isPreLoss)
{
    auto& fifo = isPreLoss ? preFifo : postFifo;
    auto& fifoBuffer = isPreLoss ? preFifoBuffer : postFifoBuffer;

    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    if (size1 > 0) std::copy(data, data + size1, fifoBuffer.begin() + start1);
    if (size2 > 0) std::copy(data + size1, data + size1 + size2, fifoBuffer.begin() + start2);

    fifo.finishedWrite(size1 + size2);
}

//==============================================================================
void SpectrumAnalyser::processFifo(juce::AbstractFifo& fifo,
    std::array<float, fifoSize>& fifoBuffer,
    std::array<float, fftSize>& accum,
    int& accumPos,
    std::array<float, fftSize / 2>& magnitudes)
{
    const int available = fifo.getNumReady();
    if (available <= 0) return;

    int start1, size1, start2, size2;
    fifo.prepareToRead(available, start1, size1, start2, size2);

    // Helper to feed samples into accumulator and trigger FFT when full
    auto feedSamples = [&](int start, int size)
        {
            for (int i = 0; i < size; ++i)
            {
                accum[accumPos] = fifoBuffer[start + i];
                accumPos = (accumPos + 1) % fftSize;

                // When accumulator is full — run an FFT frame
                if (accumPos == 0)
                {
                    // Copy accum into work buffer and apply Hann window
                    std::copy(accum.begin(), accum.end(), fftWorkBuffer.begin());
                    std::fill(fftWorkBuffer.begin() + fftSize, fftWorkBuffer.end(), 0.0f);
                    window.multiplyWithWindowingTable(fftWorkBuffer.data(), fftSize);

                    fft.performRealOnlyForwardTransform(fftWorkBuffer.data(), true);

                    // Extract magnitude for each bin and smooth
                    const float normFactor = 1.0f / (float)fftSize;
                    for (int b = 0; b < fftSize / 2; ++b)
                    {
                        const float re = fftWorkBuffer[b * 2];
                        const float im = fftWorkBuffer[b * 2 + 1];
                        const float mag = std::sqrt(re * re + im * im) * normFactor;

                        // Convert to dB, map to 0.0–1.0 range
                        // -90dB → 0.0,  0dB → 1.0
                        const float dB = juce::Decibels::gainToDecibels(mag, -90.0f);
                        const float mapped = juce::jmap(dB, -90.0f, 0.0f, 0.0f, 1.0f);
                        const float clamped = juce::jlimit(0.0f, 1.0f, mapped);

                        // Exponential smoothing — fast attack, slow decay
                        if (clamped > magnitudes[b])
                            magnitudes[b] = clamped;                               // instant attack
                        else
                            magnitudes[b] = magnitudes[b] * smoothingCoeff         // smooth decay
                            + clamped * (1.0f - smoothingCoeff);
                    }
                }
            }
        };

    if (size1 > 0) feedSamples(start1, size1);
    if (size2 > 0) feedSamples(start2, size2);

    fifo.finishedRead(size1 + size2);
}

//==============================================================================
void SpectrumAnalyser::timerCallback()
{
    processFifo(preFifo, preFifoBuffer, preAccum, preAccumPos, preMagnitudes);
    processFifo(postFifo, postFifoBuffer, postAccum, postAccumPos, postMagnitudes);
    repaint();
}

//==============================================================================
void SpectrumAnalyser::drawSpectrumPath(juce::Graphics& g,
    const std::array<float, fftSize / 2>& magnitudes,
    juce::Colour colour, float alpha)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    juce::Path path;

    // Map FFT bins to pixel positions on a logarithmic frequency scale
    // Bin 0 = DC (skip), we start from a bin corresponding to ~20Hz
    // At 44.1kHz: bin = freq * fftSize / sampleRate
    // We use a log scale so low frequencies are not crushed to the left edge

    const int   numBins = fftSize / 2;
    const float logMin = std::log10(20.0f);
    const float logMax = std::log10(20000.0f);
    const float logRange = logMax - logMin;

    bool started = false;

    for (int b = 1; b < numBins; ++b)
    {
        // Approximate frequency of this bin (assuming 44.1kHz)
        const float binHz = (float)b * 44100.0f / (float)fftSize;
        if (binHz < 20.0f || binHz > 20000.0f) continue;

        const float logFreq = std::log10(binHz);
        const float xNorm = (logFreq - logMin) / logRange;
        const float x = xNorm * w;
        const float y = h - magnitudes[b] * h;

        if (!started) { path.startNewSubPath(x, h); path.lineTo(x, y); started = true; }
        else { path.lineTo(x, y); }
    }

    if (started)
    {
        path.lineTo(w, h);
        path.closeSubPath();

        // Fill with gradient
        juce::ColourGradient grad(colour.withAlpha(alpha * 0.5f), 0, 0,
            colour.withAlpha(0.0f), 0, h, false);
        g.setGradientFill(grad);
        g.fillPath(path);

        // Stroke the top line
        g.setColour(colour.withAlpha(alpha));
        g.strokePath(path, juce::PathStrokeType(1.2f));
    }
}

//==============================================================================
void SpectrumAnalyser::paint(juce::Graphics& g)
{
    const float w = (float)getWidth();
    const float h = (float)getHeight();

    // Background
    g.setColour(juce::Colour(0xff0c0c0e));
    g.fillRoundedRectangle(0, 0, w, h, 4.0f);

    // Grid lines — frequency markers
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f)));

    const float logMin = std::log10(20.0f);
    const float logMax = std::log10(20000.0f);
    const float logRange = logMax - logMin;

    const float gridFreqs[] = { 50.0f, 100.0f, 200.0f, 500.0f,
                                  1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* gridLabels[] = { "50", "100", "200", "500",
                                   "1k", "2k", "5k", "10k" };

    for (int i = 0; i < 8; ++i)
    {
        const float xNorm = (std::log10(gridFreqs[i]) - logMin) / logRange;
        const float x = xNorm * w;

        g.setColour(juce::Colour(0xff3a3645));  // freq labels
        g.drawVerticalLine((int)x, 0.0f, h - 12.0f);

        g.setColour(juce::Colour(0xff444440));
        g.drawText(gridLabels[i], (int)x - 12, (int)(h - 12), 24, 11,
            juce::Justification::centred);
    }

    // dB grid lines
    for (float db : { -12.0f, -24.0f, -48.0f, -72.0f })
    {
        const float yNorm = juce::jmap(db, -90.0f, 0.0f, 0.0f, 1.0f);
        const float y = h - yNorm * h;
        g.setColour(juce::Colour(0xff191720));
        g.drawHorizontalLine((int)y, 0.0f, w);
    }

    // Pre-loss — dim, desaturated purple ghost
    drawSpectrumPath(g, preMagnitudes,
        juce::Colour(0xff625370), 0.55f);

    // Post-loss — brighter, more saturated lavender
    drawSpectrumPath(g, postMagnitudes,
        juce::Colour(0xffb09cc8), 1.0f);

    // Border
    g.setColour(juce::Colour(0xff2c2c34));
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 4.0f, 1.0f);
}