#include "LossEngine.h"

//==============================================================================
void LossEngine::prepare(double /*sampleRate*/, int /*maxBlockSize*/)
{
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);

    hannWindow.resize(fftSize);
    for (int i = 0; i < fftSize; ++i)
        hannWindow[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi
            * (float)i / (float)(fftSize - 1)));

    inputRing.assign(fftSize, 0.0f);
    outputAccum.assign(accumSize, 0.0f);
    fftBuffer.assign(fftSize * 2, 0.0f);
    previousFrame.assign(fftSize, 0.0f);

    // Disorder buffer — pre-allocate all frames
    disorderFrameBuffer.assign(disorderBufferFrames, std::vector<float>(fftSize, 0.0f));
    disorderWriteIdx = 0;
    disorderReadIdx = 0;
    disorderFill = 0;

    inputWritePos = 0;
    outputReadPos = 0;
    hopCounter = 0;
    lastFrameDropped = false;
    isPrepared = true;
}

//==============================================================================
void LossEngine::reset()
{
    if (!isPrepared) return;

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
    float lossAmount, int hopSize, LossMode mode)
{
    jassert(isPrepared);
    jassert(hopSize > 0 && hopSize <= fftSize);

    for (int i = 0; i < numSamples; ++i)
    {
        // Feed input into ring buffer
        inputRing[inputWritePos] = data[i];
        inputWritePos = (inputWritePos + 1) % fftSize;

        // Read output sample and clear that slot
        data[i] = outputAccum[outputReadPos];
        outputAccum[outputReadPos] = 0.0f;
        outputReadPos = (outputReadPos + 1) % accumSize;

        // Process a new frame every hopSize samples
        if (++hopCounter >= hopSize)
        {
            hopCounter = 0;
            processFrame(lossAmount, hopSize, mode);
        }
    }
}

