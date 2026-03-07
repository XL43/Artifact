#pragma once
#include <JuceHeader.h>
#include <random>

//==============================================================================
enum class NoiseColor
{
    White = 0,
    Pink,
    Brown
};

/*  NoiseGenerator — single-channel colored noise source.
    Instantiate one per channel. All state pre-allocated in prepare().
    No heap allocation inside processBlock.

    White: flat spectrum
    Pink:  -3dB/oct  (Paul Kellet's 3-multiplier approximation)
    Brown: -6dB/oct  (leaky integrator on white noise)

    noiseBias: -100 = concentrate noise in low frequencies (LP shelf)
                  0 = flat
               +100 = concentrate noise in high frequencies (HP shelf)
*/
class NoiseGenerator
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Fills dest[0..numSamples-1] with noise and ADDS it to the existing
    // content — call this before the Loss engine on the same buffer.
    //   amount    : 0.0 (silent) → 1.0 (full level)
    //   color     : White / Pink / Brown
    //   bias      : -1.0 → +1.0
    void process(float* dest, int numSamples,
        float amount, NoiseColor color, float bias);

private:
    float generateWhite();
    float generatePink();
    float generateBrown();

    void applyBiasFilter(float* data, int numSamples, float bias);

    // ── Pink noise state (Paul Kellet's algorithm) ────────────────────────────
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float b3 = 0.0f, b4 = 0.0f, b5 = 0.0f, b6 = 0.0f;

    // ── Brown noise state ─────────────────────────────────────────────────────
    float brownLast = 0.0f;

    // ── Bias filter state (one-pole LP / HP shelving) ─────────────────────────
    float biasFilterState = 0.0f;
    double currentSampleRate = 44100.0;

    // ── RNG ───────────────────────────────────────────────────────────────────
    std::mt19937 rng{ 99999 }; // different seed from LossEngine
    std::uniform_real_distribution<float> dist{ -1.0f, 1.0f };

    bool isPrepared = false;
};