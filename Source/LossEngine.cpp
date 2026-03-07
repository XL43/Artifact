#include "LossEngine.h"

//==============================================================================
void LossEngine::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    hannWindow.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
            * (float)i / (float)(fftSize - 1)));

    inputRing.assign(fftSize, 0.0f);
    outputAccum.assign(accumSize, 0.0f);
    fftBuffer.assign(fftSize * 2, 0.0f);
    previousFrame.assign(fftSize, 0.0f);

    disorderFrameBuffer.assign(disorderBufferFrames,
        std::vector<float>(fftSize, 0.0f));

    buildCodecWeights();

    reset();
    isPrepared = true;
}

//==============================================================================
void LossEngine::buildCodecWeights()
{
    const int numBins = fftSize / 2 + 1;
    voiceWeights.resize(numBins, 1.0f);
    broadcastWeights.resize(numBins, 1.0f);

    for (int b = 0; b < numBins; ++b)
    {
        const float binHz = (float)b * (float)currentSampleRate / (float)fftSize;

        // Voice: maps lossAmount to a normalised "how destroyed is this bin"
        // threshold. Bins with threshold < lossAmount are ALWAYS zeroed.
        // Sub-bass and air threshold = 0.0 → gone the instant loss > 0%
        // Speech band threshold = 0.8 → only gone when loss > 80%
        if (binHz < 80.0f)                         voiceWeights[b] = 0.0f;
        else if (binHz < 300.0f)                        voiceWeights[b] = 0.15f;
        else if (binHz >= 300.0f && binHz <= 3400.0f)   voiceWeights[b] = 0.85f;
        else if (binHz <= 6000.0f)                      voiceWeights[b] = 0.25f;
        else                                            voiceWeights[b] = 0.05f;

        // Broadcast: sub and air gone immediately, broadcast band preserved
        if (binHz < 80.0f)                         broadcastWeights[b] = 0.0f;
        else if (binHz < 150.0f)                        broadcastWeights[b] = 0.1f;
        else if (binHz >= 150.0f && binHz <= 8000.0f)   broadcastWeights[b] = 0.9f;
        else if (binHz <= 12000.0f)                     broadcastWeights[b] = 0.15f;
        else                                            broadcastWeights[b] = 0.02f;
    }
}

//==============================================================================
float LossEngine::binDropProb(int binIndex, float lossAmount, CodecMode codec) const
{
    // Music mode: pure random, flat across spectrum
    if (codec == CodecMode::Music)
        return lossAmount;

    // Voice / Broadcast: threshold-based deterministic zones.
    // The weight value IS the lossAmount threshold above which this bin
    // is ALWAYS zeroed. Below the threshold it uses a reduced random prob.
    //
    // weight = 0.0  → always zero (even at 1% loss)
    // weight = 0.5  → always zero when loss > 50%, random below that
    // weight = 0.9  → only zeroed randomly until loss hits 90%, then always gone
    //
    const float threshold = (codec == CodecMode::Voice)
        ? voiceWeights[binIndex]
        : broadcastWeights[binIndex];

    if (lossAmount >= threshold)
        return 1.0f;   // deterministically drop this bin

    // Below threshold: scale random probability to the remaining headroom
    // so the transition to always-drop is smooth rather than a hard click
    const float headroom = (threshold > 0.0f) ? threshold : 1.0f;
    return juce::jlimit(0.0f, 1.0f, (lossAmount / headroom) * 0.6f);
}

//==============================================================================
void LossEngine::reset()
{
    if (!isPrepared)
    {
        inputWritePos = outputReadPos = hopCounter = 0;
        disorderWriteIdx = disorderReadIdx = disorderFill = 0;
        lastFrameDropped = false;
        return;
    }

    std::fill(inputRing.begin(), inputRing.end(), 0.0f);
    std::fill(outputAccum.begin(), outputAccum.end(), 0.0f);
    std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
    std::fill(previousFrame.begin(), previousFrame.end(), 0.0f);

    for (auto& frame : disorderFrameBuffer)
        std::fill(frame.begin(), frame.end(), 0.0f);

    inputWritePos = 0;
    outputReadPos = 0;
    hopCounter = 0;
    disorderWriteIdx = 0;
    disorderReadIdx = 0;
    disorderFill = 0;
    lastFrameDropped = false;
}

//==============================================================================
void LossEngine::processBlock(float* data, int numSamples,
    float lossAmount, int hopSize,
    LossMode mode, CodecMode codec)
{
    jassert(isPrepared);
    jassert(hopSize > 0 && hopSize <= fftSize);

    for (int i = 0; i < numSamples; ++i)
    {
        inputRing[inputWritePos] = data[i];
        inputWritePos = (inputWritePos + 1) % fftSize;

        data[i] = outputAccum[outputReadPos];
        outputAccum[outputReadPos] = 0.0f;
        outputReadPos = (outputReadPos + 1) % accumSize;

        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            processFrame(lossAmount, hopSize, mode, codec);
        }
    }
}

