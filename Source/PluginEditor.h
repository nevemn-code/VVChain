#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class VVChainAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit VVChainAudioProcessorEditor(VVChainAudioProcessor&);
    ~VVChainAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void selectModule(int index);
    void selectBand(int index);
    void rebuildControls();

    VVChainAudioProcessor& audioProcessor;
    int moduleIndex = 0;
    int bandIndex = 0;
    bool detailOpen = false;

    std::array<juce::TextButton, 6> moduleButtons;
    std::array<juce::TextButton, 4> bandButtons;
    std::array<juce::Slider, 16> sliders;
    std::array<juce::Label, 16> labels;
    std::array<std::unique_ptr<Attachment>, 16> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};
