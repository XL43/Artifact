#include "FilterSection.h"

//==============================================================================
void FilterSection::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32)maxBlockSize;
    spec.numChannels = 1; // each filter processes one channel

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < maxStages; ++s)
        {
            lpFilters[ch][s].prepare(spec);
            hpFilters[ch][s].prepare(spec);
        }
    }

    isPrepared = true;
    reset();
}

//==============================================================================
void FilterSection::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < maxStages; ++s)
        {
            lpFilters[ch][s].reset();
            hpFilters[ch][s].reset();
        }
    }
}

//==============================================================================
void FilterSection::updateCoefficients(float lowCutHz, float highCutHz,
    FilterSlope slope)
{
    activeStages = stagesToInt(slope);

    // Clamp frequencies to safe ranges
    // LP cutoff must be above HP cutoff, both within Nyquist
    const float nyquist = (float)currentSampleRate * 0.5f;
    const float safeHP = juce::jlimit(20.0f, nyquist * 0.95f, lowCutHz);
    const float safeLP = juce::jlimit(safeHP + 10.0f, nyquist * 0.98f, highCutHz);

    // Build coefficients — same coefficients applied to all stages
    // (cascading identical biquads gives steeper slope)
    auto lpCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(
        currentSampleRate, safeLP);

    auto hpCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
        currentSampleRate, safeHP);

    // Apply to all channels and all active stages
    for (int ch = 0; ch < 2; ++ch)
    {
        for (int s = 0; s < activeStages; ++s)
        {
            *lpFilters[ch][s].coefficients = *lpCoeffs;
            *hpFilters[ch][s].coefficients = *hpCoeffs;
        }
    }
}

//==============================================================================
void FilterSection::processBlock(juce::AudioBuffer<float>& buffer,
    FilterMode mode,
    float lowCutHz, float highCutHz,
    FilterSlope slope)
{
    jassert(isPrepared);

    // Rebuild coefficients every block — cheap enough and avoids
    // needing a change-detection system for now
    updateCoefficients(lowCutHz, highCutHz, slope);

    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    if (mode == FilterMode::Normal)
    {
        // ── Bandpass: LP then HP in series ────────────────────────────────────
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);

            // Run through all active LP stages
            for (int s = 0; s < activeStages; ++s)
            {
                auto block = juce::dsp::AudioBlock<float>(&data, 1, (size_t)numSamples);
                auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
                lpFilters[ch][s].process(ctx);
            }

            // Then all active HP stages
            for (int s = 0; s < activeStages; ++s)
            {
                auto block = juce::dsp::AudioBlock<float>(&data, 1, (size_t)numSamples);
                auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
                hpFilters[ch][s].process(ctx);
            }
        }
    }
    else // FilterMode::Inverted — bandreject
    {
        // ── Bandreject: LP and HP in parallel, summed ─────────────────────────
        // Strategy:
        //   lpOut = lowpass of input
        //   hpOut = highpass of input
        //   output = lpOut + hpOut
        //
        // We need separate LP and HP buffers to avoid processing the
        // already-modified signal. Use a stack copy for the HP path.

        // Max stack buffer size — covers any realistic DAW block size
        constexpr int maxBuf = 4096;
        jassert(numSamples <= maxBuf);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);

            // Copy input into a temporary HP buffer
            float hpBuf[maxBuf];
            std::copy(data, data + numSamples, hpBuf);

            // Process LP path in-place on data
            for (int s = 0; s < activeStages; ++s)
            {
                auto block = juce::dsp::AudioBlock<float>(&data, 1, (size_t)numSamples);
                auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
                lpFilters[ch][s].process(ctx);
            }

            // Process HP path on the copy
            float* hpPtr = hpBuf;
            for (int s = 0; s < activeStages; ++s)
            {
                auto block = juce::dsp::AudioBlock<float>(&hpPtr, 1, (size_t)numSamples);
                auto ctx = juce::dsp::ProcessContextReplacing<float>(block);
                hpFilters[ch][s].process(ctx);
            }

            // Sum LP + HP into output
            for (int i = 0; i < numSamples; ++i)
                data[i] += hpBuf[i];
        }
    }
}