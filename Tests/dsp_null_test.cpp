#include "../Source/DSP/ChainDSP.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
VVChainDSP::Parameters neutralParameters(bool delta = false)
{
    VVChainDSP::Parameters p;
    p.eqBypass = true;
    p.eqColorGlobalBypass = true;
    p.udmbcBypass = true;
    p.tapeBypass = true;
    p.mixBypass = false;
    p.masterBypass = false;
    p.deltaMonitor = delta;
    p.dryWet = 100.0f;
    p.outputDb = 0.0f;
    p.soloBand = -1;
    p.graphSoloActive = false;
    p.transientAmount = { 0.f, 0.f, 0.f, 0.f };
    p.eqColor = { 0.f, 0.f, 0.f, 0.f };
    p.udmbcDegree = { 0.f, 0.f, 0.f, 0.f };
    p.tapeDegree = { 0.f, 0.f, 0.f, 0.f };
    return p;
}

float sourceSample(int channel, int index)
{
    const double t = static_cast<double>(index) / 48000.0;
    const double a = channel == 0 ? 0.041 : 0.037;
    return static_cast<float>(
        a * std::sin(2.0 * juce::MathConstants<double>::pi * 997.0 * t)
        + 0.013 * std::sin(2.0 * juce::MathConstants<double>::pi * 6137.0 * t));
}

bool runNeutralNull()
{
    constexpr int blockSize = 256;
    constexpr int totalSamples = 16384;

    VVChainDSP dsp;
    dsp.prepare(48000.0, blockSize, 2);
    const auto p = neutralParameters(false);
    const int latency = dsp.getLatencySamples();

    std::vector<float> inputL(totalSamples), inputR(totalSamples);
    for (int n = 0; n < totalSamples; ++n)
    {
        inputL[(size_t)n] = sourceSample(0, n);
        inputR[(size_t)n] = sourceSample(1, n);
    }

    double maxError = 0.0;
    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        juce::AudioBuffer<float> block(2, blockSize);
        for (int n = 0; n < blockSize; ++n)
        {
            block.setSample(0, n, inputL[(size_t)(offset + n)]);
            block.setSample(1, n, inputR[(size_t)(offset + n)]);
        }

        dsp.process(block, p);

        for (int n = 0; n < blockSize; ++n)
        {
            const int global = offset + n;
            const int source = global - latency;
            const float expectedL = source >= 0 ? inputL[(size_t)source] : 0.f;
            const float expectedR = source >= 0 ? inputR[(size_t)source] : 0.f;
            maxError = std::max(maxError,
                static_cast<double>(std::abs(block.getSample(0, n) - expectedL)));
            maxError = std::max(maxError,
                static_cast<double>(std::abs(block.getSample(1, n) - expectedR)));
        }
    }

    if (maxError > 2.0e-6)
    {
        std::cerr << "neutral null failed: max error=" << maxError << "\n";
        return false;
    }

    VVChainDSP deltaDsp;
    deltaDsp.prepare(48000.0, blockSize, 2);
    const auto deltaParams = neutralParameters(true);
    double deltaPeak = 0.0;

    for (int offset = 0; offset < totalSamples; offset += blockSize)
    {
        juce::AudioBuffer<float> block(2, blockSize);
        for (int n = 0; n < blockSize; ++n)
        {
            block.setSample(0, n, inputL[(size_t)(offset + n)]);
            block.setSample(1, n, inputR[(size_t)(offset + n)]);
        }
        deltaDsp.process(block, deltaParams);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < blockSize; ++n)
                deltaPeak = std::max(
                    deltaPeak,
                    static_cast<double>(std::abs(block.getSample(ch, n))));
    }

    if (deltaPeak > 2.0e-6)
    {
        std::cerr << "neutral delta failed: peak=" << deltaPeak << "\n";
        return false;
    }

    std::cout << "neutral full-chain null: PASS; latency="
              << latency << " samples; max error=" << maxError
              << "; delta peak=" << deltaPeak << "\n";
    return true;
}

bool runLargeBlock()
{
    constexpr int numSamples = 70000;

    VVChainDSP dsp;
    dsp.prepare(48000.0, 64, 2);
    const auto p = neutralParameters(false);
    const int latency = dsp.getLatencySamples();

    juce::AudioBuffer<float> buffer(2, numSamples);
    std::vector<float> inputL(numSamples), inputR(numSamples);
    for (int n = 0; n < numSamples; ++n)
    {
        inputL[(size_t)n] = sourceSample(0, n);
        inputR[(size_t)n] = sourceSample(1, n);
        buffer.setSample(0, n, inputL[(size_t)n]);
        buffer.setSample(1, n, inputR[(size_t)n]);
    }

    dsp.process(buffer, p);

    double maxError = 0.0;
    for (int n = 0; n < numSamples; ++n)
    {
        const int source = n - latency;
        const float expectedL = source >= 0 ? inputL[(size_t)source] : 0.f;
        const float expectedR = source >= 0 ? inputR[(size_t)source] : 0.f;
        maxError = std::max(maxError,
            static_cast<double>(std::abs(buffer.getSample(0, n) - expectedL)));
        maxError = std::max(maxError,
            static_cast<double>(std::abs(buffer.getSample(1, n) - expectedR)));
    }

    if (maxError > 2.0e-6)
    {
        std::cerr << "large-block processing failed: max error=" << maxError << "\n";
        return false;
    }

    std::cout << "large-block chunking: PASS; samples=" << numSamples
              << "; max error=" << maxError << "\n";
    return true;
}
}

int main()
{
    if (!runNeutralNull())
        return 1;
    if (!runLargeBlock())
        return 2;
    std::cout << "PASS VVChain DSP signal-integrity harness\n";
    return 0;
}
