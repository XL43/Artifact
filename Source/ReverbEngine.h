#pragma once
#include <JuceHeader.h>

/*  ReverbEngine — stereo Schroeder-style algorithmic reverb.

    Architecture (per channel):
      Input → 4x Allpass cascade → 4x Parallel comb filters → sum → output

    Allpass cascade:    smears transients and builds initial density
    Parallel combs:     create the late reverb tail

    Delay times are tuned to prime-number sample counts (at 44.1kHz) to
    avoid flutter echoes and modal resonances. They are scaled proportionally
    when the sample rate differs from 44.1kHz.

    verbAmount : 0.0 → 1.0  wet level
    verbDecay  : 0.0 → 2.0  scales comb feedback (0 = short, 2 = long)
*/
class ReverbEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Process buffer in-place (stereo)
    // verbAmount : 0.0 (dry) → 1.0 (full wet)
    // verbDecay  : 0.0 → 2.0
    void processBlock(juce::AudioBuffer<float>& buffer,
        float verbAmount, float verbDecay);

private:
    //==========================================================================
    // Simple delay line — fixed max size, no heap allocation after prepare()
    struct DelayLine
    {
        std::vector<float> buffer;
        int writePos = 0;
        int delayInSamples = 0;

        void prepare(int maxDelaySamples)
        {
            buffer.assign(maxDelaySamples + 1, 0.0f);
            writePos = 0;
        }

        void setDelay(int samples)
        {
            delayInSamples = juce::jlimit(1, (int)buffer.size() - 1, samples);
        }

        void reset() { std::fill(buffer.begin(), buffer.end(), 0.0f); writePos = 0; }

        void write(float sample)
        {
            buffer[writePos] = sample;
            writePos = (writePos + 1) % (int)buffer.size();
        }

        float read() const
        {
            int readPos = writePos - delayInSamples;
            if (readPos < 0) readPos += (int)buffer.size();
            return buffer[readPos];
        }
    };

    //==========================================================================
    // Allpass filter — Schroeder allpass section
    // y[n] = -g*x[n] + x[n-D] + g*y[n-D]
    struct AllpassFilter
    {
        DelayLine delay;
        float g = 0.7f;   // allpass coefficient — fixed

        void prepare(int maxDelaySamples) { delay.prepare(maxDelaySamples); }
        void setDelay(int samples) { delay.setDelay(samples); }
        void reset() { delay.reset(); }

        float process(float input)
        {
            const float delayed = delay.read();
            const float output = -g * input + delayed;
            delay.write(input + g * delayed);
            return output;
        }
    };

    //==========================================================================
    // Comb filter — feedback comb with damping lowpass
    // y[n] = x[n] + feedback * damp(y[n-D])
    struct CombFilter
    {
        DelayLine delay;
        float feedback = 0.84f;
        float dampCoeff = 0.5f;    // increased from 0.2 — prevents bass accumulation
        float filterState = 0.0f;

        void prepare(int maxDelaySamples) { delay.prepare(maxDelaySamples); }
        void setDelay(int samples) { delay.setDelay(samples); }
        void reset() { delay.reset(); filterState = 0.0f; }
        void setFeedback(float fb) { feedback = juce::jlimit(0.0f, 0.97f, fb); }

        // Returns ONLY the wet reverb signal — input is NOT included in output.
        // Dry/wet blending is handled in processBlock, not here.
        float process(float input)
        {
            const float delayed = delay.read();
            filterState = delayed * (1.0f - dampCoeff) + filterState * dampCoeff;
            const float output = input + filterState * feedback;
            delay.write(output);
            return filterState * feedback; // wet only — no input passthrough
        }
    };

    //==========================================================================
    static constexpr int numAllpass = 4;
    static constexpr int numCombs = 4;

    // One set per channel (L and R use slightly offset delay times for width)
    AllpassFilter allpassL[numAllpass];
    AllpassFilter allpassR[numAllpass];
    CombFilter    combL[numCombs];
    CombFilter    combR[numCombs];

    double currentSampleRate = 44100.0;
    bool   isPrepared = false;

    // Scale a 44.1kHz sample count to the current sample rate
    int scaleDelay(int samplesAt44k) const
    {
        return juce::roundToInt(samplesAt44k * currentSampleRate / 44100.0);
    }


    // High-pass filter state for reverb input
    float hpStateL = 0.0f;
    float hpStateR = 0.0f;
};