#pragma once
#include <JuceHeader.h>

/*  SpectrumAnalyser — lock-free FFT spectrum display.

    The audio thread calls pushSamples() for both pre-loss and post-loss buffers.
    A timer fires at 30Hz, pulls samples from the FIFO, runs FFT, smooths the
    magnitude data, and repaints.

    Pre-loss spectrum: dim amber  — shows what went IN to the Loss engine
    Post-loss spectrum: full amber — shows what came OUT

    The visual difference between them makes the Loss engine's effect immediately
    obvious — frequency holes, smearing, and dropout are all visible in real time.
*/
class SpectrumAnalyser : public juce::Component,
    private juce::Timer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;  // 2048
    static constexpr int fifoSize = fftSize * 4;     // ring buffer

    SpectrumAnalyser();
    ~SpectrumAnalyser() override = default;

    // Called from audio thread — push one channel of samples
    // isPreLoss: true = before Loss engine, false = after
    void pushSamples(const float* data, int numSamples, bool isPreLoss);

    void paint(juce::Graphics&) override;
    void resized() override {}

private:
    void timerCallback() override;
    void drawSpectrumPath(juce::Graphics& g, const std::array<float, fftSize / 2>& magnitudes,
        juce::Colour colour, float alpha);

    // ── Lock-free FIFO ────────────────────────────────────────────────────────
    juce::AbstractFifo preFifo{ fifoSize };
    juce::AbstractFifo postFifo{ fifoSize };
    std::array<float, fifoSize> preFifoBuffer;
    std::array<float, fifoSize> postFifoBuffer;

    // ── FFT processing ────────────────────────────────────────────────────────
    juce::dsp::FFT                         fft{ fftOrder };
    juce::dsp::WindowingFunction<float>    window{ fftSize,
                                                    juce::dsp::WindowingFunction<float>::hann };

    std::array<float, fftSize * 2> fftWorkBuffer;

    // ── Smoothed magnitude bins ───────────────────────────────────────────────
    std::array<float, fftSize / 2> preMagnitudes;
    std::array<float, fftSize / 2> postMagnitudes;

    // Accumulation buffer — collect samples until we have a full fftSize frame
    std::array<float, fftSize> preAccum;
    std::array<float, fftSize> postAccum;
    int preAccumPos = 0;
    int postAccumPos = 0;

    // Smoothing coefficient — higher = slower decay
    static constexpr float smoothingCoeff = 0.78f;

    void processFifo(juce::AbstractFifo& fifo,
        std::array<float, fifoSize>& fifoBuffer,
        std::array<float, fftSize>& accum,
        int& accumPos,
        std::array<float, fftSize / 2>& magnitudes);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyser)
};