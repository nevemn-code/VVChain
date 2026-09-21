#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include <tuple>
#include "PluginProcessor.h"

class VVChainAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                  private juce::Timer
{
public:
    explicit VVChainAudioProcessorEditor(VVChainAudioProcessor&);
    ~VVChainAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void timerCallback() override;

private:
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BoolAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class WheelSlider final : public juce::Slider
    {
    public:
        void mouseWheelMove(const juce::MouseEvent& e,
                            const juce::MouseWheelDetails& wheel) override
        {
            juce::ignoreUnused(e);
            if (!isEnabled() || !isScrollWheelEnabled() || std::abs(wheel.deltaY) < 0.0001f)
                return;

            const double lo = getMinimum();
            const double hi = getMaximum();
            if (!(hi > lo))
                return;

            // Common studio-knob feel: about 0.5% of the normalized range per
            // mouse-wheel notch, with no acceleration. This stays usable on
            // wide ranges such as frequency and level without jumping too far.
            const double proportion = getNormalisableRange().convertTo0to1(getValue());
            const double next = juce::jlimit(
                0.0, 1.0,
                proportion + static_cast<double>(wheel.deltaY) * 0.005);

            setValue(getNormalisableRange().convertFrom0to1(next),
                     juce::sendNotificationSync);
        }
    };

    class MetalLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        bool monochrome = false;

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
                 juce::Colour accent, bool tapeDisplayDb = false,
                 const juce::String& attachmentId = {});

    void addBypass(int index, const juce::String& parameterId,
                   const juce::String& tooltip, juce::Colour accent);

    Knob* findKnob(const juce::String& id);
    void placeKnob(const juce::String& id, juce::Rectangle<int> area);
    void setExpandedBand(int band);

    float parameterValue(const juce::String& id) const;
    void setParameter(const juce::String& id, float value);

    juce::Rectangle<float> eqGraphBounds() const;
    float graphFrequencyToX(const juce::Rectangle<float>&, float hz) const;
    float constrainXoverFrequency(int index, float hz) const;
    float graphXToFrequency(const juce::Rectangle<float>&, float x) const;
    float eqDbToY(const juce::Rectangle<float>&, float db) const;
    void drawEqGraph(juce::Graphics&, juce::Rectangle<float>);
    void drawCard(juce::Graphics&, juce::Rectangle<float>, juce::Colour,
                  const juce::String&, const juce::String&);
    void drawPanel(juce::Graphics&, juce::Rectangle<float>, const juce::String&,
                   const juce::String&, juce::Colour);
    void drawModuleLeds(juce::Graphics&);
    juce::Colour uiColour(juce::Colour) const noexcept;
    void updateBypassVisuals();
    bool isMasterBypassed() const noexcept;

    VVChainAudioProcessor& audioProcessor;
    MetalLookAndFeel metalLook;

    std::vector<Knob> knobs;
    std::array<std::unique_ptr<juce::ToggleButton>, 5> bypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 5> bypassAttachments;
    std::unique_ptr<juce::ToggleButton> masterBypassButton;
    std::unique_ptr<BoolAttachment> masterBypassAttachment;
    std::unique_ptr<juce::ToggleButton> deessBypassButton;
    std::unique_ptr<BoolAttachment> deessBypassAttachment;
    std::array<std::unique_ptr<juce::TextButton>, 4> advancedButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> ottBandBypassButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> analogModeButtons;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> analogBypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> analogBypassAttachments;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> soloButtons;
    std::unique_ptr<juce::ToggleButton> soloModeButton;
    std::array<std::unique_ptr<juce::ToggleButton>, 4> atypeBandBypassButtons;
    std::array<std::unique_ptr<BoolAttachment>, 4> ottBandBypassAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> analogModeAttachments;
    std::array<std::unique_ptr<BoolAttachment>, 4> atypeBandBypassAttachments;

    std::unique_ptr<juce::ToggleButton> ottClipper;
    std::unique_ptr<BoolAttachment> ottClipperAttachment;
    std::unique_ptr<juce::TextButton> closeAdvanced;

    int expandedBand = -1;
    int dragBand = -1;
    int dragXover = -1;
    bool lastMasterBypassUi = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VVChainAudioProcessorEditor)
};