//==============================================================================
void LossEngine::processFrame(float lossAmount, int hopSize,
    LossMode mode, CodecMode codec)
{
    // ── 1. Window input ───────────────────────────────────────────────────────
    for (int k = 0; k < fftSize; ++k)
    {
        const int idx = (inputWritePos + k) % fftSize;
        fftBuffer[k] = inputRing[idx] * hannWindow[k];
        fftBuffer[k + fftSize] = 0.0f;
    }

    // ── 2. Forward FFT ────────────────────────────────────────────────────────
    fft->performRealOnlyForwardTransform(fftBuffer.data(), true);

    const int numBins = fftSize / 2 + 1;
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distPi(-juce::MathConstants<float>::pi,
        juce::MathConstants<float>::pi);

    // ── 3. Mode-specific spectral processing ──────────────────────────────────
    switch (mode)
    {
        //----------------------------------------------------------------------
    case LossMode::Standard:
    {
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) < binDropProb(b, lossAmount, codec))
            {
                fftBuffer[b * 2] = 0.0f;
                fftBuffer[b * 2 + 1] = 0.0f;
            }
        }
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::Inverse:
    {
        for (int b = 1; b < numBins - 1; ++b)
        {
            // Keep only what Standard would have dropped
            if (dist01(rng) >= binDropProb(b, lossAmount, codec))
            {
                fftBuffer[b * 2] = 0.0f;
                fftBuffer[b * 2 + 1] = 0.0f;
            }
        }
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::PhaseJitter:
    {
        // Phase jitter is spectrally flat — codec mode has no effect here
        for (int b = 1; b < numBins - 1; ++b)
        {
            const float jitter = lossAmount * distPi(rng);
            const float re = fftBuffer[b * 2];
            const float im = fftBuffer[b * 2 + 1];
            fftBuffer[b * 2] = re * std::cos(jitter) - im * std::sin(jitter);
            fftBuffer[b * 2 + 1] = re * std::sin(jitter) + im * std::cos(jitter);
        }
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::PacketLoss:
    {
        if (dist01(rng) < lossAmount)
        {
            std::fill(fftBuffer.begin(), fftBuffer.begin() + fftSize * 2, 0.0f);
            lastFrameDropped = true;
        }
        else
        {
            lastFrameDropped = false;
        }
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::PacketRepeat:
    {
        lastFrameDropped = (dist01(rng) < lossAmount);
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::StdPlusPacketLoss:
    {
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) < binDropProb(b, lossAmount, codec))
            {
                fftBuffer[b * 2] = 0.0f;
                fftBuffer[b * 2 + 1] = 0.0f;
            }
        }
        if (dist01(rng) < lossAmount * 0.5f)
            std::fill(fftBuffer.begin(), fftBuffer.begin() + fftSize * 2, 0.0f);
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::StdPlusPacketRepeat:
    {
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) < binDropProb(b, lossAmount, codec))
            {
                fftBuffer[b * 2] = 0.0f;
                fftBuffer[b * 2 + 1] = 0.0f;
            }
        }
        lastFrameDropped = (dist01(rng) < lossAmount * 0.5f);
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::PacketDisorder:
    {
        // No spectral processing — disorder happens at OLA stage below
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::DisorderPlusStandard:
    {
        // Standard bin zeroing first, then disorder at OLA stage
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) < binDropProb(b, lossAmount, codec))
            {
                fftBuffer[b * 2] = 0.0f;
                fftBuffer[b * 2 + 1] = 0.0f;
            }
        }
        break;
    }
    }

    // ── 4. Inverse FFT ────────────────────────────────────────────────────────
    fft->performRealOnlyInverseTransform(fftBuffer.data());

    const float norm = 2.0f * (float)hopSize / (float)fftSize;

    // ── 5. Overlap-add (mode-specific output routing) ─────────────────────────
    const bool isDisorder = (mode == LossMode::PacketDisorder ||
        mode == LossMode::DisorderPlusStandard);

    if (isDisorder)
    {
        // Store decoded frame in disorder buffer
        auto& writeSlot = disorderFrameBuffer[disorderWriteIdx];
        for (int k = 0; k < fftSize; ++k)
            writeSlot[k] = fftBuffer[k] * norm;

        disorderWriteIdx = (disorderWriteIdx + 1) % disorderBufferFrames;
        if (disorderFill < disorderBufferFrames) ++disorderFill;

        if (disorderFill > 0)
        {
            int readIdx;
            if (dist01(rng) < lossAmount)
            {
                std::uniform_int_distribution<int> frameDist(0, disorderFill - 1);
                readIdx = (disorderWriteIdx - 1 - frameDist(rng)
                    + disorderBufferFrames) % disorderBufferFrames;
            }
            else
            {
                readIdx = disorderReadIdx;
                disorderReadIdx = (disorderReadIdx + 1) % disorderBufferFrames;
            }

            const auto& readSlot = disorderFrameBuffer[readIdx];
            for (int k = 0; k < fftSize; ++k)
            {
                const int idx = (outputReadPos + k) % accumSize;
                outputAccum[idx] += readSlot[k];
            }
        }
    }
    else if ((mode == LossMode::PacketRepeat || mode == LossMode::StdPlusPacketRepeat)
        && lastFrameDropped)
    {
        // Repeat previous frame
        for (int k = 0; k < fftSize; ++k)
        {
            const int idx = (outputReadPos + k) % accumSize;
            outputAccum[idx] += previousFrame[k] * norm;
        }
    }
    else
    {
        // Normal OLA
        for (int k = 0; k < fftSize; ++k)
        {
            const int idx = (outputReadPos + k) % accumSize;
            outputAccum[idx] += fftBuffer[k] * norm;
        }

        for (int k = 0; k < fftSize; ++k)
            previousFrame[k] = fftBuffer[k];
    }
}