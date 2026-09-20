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
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    enum DragTarget
    {
        DragNone = 0,
        EqBand1, EqBand2, EqBand3, EqBand4,
        OttDegree1, OttDegree2, OttDegree3, OttDegree4,
        OttXover1, OttXover2, OttXover3,
        TypeDegree1, TypeDegree2, TypeDegree3, TypeDegree4,
        DeessLow, DeessHigh, DeessStrength, DeessRange,
        MixDryWet, MixOutput
    };

    void timerCallback() override;
    void selectModule(int index);
    void selectBand(int index);
    void rebuildControls();

    float graphFrequencyToX(const juce::Rectangle<float>&, float hz) const;
    float graphXToFrequency(const juce::Rectangle<float>&, float x) const;
    float graphPercentToY(const juce::Rectangle<float>&, float percent) const;
    float graphYToPercent(const juce::Rectangle<float>&, float y) const;

    VVChainAudioProcessor& audioProcessor;
    int moduleIndex = 0;
    int bandIndex = 0;
    bool detailOpen = true;
    int dragTarget = DragNone;

    std::array<juce::TextButton, 6> moduleButtons;
    std::array<juce::TextButton, 4> bandButtons;
    std::array<juce::Slider, 20> sliders;
    std::array<juce::Label, 20> labels;
    std::array<std::unique_ptr<Attachment>, 20> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};
