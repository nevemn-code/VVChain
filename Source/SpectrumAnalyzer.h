#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstring>

class VVChainSpectrumAnalyzer final : private juce::Thread
{
public:
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kHopSize = kFftSize / 2;
    static constexpr int kNumBins = kFftSize / 2;

    VVChainSpectrumAnalyzer();
    ~VVChainSpectrumAnalyzer() override;

    void prepare(double sampleRate);
    void reset() noexcept;
    void pushSamples(const float* monoSamples, int numSamples) noexcept;
    float getMagnitudeDb(int bin) const noexcept;

private:
    void run() override;

    static constexpr int kFifoSize = 16384;

    juce::AbstractFifo fifo { kFifoSize };
    std::array<float, kFifoSize> fifoBuffer {};
    std::array<float, kFftSize> history {};
    std::array<float, kFftSize * 2> fftBuffer {};
    std::array<std::atomic<float>, kNumBins> magnitudesDb {};

    juce::dsp::FFT fft { kFftOrder };
    std::array<float, kFftSize> window {};
    std::atomic<double> sampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainSpectrumAnalyzer)
};
