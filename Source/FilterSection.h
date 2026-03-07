#pragma once
#include <JuceHeader.h>

//==============================================================================
enum class FilterMode
{
    Normal = 0,   // Bandpass  — frequencies inside range are audible
    Inverted = 1    // Bandreject — frequencies outside range are audible
};

enum class FilterSlope
{
    dB12 = 0,   // 1 biquad  stage
    dB24 = 1,   // 2 biquad stages
    dB36 = 2,   // 3 biquad stages
    dB48 = 3    // 4 biquad stages
};

/*  FilterSection — stereo resonant filter with variable slope.

    Normal mode  : LP + HP in series  → bandpass character
    Inverted mode: LP + HP in parallel with polarity flip → bandreject

    One instance handles both channels.
    All state allocated in prepare() — nothing in processBlock.
*/
class FilterSection
{
public:
    static constexpr int maxStages = 4;   // max biquad stages per filter

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Process buffer in-place (stereo — numChannels must be 1 or 2)
    void processBlock(juce::AudioBuffer<float>& buffer,
        FilterMode mode,
        float lowCutHz, float highCutHz,
        FilterSlope slope);

private:
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using IIRCoeffPtr = juce::dsp::IIR::Coefficients<float>::Ptr;

    // LP and HP filter chains — up to maxStages stages per channel per type
    // Index: [channel][stage]
    IIRFilter lpFilters[2][maxStages];
    IIRFilter hpFilters[2][maxStages];

    // How many stages are currently active (depends on FilterSlope)
    int activeStages = 1;

    double currentSampleRate = 44100.0;
    bool   isPrepared = false;

    // Rebuild coefficients when params change
    void updateCoefficients(float lowCutHz, float highCutHz, FilterSlope slope);

    // Returns the number of biquad stages for a given slope enum
    static int stagesToInt(FilterSlope slope)
    {
        switch (slope)
        {
        case FilterSlope::dB12: return 1;
        case FilterSlope::dB24: return 2;
        case FilterSlope::dB36: return 3;
        case FilterSlope::dB48: return 4;
        }
        return 1;
    }
};