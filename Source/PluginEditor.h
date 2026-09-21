#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include <tuple>
#include "PluginProcessor.h"

class VVChainAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VVChainAudioProcessorEditor(VVChainAudioProcessor&);
    ~VVChainAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoolAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class MetalLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    };

    struct Knob
    {
        juce::String id;
        juce::String title;
        juce::String suffix;
        juce::Colour accent;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<Attachment> attachment;
        int band = -1;
        int slot = -1;
        bool tapeDisplayDb = false;
    };

    void addKnob(const juce::String& id, const juce::String& title,
                 double min, double max, double step, double defaultValue,
                 const juce::String& suffix, int band, int slot,
                 juce::Colour accent, bool tapeDisplayDb = false);

    void addBypass(int index, const juce::String& parameterId,
                   const juce::String& tooltip, juce::Colour accent);

    Knob* findKnob(const juce::String& id);
    void placeKnob(const juce::String& id, juce::Rectangle<int> area);
    void setExpandedBand(int band);

    float parameterValue(const juce::String& id) const;
    void setParameter(const juce::String& id, float value);

    juce::Rectangle<float> eqGraphBounds() const;
    float graphFrequencyToX(const juce::Rectangle<float>&, float hz) const;
    float graphXToFrequency(const juce::Rectangle<float>&, float x) const;
    float eqDbToY(const juce::Rectangle<float>&, float db) const;
    void drawEqGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawCard(juce::Graphics&, juce::Rectangle<float>, juce::Colour,
                  const juce::String&, const juce::String&);
    void drawPanel(juce::Graphics&, juce::Rectangle<float>, const juce::String&,
                   const juce::String&, juce::Colour);
    void drawModuleLeds(juce::Graphics&);

    VVChainAudioProcessor& audioProcessor;
    MetalLookAndFeel metalLook;

    std::vector<Knob> knobs;
    std::array<std::unique_ptr<juce::ToggleButton>, 5> bypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 5> bypassAttachments;
    std::array<std::unique_ptr<juce::TextButton>, 4> advancedButtons;

    std::unique_ptr<juce::ToggleButton> ottClipper;
    std::unique_ptr<BoolAttachment> ottClipperAttachment;
    std::unique_ptr<juce::TextButton> closeAdvanced;

    int expandedBand = -1;
    int dragBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};