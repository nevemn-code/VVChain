#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include "PluginProcessor.h"

class VVChainAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VVChainAudioProcessorEditor(VVChainAudioProcessor&);
    ~VVChainAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoolAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    enum class DragTarget
    {
        None,
        EqBand1, EqBand2, EqBand3, EqBand4,
        OttDegree1, OttDegree2, OttDegree3, OttDegree4,
        OttX1, OttX2, OttX3,
        TypeDegree1, TypeDegree2, TypeDegree3, TypeDegree4,
        MixDryWet, MixOutput
    };

    void selectModule(int index);
    void selectBand(int index);
    void rebuildControls();

    void hideControl(int index);
    void setControl(int index, const juce::String& parameterId, const juce::String& title);
    void setControlRangeForDisplay(int index, double minimum, double maximum,
                                   double step, const juce::String& suffix);

    void drawTopBar(juce::Graphics&, juce::Rectangle<float>);
    void drawGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawParameterPanel(juce::Graphics&, juce::Rectangle<float>);

    void drawGrid(juce::Graphics&, juce::Rectangle<float>, float minDb, float maxDb);
    void drawEqGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawOttGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawTypeAGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawDeEsserGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawMixGraph(juce::Graphics&, juce::Rectangle<float>);

    float graphFrequencyToX(const juce::Rectangle<float>&, float hz) const;
    float graphXToFrequency(const juce::Rectangle<float>&, float x) const;
    float graphPercentToY(const juce::Rectangle<float>&, float percent) const;
    float graphYToPercent(const juce::Rectangle<float>&, float y) const;

    float parameterValue(const juce::String& id) const;
    void setParameter(const juce::String& id, float value);

    VVChainAudioProcessor& audioProcessor;
    int moduleIndex = 0;
    int bandIndex = 0;
    DragTarget dragTarget = DragTarget::None;

    std::array<juce::TextButton, 5> moduleButtons;
    std::array<juce::ToggleButton, 5> bypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 5> bypassAttachments;

    std::array<juce::TextButton, 4> bandButtons;
    std::array<juce::Slider, 18> controls;
    std::array<juce::Label, 18> controlLabels;
    std::array<std::unique_ptr<Attachment>, 18> attachments;

    juce::ToggleButton ottClipper { "CLIP" };
    juce::ComboBox deEssVoice;
    std::unique_ptr<ComboAttachment> deEssVoiceAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};
