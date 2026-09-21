#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

class VVChainAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VVChainAudioProcessorEditor(VVChainAudioProcessor&);
    ~VVChainAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoolAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    class MetalLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                              float sliderPosProportional, float rotaryStartAngle,
                              float rotaryEndAngle, juce::Slider&) override;
        void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    };

    class Page final : public juce::Component
    {
    public:
        explicit Page(VVChainAudioProcessorEditor& ownerRef) : owner(ownerRef) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
        void mouseDrag(const juce::MouseEvent&) override;
        void mouseUp(const juce::MouseEvent&) override;

    private:
        VVChainAudioProcessorEditor& owner;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Page)
    };

    struct Knob
    {
        int group = 0;
        int slot = 0;
        juce::Colour accent;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<Attachment> attachment;
    };

    void addKnob(const juce::String& id, const juce::String& title,
                 double min, double max, double step, double defaultValue,
                 const juce::String& suffix, int group, int slot, juce::Colour accent);

    void addBypass(int moduleIndex, const juce::String& parameterId, const juce::String& title);
    void addSectionLabel(const juce::String& text, int group);

    float parameterValue(const juce::String& id) const;
    void setParameter(const juce::String& id, float value);

    juce::Rectangle<float> graphBounds() const;
    float graphFrequencyToX(const juce::Rectangle<float>&, float hz) const;
    float graphXToFrequency(const juce::Rectangle<float>&, float x) const;
    float eqDbToY(const juce::Rectangle<float>&, float db) const;

    void paintPage(juce::Graphics&, juce::Rectangle<float>);
    void drawEqGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawOttOverview(juce::Graphics&, juce::Rectangle<float>);
    void drawTypeOverview(juce::Graphics&, juce::Rectangle<float>);
    void drawDeEsserOverview(juce::Graphics&, juce::Rectangle<float>);
    void drawMixOverview(juce::Graphics&, juce::Rectangle<float>);

    void pageMouseDown(const juce::MouseEvent&);
    void pageMouseDrag(const juce::MouseEvent&);
    void pageMouseUp();

    void placeGroup(int group, juce::Rectangle<int> area, int columns, int knobWidth = 86,
                    int knobHeight = 96, int gapX = 10, int gapY = 8);

    VVChainAudioProcessor& audioProcessor;
    MetalLookAndFeel metalLook;
    juce::Viewport viewport;
    Page page;

    std::vector<Knob> knobs;
    std::array<std::unique_ptr<juce::ToggleButton>, 5> bypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 5> bypassAttachments;
    std::unique_ptr<juce::ToggleButton> ottClipper;
    std::unique_ptr<BoolAttachment> ottClipperAttachment;

    juce::ComboBox deEssVoice;
    std::unique_ptr<ComboAttachment> deEssVoiceAttachment;

    int selectedBand = 0;
    int dragBand = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};
