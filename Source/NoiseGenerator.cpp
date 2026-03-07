#include "NoiseGenerator.h"

//==============================================================================
void NoiseGenerator::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;
    reset();
    isPrepared = true;
}

//==============================================================================
void NoiseGenerator::reset()
{
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    brownLast = 0.0f;
    biasFilterState = 0.0f;
}

//==============================================================================
float NoiseGenerator::generateWhite()
{
    return dist(rng);
}

//==============================================================================
float NoiseGenerator::generatePink()
{
    // Paul Kellet's economy pink noise algorithm.
    // Approximates -3dB/oct by summing filtered white noise at multiple
    // time scales. All state (b0–b6) pre-allocated — no allocation here.
    const float white = dist(rng);

    b0 = 0.99886f * b0 + white * 0.0555179f;
    b1 = 0.99332f * b1 + white * 0.0750759f;
    b2 = 0.96900f * b2 + white * 0.1538520f;
    b3 = 0.86650f * b3 + white * 0.3104856f;
    b4 = 0.55000f * b4 + white * 0.5329522f;
    b5 = -0.7616f * b5 - white * 0.0168980f;

    const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
    b6 = white * 0.115926f;

    return pink * 0.11f; // scale to roughly unity RMS
}

//==============================================================================
float NoiseGenerator::generateBrown()
{
    // Leaky integrator on white noise gives -6dB/oct (brownian motion).
    // Leak factor 0.998 prevents DC buildup.
    const float white = dist(rng);
    brownLast = (brownLast + (0.02f * white)) * 0.998f;
    return brownLast * 3.5f; // scale to roughly unity RMS
}

//==============================================================================
void NoiseGenerator::applyBiasFilter(float* data, int numSamples, float bias)
{
    // bias = 0.0  → no filtering
    // bias > 0.0  → HP shelf: attenuates lows, emphasises highs
    // bias < 0.0  → LP shelf: attenuates highs, emphasises lows
    //
    // Implementation: one-pole filter blended with dry signal.
    // Positive bias mixes in a high-pass (signal - LP output).
    // Negative bias mixes in a low-pass.
    //
    // Cutoff is fixed at ~500Hz as the shelf transition point.

    if (std::abs(bias) < 0.01f)
        return; // no bias — skip entirely

    const float cutoffHz = 500.0f;
    const float rc = 1.0f / (2.0f * juce::MathConstants<float>::pi * cutoffHz);
    const float dt = 1.0f / (float)currentSampleRate;
    const float alpha = dt / (rc + dt);  // LP coefficient

    const float absBias = std::abs(bias);  // 0.0 → 1.0

    for (int i = 0; i < numSamples; ++i)
    {
        biasFilterState += alpha * (data[i] - biasFilterState);
        const float lp = biasFilterState;
        const float hp = data[i] - lp;

        if (bias > 0.0f)
            data[i] = data[i] * (1.0f - absBias) + hp * absBias;  // blend toward HP
        else
            data[i] = data[i] * (1.0f - absBias) + lp * absBias;  // blend toward LP
    }
}

//==============================================================================
void NoiseGenerator::process(float* dest, int numSamples,
    float amount, NoiseColor color, float bias)
{
    jassert(isPrepared);

    if (amount < 0.0001f)
        return; // nothing to do

    // ── 1. Generate noise into a temporary stack buffer ───────────────────────
    // Using a fixed-size stack array avoids heap allocation.
    // 4096 samples covers any realistic DAW buffer size.
    constexpr int maxBuf = 4096;
    jassert(numSamples <= maxBuf);
    float noiseBuf[maxBuf];

    switch (color)
    {
    case NoiseColor::White:
        for (int i = 0; i < numSamples; ++i)
            noiseBuf[i] = generateWhite();
        break;

    case NoiseColor::Pink:
        for (int i = 0; i < numSamples; ++i)
            noiseBuf[i] = generatePink();
        break;

    case NoiseColor::Brown:
        for (int i = 0; i < numSamples; ++i)
            noiseBuf[i] = generateBrown();
        break;
    }

    // ── 2. Apply bias filter ──────────────────────────────────────────────────
    // bias param is -100 to +100, normalise to -1.0 → +1.0
    const float normBias = bias / 100.0f;
    applyBiasFilter(noiseBuf, numSamples, normBias);

    // ── 3. Scale by amount and ADD to destination buffer ─────────────────────
    // Noise enters the Loss engine by being summed with the input signal.
    // Scale factor 0.25 keeps the noise at a sensible level relative to
    // the audio signal — amount = 1.0 gives roughly -12dBFS of noise.
    const float scale = amount * 0.25f;
    for (int i = 0; i < numSamples; ++i)
        dest[i] += noiseBuf[i] * scale;
}