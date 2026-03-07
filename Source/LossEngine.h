#pragma once
#include <JuceHeader.h>
#include <random>

//==============================================================================
enum class LossMode
{
    Standard = 0,
    Inverse,
    PhaseJitter,
    PacketRepeat,
    PacketLoss,
    StdPlusPacketLoss,
    StdPlusPacketRepeat,
    PacketDisorder,
    DisorderPlusStandard   // ← new combo mode
};

enum class CodecMode
{
    Music = 0,   // Uniform bin selection across full spectrum
    Voice,       // Mid-range (200–3000 Hz) bins drop first — telephone character
    Broadcast    // High (>8kHz) and low (<200Hz) bins drop first — radio bandwidth
};

//==============================================================================
class LossEngine
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;  // 2048
    static constexpr int accumSize = fftSize * 2;    // 4096
    static constexpr int disorderBufferFrames = 8;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    void processBlock(float* channelData, int numSamples,
        float lossAmount, int hopSize,
        LossMode mode, CodecMode codec);

private:
    void processFrame(float lossAmount, int hopSize,
        LossMode mode, CodecMode codec);

    // Returns the drop probability for a given bin index, respecting codec mode
    float binDropProb(int binIndex, float lossAmount, CodecMode codec) const;

    // ── Core FFT ──────────────────────────────────────────────────────────────
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> hannWindow;
    std::vector<float> inputRing;
    std::vector<float> outputAccum;
    std::vector<float> fftBuffer;

    int inputWritePos = 0;
    int outputReadPos = 0;
    int hopCounter = 0;

    // ── Packet Repeat ─────────────────────────────────────────────────────────
    std::vector<float> previousFrame;
    bool lastFrameDropped = false;

    // ── Packet Disorder ───────────────────────────────────────────────────────
    std::vector<std::vector<float>> disorderFrameBuffer;
    int disorderWriteIdx = 0;
    int disorderReadIdx = 0;
    int disorderFill = 0;

    // ── Codec Mode weight tables ───────────────────────────────────────────────
    // Pre-computed per-bin multipliers applied to lossAmount before the drop test.
    // Avoids any per-sample branching on codec mode inside the hot loop.
    std::vector<float> voiceWeights;      // high weight = drops sooner
    std::vector<float> broadcastWeights;

    double currentSampleRate = 44100.0;
    void buildCodecWeights();             // called inside prepare()

    std::mt19937 rng{ 12345 };
    bool isPrepared = false;
};