#include "ReverbEngine.h"

//==============================================================================
void ReverbEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;

    // ── Allpass delay times (prime numbers at 44.1kHz) ────────────────────────
    // Short delays build density in the early reflections.
    // Prime numbers avoid resonances at musical intervals.
    const int allpassDelays[numAllpass] = { 347, 113, 37, 59 };

    // ── Comb filter delay times (prime numbers at 44.1kHz) ────────────────────
    // Longer delays create the characteristic late reverb tail.
    // R channel offsets (+1 to +4 samples) create stereo width.
    const int combDelaysL[numCombs] = { 1687, 1601, 2053, 2251 };
    const int combDelaysR[numCombs] = { 1693, 1609, 2063, 2267 };

    // Maximum delay we'll ever need — comb delay + some headroom
    const int maxDelay = scaleDelay(3000);

    for (int i = 0; i < numAllpass; ++i)
    {
        allpassL[i].prepare(maxDelay);
        allpassR[i].prepare(maxDelay);
        allpassL[i].setDelay(scaleDelay(allpassDelays[i]));
        allpassR[i].setDelay(scaleDelay(allpassDelays[i]));
    }

    for (int i = 0; i < numCombs; ++i)
    {
        combL[i].prepare(maxDelay);
        combR[i].prepare(maxDelay);
        combL[i].setDelay(scaleDelay(combDelaysL[i]));
        combR[i].setDelay(scaleDelay(combDelaysR[i]));
    }

    isPrepared = true;
    reset();
}

//==============================================================================
void ReverbEngine::reset()
{
    for (int i = 0; i < numAllpass; ++i)
    {
        allpassL[i].reset();
        allpassR[i].reset();
    }
    for (int i = 0; i < numCombs; ++i)
    {
        combL[i].reset();
        combR[i].reset();
    }
    hpStateL = 0.0f;
    hpStateR = 0.0f;
}

//==============================================================================
void ReverbEngine::processBlock(juce::AudioBuffer<float>& buffer,
    float verbAmount, float verbDecay)
{
    jassert(isPrepared);

    if (verbAmount < 0.0001f)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    const float feedback = juce::jmap(verbDecay,
        0.0f, 2.0f,
        0.5f, 0.96f);
    for (int i = 0; i < numCombs; ++i)
    {
        combL[i].setFeedback(feedback);
        combR[i].setFeedback(feedback);
    }

    // Simple one-pole high-pass coefficient — blocks below ~80Hz
    // Prevents sub-bass from accumulating in comb filter feedback loops
    const float hpCoeff = 1.0f - (2.0f * juce::MathConstants<float>::pi
        * 80.0f / (float)currentSampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        float inL = (numChannels > 0) ? buffer.getSample(0, i) : 0.0f;
        float inR = (numChannels > 1) ? buffer.getSample(1, i) : inL;

        // ── High-pass input before reverb ─────────────────────────────────────
        // Removes sub-bass before it enters the feedback network
        const float hpInL = inL - hpStateL;
        const float hpInR = inR - hpStateR;
        hpStateL = hpStateL * hpCoeff + inL * (1.0f - hpCoeff);
        hpStateR = hpStateR * hpCoeff + inR * (1.0f - hpCoeff);

        // ── Allpass cascade ───────────────────────────────────────────────────
        float apL = hpInL;
        float apR = hpInR;
        for (int a = 0; a < numAllpass; ++a)
        {
            apL = allpassL[a].process(apL);
            apR = allpassR[a].process(apR);
        }

        // ── Parallel comb filters ─────────────────────────────────────────────
        float combOutL = 0.0f;
        float combOutR = 0.0f;
        for (int c = 0; c < numCombs; ++c)
        {
            combOutL += combL[c].process(apL);
            combOutR += combR[c].process(apR);
        }

        combOutL *= (1.0f / (float)numCombs);
        combOutR *= (1.0f / (float)numCombs);

        // ── Wet/dry blend ─────────────────────────────────────────────────────
        // inL/inR is the original dry signal — combOut is purely wet
        if (numChannels > 0)
            buffer.setSample(0, i, inL + combOutL * verbAmount);
        if (numChannels > 1)
            buffer.setSample(1, i, inR + combOutR * verbAmount);
    }
}