//==============================================================================
void LossEngine::processFrame(float lossAmount, int hopSize, LossMode mode)
{
    // ── Step 1: Window and copy ring buffer into fftBuffer ────────────────────
    for (int k = 0; k < fftSize; ++k)
    {
        const int idx = (inputWritePos + k) % fftSize;
        fftBuffer[k] = inputRing[idx] * hannWindow[k];
        fftBuffer[k + fftSize] = 0.0f;
    }

    // ── Step 2: Forward FFT ───────────────────────────────────────────────────
    fft->performRealOnlyForwardTransform(fftBuffer.data(), true);

    // ── Step 3: Per-mode spectral processing ──────────────────────────────────
    const int numBins = fftSize / 2 + 1;
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distPi(-juce::MathConstants<float>::pi,
        juce::MathConstants<float>::pi);

    // Helper: which bins would Standard mode zero?
    // We compute this mask for modes that need to know (Inverse, combos).
    auto buildDropMask = [&](std::vector<bool>& mask)
        {
            mask.resize(numBins);
            for (int b = 1; b < numBins - 1; ++b)
                mask[b] = (dist01(rng) < lossAmount);
            mask[0] = false; // always keep DC
            mask[numBins - 1] = false; // always keep Nyquist
        };

    switch (mode)
    {
        //----------------------------------------------------------------------
    case LossMode::Standard:
    {
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) < lossAmount)
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
        // Keep ONLY the bins that Standard would have dropped.
        // Bins that Standard keeps → we zero them.
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) >= lossAmount) // Standard would have kept this bin
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
        // Rotate each bin's phase by a random amount scaled by lossAmount.
        // This produces metallic ringing and pitch instability without
        // removing any energy — all bins stay present but are phase-corrupted.
        for (int b = 1; b < numBins - 1; ++b)
        {
            const float jitterAmount = lossAmount * distPi(rng);
            const float re = fftBuffer[b * 2];
            const float im = fftBuffer[b * 2 + 1];
            const float cosJ = std::cos(jitterAmount);
            const float sinJ = std::sin(jitterAmount);
            fftBuffer[b * 2] = re * cosJ - im * sinJ;
            fftBuffer[b * 2 + 1] = re * sinJ + im * cosJ;
        }
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::PacketLoss:
    {
        // Drop the entire frame with probability = lossAmount.
        // Output silence for this frame if dropped.
        if (dist01(rng) < lossAmount)
        {
            // Zero all bins — entire frame is lost
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
        // Drop the entire frame with probability = lossAmount.
        // On a drop, repeat the previous successfully decoded frame.
        if (dist01(rng) < lossAmount)
        {
            // Replace fftBuffer time-domain data with the previous frame.
            // We need to re-forward-FFT the previous frame, or more simply:
            // skip the IFFT write and OLA the previous frame directly.
            // Easiest: store and replay the previous post-IFFT frame.
            // We flag this and handle it after the IFFT below.
            lastFrameDropped = true;
        }
        else
        {
            lastFrameDropped = false;
        }
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::StdPlusPacketLoss:
    {
        // First apply Standard bin zeroing...
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) < lossAmount)
            {
                fftBuffer[b * 2] = 0.0f;
                fftBuffer[b * 2 + 1] = 0.0f;
            }
        }
        // ...then also drop the whole frame with probability = lossAmount * 0.5
        // (halved so the combo doesn't become completely silent at mid amounts)
        if (dist01(rng) < lossAmount * 0.5f)
            std::fill(fftBuffer.begin(), fftBuffer.begin() + fftSize * 2, 0.0f);
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::StdPlusPacketRepeat:
    {
        // Standard bin zeroing first
        for (int b = 1; b < numBins - 1; ++b)
        {
            if (dist01(rng) < lossAmount)
            {
                fftBuffer[b * 2] = 0.0f;
                fftBuffer[b * 2 + 1] = 0.0f;
            }
        }
        // Then potentially repeat the frame
        if (dist01(rng) < lossAmount * 0.5f)
            lastFrameDropped = true;
        else
            lastFrameDropped = false;
        break;
    }

    //----------------------------------------------------------------------
    case LossMode::PacketDisorder:
    {
        // No spectral processing here — disorder happens at the frame
        // output stage after the IFFT (see below)
        break;
    }
    }

    // ── Step 4: Inverse FFT ───────────────────────────────────────────────────
    fft->performRealOnlyInverseTransform(fftBuffer.data());

    // ── Step 5: Overlap-add ───────────────────────────────────────────────────
    const float norm = 2.0f * (float)hopSize / (float)fftSize;

    if (mode == LossMode::PacketDisorder)
    {
        // Store this decoded frame into the disorder buffer
        auto& writeSlot = disorderFrameBuffer[disorderWriteIdx];
        for (int k = 0; k < fftSize; ++k)
            writeSlot[k] = fftBuffer[k] * norm;

        disorderWriteIdx = (disorderWriteIdx + 1) % disorderBufferFrames;
        if (disorderFill < disorderBufferFrames) ++disorderFill;

        // Pick a random frame from the buffer to output
        if (disorderFill > 0)
        {
            // At lossAmount = 0: always read in order (no disorder)
            // At lossAmount = 1: fully random read index
            int readIdx;
            if (dist01(rng) < lossAmount)
            {
                // Random frame from the filled portion
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
        // Repeat the previous frame instead of the current one
        for (int k = 0; k < fftSize; ++k)
        {
            const int idx = (outputReadPos + k) % accumSize;
            outputAccum[idx] += previousFrame[k] * norm;
        }
    }
    else
    {
        // Normal overlap-add
        for (int k = 0; k < fftSize; ++k)
        {
            const int idx = (outputReadPos + k) % accumSize;
            outputAccum[idx] += fftBuffer[k] * norm;
        }

        // Save this frame as the previous frame for Packet Repeat
        for (int k = 0; k < fftSize; ++k)
            previousFrame[k] = fftBuffer[k];
    }
}