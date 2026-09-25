#pragma once

#include <JuceHeader.h>
#include <array>
#include "DSP/ChainDSP.h"

class VVChainAudioProcessor final : public juce::AudioProcessor
{
public:
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

    float getDynamicMidGainChangeDb(int band) const noexcept { return dsp.dynamicMidGainChangeDb(band); }
    float getDynamicSideGainChangeDb(int band) const noexcept { return dsp.dynamicSideGainChangeDb(band); }
    float getDynamicAverageGainChangeDb(int band) const noexcept { return dsp.dynamicAverageGainChangeDb(band); }

    void setAnalyzerEnabled(bool enabled) noexcept { analyzerEnabled.store(enabled, std::memory_order_relaxed); }
    bool isAnalyzerEnabled() const noexcept { return analyzerEnabled.load(std::memory_order_relaxed); }
    int popAnalyzerSamples(float* destination, int maxSamples) noexcept;

private:
    static constexpr int analyzerCapacity = 16384;
    VVChainDSP dsp;
    std::array<float, analyzerCapacity> analyzerBuffer {};
    juce::AbstractFifo analyzerFifo { analyzerCapacity };
    std::atomic<bool> analyzerEnabled { true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessor)
};
