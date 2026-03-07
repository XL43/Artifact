#pragma once
#include <JuceHeader.h>
#include <random>

//==============================================================================
// Must match the AudioParameterChoice order in PluginProcessor.cpp exactly
enum class LossMode
{
    Standard = 0,
    Inverse,
    PhaseJitter,
    PacketRepeat,
    PacketLoss,
    StdPlusPacketLoss,
    StdPlusPacketRepeat,
    PacketDisorder
};

//==============================================================================
class LossEngine
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;  // 2048
    static constexpr int accumSize = fftSize * 2;     // 4096

    // ── Disorder buffer ───────────────────────────────────────────────────────
    // Stores up to this many reconstructed frames before shuffling playback order.
    static constexpr int disorderBufferFrames = 8;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // lossAmount : 0.0 → 1.0
    // hopSize    : samples between frames (Loss Speed)
    // mode       : which algorithm to apply
    void processBlock(float* channelData, int numSamples,
        float lossAmount, int hopSize, LossMode mode);

private:
    void processFrame(float lossAmount, int hopSize, LossMode mode);

    // ── Core FFT machinery ────────────────────────────────────────────────────
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> hannWindow;
    std::vector<float> inputRing;
    std::vector<float> outputAccum;
    std::vector<float> fftBuffer;

    int inputWritePos = 0;
    int outputReadPos = 0;
    int hopCounter = 0;

    // ── Packet Repeat / Packet Loss ───────────────────────────────────────────
    // Stores the last successfully output frame so we can repeat it on drops
    std::vector<float> previousFrame;
    bool lastFrameDropped = false;

    // ── Packet Disorder ───────────────────────────────────────────────────────
    // Circular buffer of decoded frames — played back in shuffled order
    std::vector<std::vector<float>> disorderFrameBuffer;
    int disorderWriteIdx = 0;
    int disorderReadIdx = 0;
    int disorderFill = 0;  // how many frames are currently buffered

    // ── RNG ───────────────────────────────────────────────────────────────────
    std::mt19937 rng{ 12345 };

    bool isPrepared = false;
};