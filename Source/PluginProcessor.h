#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "DSP/ChainDSP.h"

class VVChainAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int kSpectrumBins = 1024;

    VVChainAudioProcessor();
    ~VVChainAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VVChain"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void copySpectrumTo(float* destination, int numberOfBins) const noexcept;
    double getAnalyzerSampleRate() const noexcept { return analyzerSampleRate; }

private:
    void pushAnalyzerSamples(const juce::AudioBuffer<float>& buffer) noexcept;

    VVChainDSP dsp;

    static constexpr int kFFTOrder = 11;
    static constexpr int kFFTSize = 1 << kFFTOrder;

    juce::dsp::FFT analyzerFFT { kFFTOrder };
    juce::dsp::WindowingFunction<float> analyzerWindow
    {
        kFFTSize,
        juce::dsp::WindowingFunction<float>::hann,
        true
    };

    std::array<float, kFFTSize * 2> analyzerFftData {};
    int analyzerFifoIndex = 0;
    std::array<std::atomic<float>, kSpectrumBins> analyzerSpectrumDb {};
    double analyzerSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessor)
};
