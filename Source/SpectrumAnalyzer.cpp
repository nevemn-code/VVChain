#include "SpectrumAnalyzer.h"

VVChainSpectrumAnalyzer::VVChainSpectrumAnalyzer()
    : juce::Thread("VVChain Spectrum")
{
    juce::dsp::WindowingFunction<float> windowing(
        kFftSize,
        juce::dsp::WindowingFunction<float>::hann,
        false);

    std::array<float, kFftSize> unity {};
    unity.fill(1.0f);
    windowing.multiplyWithWindowingTable(unity.data(), kFftSize);

    float sum = 0.0f;
    for (const auto v : unity)
        sum += v;

    const float scale = sum > 1.0e-6f
        ? static_cast<float>(kFftSize) / sum
        : 1.0f;

    for (int i = 0; i < kFftSize; ++i)
        window[(size_t) i] = unity[(size_t) i] * scale;

    for (auto& value : magnitudesDb)
        value.store(-120.0f, std::memory_order_relaxed);

    startThread();
}

VVChainSpectrumAnalyzer::~VVChainSpectrumAnalyzer()
{
    stopThread(250);
}

void VVChainSpectrumAnalyzer::prepare(double newSampleRate)
{
    sampleRate.store(std::max(8000.0, newSampleRate),
                     std::memory_order_release);
    reset();
}

void VVChainSpectrumAnalyzer::reset() noexcept
{
    fifo.reset();
    history.fill(0.0f);
    fftBuffer.fill(0.0f);

    for (auto& value : magnitudesDb)
        value.store(-120.0f, std::memory_order_relaxed);
}

void VVChainSpectrumAnalyzer::pushSamples(
    const float* monoSamples, int numSamples) noexcept
{
    if (monoSamples == nullptr || numSamples <= 0)
        return;

    int offset = 0;
    while (offset < numSamples)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite(numSamples - offset,
                            start1, size1, start2, size2);

        if (size1 == 0 && size2 == 0)
            return;

        if (size1 > 0)
        {
            std::memcpy(fifoBuffer.data() + start1,
                        monoSamples + offset,
                        static_cast<size_t>(size1) * sizeof(float));
            offset += size1;
        }

        if (size2 > 0)
        {
            std::memcpy(fifoBuffer.data() + start2,
                        monoSamples + offset,
                        static_cast<size_t>(size2) * sizeof(float));
            offset += size2;
        }

        fifo.finishedWrite(size1 + size2);
    }
}

float VVChainSpectrumAnalyzer::getMagnitudeDb(int bin) const noexcept
{
    if (bin < 0 || bin >= kNumBins)
        return -120.0f;

    return magnitudesDb[(size_t) bin].load(std::memory_order_relaxed);
}

void VVChainSpectrumAnalyzer::run()
{
    while (!threadShouldExit())
    {
        if (fifo.getNumReady() < kHopSize)
        {
            wait(4);
            continue;
        }

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(kHopSize, start1, size1, start2, size2);

        const int available = size1 + size2;
        if (available <= 0)
        {
            fifo.finishedRead(0);
            continue;
        }

        std::memmove(history.data(),
                     history.data() + kHopSize,
                     static_cast<size_t>(kHopSize) * sizeof(float));

        if (size1 > 0)
            std::memcpy(history.data() + kHopSize,
                        fifoBuffer.data() + start1,
                        static_cast<size_t>(size1) * sizeof(float));

        if (size2 > 0)
            std::memcpy(history.data() + kHopSize + size1,
                        fifoBuffer.data() + start2,
                        static_cast<size_t>(size2) * sizeof(float));

        fifo.finishedRead(available);

        for (int i = 0; i < kFftSize; ++i)
            fftBuffer[(size_t) i] =
                history[(size_t) i] * window[(size_t) i];

        std::fill(fftBuffer.begin() + kFftSize, fftBuffer.end(), 0.0f);
        fft.performRealOnlyForwardTransform(fftBuffer.data(), true);

        constexpr float normalisation = 2.0f / static_cast<float>(kFftSize);

        for (int bin = 0; bin < kNumBins; ++bin)
        {
            const float real = fftBuffer[(size_t) (bin * 2)];
            const float imag = fftBuffer[(size_t) (bin * 2 + 1)];
            const float magnitude =
                std::sqrt(real * real + imag * imag) * normalisation;

            const float db = juce::Decibels::gainToDecibels(
                juce::jlimit(1.0e-7f, 1.0e6f, magnitude), -120.0f);

            const float old =
                magnitudesDb[(size_t) bin].load(std::memory_order_relaxed);

            magnitudesDb[(size_t) bin].store(
                old + (db - old) * 0.32f,
                std::memory_order_relaxed);
        }
    }
}
